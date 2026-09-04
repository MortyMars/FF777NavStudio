#include <QAction>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPair>
#include <QPixmap>
#include <QPushButton>
#include <QSize>
#include <QSizePolicy>
#include <QSortFilterProxyModel>
#include <QSplitter>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStyle>
#include <QTableView>
#include <QTabWidget>
#include <QTextBrowser>
#include <QTextStream>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>
#include <QWidgetAction>

#include <functional>

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "AirportEditorWidget.h"
#include "AirportExtractDialog.h"
#include "ApproachEditorWidget.h"
#include "ApproachTransitionEditorWidget.h"
#include "GenericTableModel.h"
#include "LegEditorWidget.h"
#include "LegSequenceEditorWidget.h"
#include "NavDataWriter.h"
#include "NavaidEditorWidget.h"
#include "PointEditorWidget.h"
#include "ProcedureEditorWidget.h"
#include "ProcedureTransitionEditorWidget.h"
#include "RunwayEditorWidget.h"
#include "RunwayProcedureTransitionEditorWidget.h"
#include "TableColumnHelpers.h"
#include "UnitConverterWidget.h"
#include "WaypointEditorWidget.h"
#include "WorldIndexReader.h"
#include "Nav1DbPipeline.h"

// -----------------------------------------------------------------------------
// Fractionneur à 50/50 : redécoupe les deux volets à largeur égale à chaque
// redimensionnement (et au changement d'onglet, puisqu'un onglet caché est
// redimensionné quand il redevient visible). Garantit que la zone « base de
// données » et la zone « saisie » se partagent toujours l'espace à part égale,
// quelle que soit la taille du contenu de chaque volet.
class EvenSplitter : public QSplitter
{
public:
    using QSplitter::QSplitter;

protected:
    void resizeEvent(QResizeEvent* event) override
    {
        QSplitter::resizeEvent(event);
        if (count() < 2 || width() <= 0)
            return;
        const int half = width() / 2;
        if (sizes().at(0) != half || sizes().at(1) != width() - half)
            setSizes({half, width() - half});
    }
};

using namespace navstud::model;
using namespace navstud::userdata;
using namespace navstud::persistence;
using namespace navstud::worldindex;
using namespace navstud::conversion;
using namespace navstud::validator;
using namespace navstud::ui;
using namespace navstud::writer;

// ----------------------------------------------------------------------------------------------------------
// CONSTRUCTEUR DE LA CLASSE 'MAINWINDOW'
// Construit la fenêtre principale : ouvre la base SQLite, crée le menu,
// la barre d'outils et les onglets d'édition.
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // ------------------------------------------------------------------------------------------------------
    // Base SQLite
    const QString appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(appDataDir);
    const QString sqlitePath = appDataDir + QStringLiteral("/projects.sqlite");

    QString storeError;
    if (!mStore.open(sqlitePath, &storeError)) {
        QMessageBox::critical(
            this,
            QStringLiteral("Base de données"),
            QStringLiteral("Impossible d'ouvrir %1 : %2").arg(sqlitePath, storeError)
        );
    }

    // ------------------------------------------------------------------------------------------------------
    // Menu principal + barre d'outils.
    // Chaque bloc est ouvert par un simple « titre de bloc » (action
    // désactivée, mise en gras) : l'ensemble des actions reste visible d'un
    // seul coup d'œil, sans sous-menu héarchique.
    auto* mainMenu = menuBar()->addMenu(QStringLiteral("Fichier"));

    // ===================================================================
    // ADDBLOCKTITLE
    // Méthode créant un titre de bloc non cliquable servant d'entête
    auto addBlockTitle = [mainMenu](const QString& title) {

        auto* headerLabel = new QLabel(title);

        headerLabel->setContentsMargins(2, 0, 0, 0);
        auto* header = new QWidgetAction(mainMenu);
        header->setDefaultWidget(headerLabel);
        mainMenu->addAction(header);

        header->setEnabled(true);   // 'true' pour une police visible

        return header;

    }; // !addBlockTitle =================================================


    // BLOC DE MENUS 'PROJETS' ------------------------------------------------------------------------------
    addBlockTitle(QStringLiteral("  PROJETS :"));

    QAction* newProjectAction  = mainMenu->addAction(QStringLiteral(" Nouveau projet"));
    QAction* openProjectAction = mainMenu->addAction(QStringLiteral(" Ouvrir un projet"));
    QAction* extractAirportAction = mainMenu->addAction(QStringLiteral(" Aéroport existant -> Projet"));

    extractAirportAction->setToolTip(
        QStringLiteral("Lit un fichier mondial nav1.txt, extrait toutes les données rattachées à un "
                       "aéroport et crée (ou remplace) un projet SQLite dédié avec elles"));
    mainMenu->addSeparator();


    // BLOC DE MENUS 'BASE DE DONNÉES' ----------------------------------------------------------------------
    addBlockTitle(QStringLiteral("  BASE de DONNÉES :"));

    mSaveAction = mainMenu->addAction(QStringLiteral("Enregistrer"));
    mSaveAction->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
    mSaveAction->setShortcut(QKeySequence::Save);
    mSaveAction->setToolTip(QStringLiteral("Enregistrer le projet (Ctrl+S)"));
    mSaveAction->setEnabled(false);

    mReloadWorldAction = mainMenu->addAction(QStringLiteral(" Recharger le fichier mondial 'nav1.txt'"));
    mReloadWorldAction->setToolTip(
        QStringLiteral("Réaligne les id du projet sur un fichier mondial mis à jour (nouvelles \"# Count:\")"));
    mReloadWorldAction->setEnabled(false);

    mExportAction = mainMenu->addAction(QStringLiteral(" Exporter les fichiers .txt"));
    mExportAction->setToolTip(QStringLiteral("Écrit les 15 fichiers _Xxx.txt (Point, NAV, LEG, ...) "
                                             "dans le dossier de l'application"));
    mExportAction->setEnabled(false);
    mainMenu->addSeparator();


    // BLOC DE MENUS 'SUITE À MÀJ AIRACS' -------------------------------------------------------------------
    // Pipeline nav1.db : décodage / intégration / réencodage. Les opérations
    // se déroulent dans le dossier de l'application (celui de l'exécutable).
    addBlockTitle(QStringLiteral("  SUITE à MàJ des AIRACS :"));

    auto* decodeWorldAction = mainMenu->addAction(QStringLiteral(" Décoder 'nav1.db' -> 'nav1.txt'"));
    mDecodeWorldAction = decodeWorldAction;

    decodeWorldAction->setToolTip(
        QStringLiteral("Décode nav1.db en nav1.txt (dossier de l'appli), "
                       "puis recharge ce fichier pour aligner les id du projet"));

    decodeWorldAction->setEnabled(false);

    auto* integrateWorldAction = mainMenu->addAction(QStringLiteral(" Compléter 'nav1.txt' et réencoder 'nav1.db'"));
    mIntegrateWorldAction = integrateWorldAction;

    integrateWorldAction->setToolTip(
        QStringLiteral("Intègre les 15 fichiers _Xxx.txt du projet dans le nav1.txt, "
                       "réencode en nav1.db et copie le tout à la destination X-Plane"));

    integrateWorldAction->setEnabled(false);


    /* ------------------------------------------------------------------------------------------------------
    MENU ONE-SHOT DÉSACTIVÉ (la méthode associée est toujours présente et active dans le code)
    Ce menu avait été créé pour peupler initialement la BDD avec les données de LFFA sans avoir à tout
    ressaisir. Un jeu de fichier 'texte' spécifique (cf. dossier '/z_peuplBddOneShot'), au format spécifique
    sans rapport avec les fichiers 'texte' produits par l'appli, a été créé pour l'occasion.

    Menu :
    QAction* importAction = fileMenu->addAction(QStringLiteral("Importer depuis fichiers texte (one-shot)..."));
    importAction->setToolTip(
        QStringLiteral("Crée un projet à partir de 15 fichiers .txt pré-extraits (points.txt, legs.txt, ...)"));

    Connexion :
    connect(importAction, &QAction::triggered, this, &MainWindow::onImportFromTextFiles);
    ------------------------------------------------------------------------------------------------------ */


    auto* toolBar = addToolBar(QStringLiteral("Principal"));
    toolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    toolBar->setIconSize(QSize(24, 24));
    toolBar->addAction(mSaveAction);

    // Pousse les convertisseurs tout à droite du bandeau.
    auto* toolBarSpacer = new QWidget(this);
    toolBarSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolBar->addWidget(toolBarSpacer);

    // Regroupés dans UN SEUL widget conteneur plutôt qu'ajoutés séparément
    // au bandeau : QToolBar::addWidget traite chaque widget comme
    // potentiellement extensible, ce qui écartait les deux convertisseurs
    // l'un de l'autre au lieu de les garder collés.
    auto* convertersContainer = new QWidget(this);
    auto* convertersLayout = new QHBoxLayout(convertersContainer);

    convertersLayout->setContentsMargins(0, 0, 0, 0);
    convertersLayout->setSpacing(6);

    convertersLayout->addWidget(
            new UnitConverterWidget(
                    QStringLiteral("ft"),
                    QStringLiteral("m"),
                    1.0 / 3.28084,
                    convertersContainer
            )
    );

    auto* convertersSeparator = new QFrame(convertersContainer);

    convertersSeparator->setFrameShape(QFrame::VLine);
    convertersSeparator->setFrameShadow(QFrame::Sunken);
    convertersLayout->addWidget(convertersSeparator);

    convertersLayout->addWidget(
        new UnitConverterWidget(
            QStringLiteral("NM"),
            QStringLiteral("m"),
            1852.0,
            convertersContainer
        )
    );

    convertersContainer->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);

    toolBar->addWidget(convertersContainer);


    // Connexion
    connect(newProjectAction,       &QAction::triggered, this, &MainWindow::onNewProject);
    connect(openProjectAction,      &QAction::triggered, this, &MainWindow::onOpenProject);
    connect(mSaveAction,            &QAction::triggered, this, &MainWindow::onSaveProject);
    connect(mReloadWorldAction,     &QAction::triggered, this, &MainWindow::onReloadWorldFile);
    connect(mExportAction,          &QAction::triggered, this, &MainWindow::onExportFiles);
    connect(decodeWorldAction,      &QAction::triggered, this, &MainWindow::onDecodeWorldFile);
    connect(integrateWorldAction,   &QAction::triggered, this, &MainWindow::onIntegrateWorldFile);
    connect(extractAirportAction,   &QAction::triggered, this, &MainWindow::onExtractAirport);

    // ------------------------------------------------------------------------------------------------------
    // MENU AIDE
    auto* helpMenu = menuBar()->addMenu(QStringLiteral("Aide"));

    // À propos
    QAction* aboutAction = helpMenu->addAction(QStringLiteral("À propos ..."));
    connect(aboutAction,            &QAction::triggered, this, &MainWindow::onAbout);

    // Objectifs
    QAction* documentationAction = helpMenu->addAction(QStringLiteral("Objectifs ..."));
    connect(documentationAction,    &QAction::triggered, this, &MainWindow::onDocumentation);





    // ------------------------------------------------------------------------------------------------------
    // Onglets — un par structure. Point n'a pas besoin de régénération
    // (aucune résolution d'ident), les 4 autres si.
    auto* tabs = new QTabWidget(this);

    // POINT ---
    mPointModel = new GenericTableModel(
        orderFnFor(&mProject.points()),
        {
            idColumn(),
            textColumn(QStringLiteral("Ident"), &mProject.points(), &UserPoint::ident),
            doubleColumn(QStringLiteral("Latitude"), &mProject.points(), &UserPoint::latitude, 12),
            doubleColumn(QStringLiteral("Longitude"), &mProject.points(), &UserPoint::longitude, 12),
            doubleColumn(QStringLiteral("Var. magn."), &mProject.points(), &UserPoint::magVar, 6),
            doubleColumn(QStringLiteral("Cap hold"), &mProject.points(), &UserPoint::holdCourse, 6),
            doubleColumn(QStringLiteral("Dist hold (m)"), &mProject.points(), &UserPoint::holdDistInMeters, 3),
            doubleColumn(QStringLiteral("Temps hold"), &mProject.points(), &UserPoint::holdTime, 3),
            intColumn(QStringLiteral("Sens hold"), &mProject.points(), &UserPoint::holdSide),
        },
        this
    );

    mPointTable = new QTableView(this);
    mPointProxy = new QSortFilterProxyModel(this);
    mPointProxy->setSourceModel(mPointModel);
    mPointTable->setModel(mPointProxy);
    mPointTable->setSortingEnabled(true);
    mPointTable->sortByColumn(1, Qt::AscendingOrder);
    mPointTable->horizontalHeader()->setStretchLastSection(true);
    mPointTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    mPointTable->setSelectionMode(QAbstractItemView::SingleSelection);
    mPointEditor = new PointEditorWidget(this);
    mPointEditor->setEnabled(false);

    auto* newPointButton = new QPushButton(QStringLiteral("Nouveau point"), this);
    auto* deletePointButton = new QPushButton(QStringLiteral("Supprimer"), this);

    tabs->addTab(
        buildTabLayout(
            newPointButton,
            deletePointButton,
            mPointTable,
            mPointEditor
        ),
        QStringLiteral("Point")
    );

    connect(
        mPointTable->selectionModel(),
        &QItemSelectionModel::currentRowChanged,
        this, &MainWindow::
        onPointSelectionChanged
    );

    connect(
        newPointButton,
        &QPushButton::clicked,
        this,
        &MainWindow::onNewPoint
    );

    connect(
        deletePointButton,
        &QPushButton::clicked,
        this,
        [this]() {
            deleteCurrentRow(
                mPointTable,
                mPointModel,
                mPointProxy,
                mPointEditor,
                [this](qint32 rawId) {
                    return mProject.points().remove(PointId(rawId));
                },
                [this]() {
                    mCurrentPointId = PointId::invalid();
                },
                [this]() {
                        auto& table = mProject.points();
                        if (!table.order().isEmpty())
                                 table.renumberFrom(table.order().first().value());
                }
            );
        }
    );

    connect(mPointEditor, &PointEditorWidget::valueEdited, this, &MainWindow::onPointEdited);



    // WAYPOINT ---
    mWaypointModel = new GenericTableModel(
        orderFnFor(&mProject.waypoints()),
        {
            idColumn(),
            textColumn(
                QStringLiteral("Ident Point"),
                &mProject.waypoints(),
                &UserWaypoint::pointIdent
            ),
        },
        this
    );

    mWaypointTable = new QTableView(this);
    mWaypointProxy = new QSortFilterProxyModel(this);
    mWaypointProxy->setSourceModel(mWaypointModel);
    mWaypointTable->setModel(mWaypointProxy);
    mWaypointTable->setSortingEnabled(true);
    mWaypointTable->sortByColumn(1, Qt::AscendingOrder);
    mWaypointTable->horizontalHeader()->setStretchLastSection(true);
    mWaypointTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    mWaypointTable->setSelectionMode(QAbstractItemView::SingleSelection);
    mWaypointEditor = new WaypointEditorWidget(this);
    mWaypointEditor->setEnabled(false);

    auto* newWaypointButton = new QPushButton(QStringLiteral("Nouveau waypoint"), this);
    auto* deleteWaypointButton = new QPushButton(QStringLiteral("Supprimer"), this);

    tabs->addTab(
        buildTabLayout(
            newWaypointButton,
            deleteWaypointButton,
            mWaypointTable,
            mWaypointEditor
        ),
        QStringLiteral("Waypoint")
    );

    connect(mWaypointTable->selectionModel(),
            &QItemSelectionModel::currentRowChanged,
            this,
            &MainWindow::onWaypointSelectionChanged
    );

    connect(
        newWaypointButton,
        &QPushButton::clicked,
        this,
        &MainWindow::onNewWaypoint
    );

    connect(
        deleteWaypointButton,
        &QPushButton::clicked,
        this,
        [this]() {
            deleteCurrentRow(
                mWaypointTable, mWaypointModel, mWaypointProxy, mWaypointEditor,
                        [this](qint32 rawId) {
                            return mProject.waypoints().remove(WaypointId(rawId));
                        },
                        [this]() {
                            mCurrentWaypointId = WaypointId::invalid();
                        },
                        [this]() {
                            auto& table = mProject.waypoints();
                            if (!table.order().isEmpty())
                                 table.renumberFrom(table.order().first().value());
                        }
            );
        }
    );

    connect(
        mWaypointEditor,
        &WaypointEditorWidget::valueEdited,
        this,
        &MainWindow::onWaypointEdited
    );

    // --- Airport ---
    mAirportModel = new GenericTableModel(
        orderFnFor(&mProject.airports()),
        {
            idColumn(),
            textColumn(
                QStringLiteral("Ident Point"),
                &mProject.airports(),
                &UserAirport::pointIdent
            ),
            doubleColumn(
                QStringLiteral("Élévation (m)"),
                &mProject.airports(),
                &UserAirport::elevationInMeters,
                6
            ),
            doubleColumn(
                QStringLiteral("Lim. vitesse"),
                &mProject.airports(),
                &UserAirport::limitSpeedInMetersPerSec,
                6
            ),
            doubleColumn(
                QStringLiteral("Lim. altitude"),
                &mProject.airports(),
                &UserAirport::limitAltitudeInMeters,
                6
            ),
            doubleColumn(
                QStringLiteral("Alt. transition"),
                &mProject.airports(),
                &UserAirport::transitionAltitudeInMeters,
                6
            ),
            doubleColumn(
                QStringLiteral("Niv. transition"),
                &mProject.airports(),
                &UserAirport::transitionLevelInMeters,
                6
            ),
        },
        this
    );

    mAirportTable = new QTableView(this);
    mAirportProxy = new QSortFilterProxyModel(this);
    mAirportProxy->setSourceModel(mAirportModel);
    mAirportTable->setModel(mAirportProxy);
    mAirportTable->setSortingEnabled(true);
    mAirportTable->sortByColumn(1, Qt::AscendingOrder);
    mAirportTable->horizontalHeader()->setStretchLastSection(true);
    mAirportTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    mAirportTable->setSelectionMode(QAbstractItemView::SingleSelection);
    mAirportEditor = new AirportEditorWidget(this);
    mAirportEditor->setEnabled(false);

    auto* newAirportButton = new QPushButton(QStringLiteral("Nouvel airport"), this);
    auto* deleteAirportButton = new QPushButton(QStringLiteral("Supprimer"), this);

    tabs->addTab(
        buildTabLayout(
            newAirportButton,
            deleteAirportButton,
            mAirportTable,
            mAirportEditor
        ),
        QStringLiteral("Airport")
    );

    connect(
        mAirportTable->selectionModel(),
        &QItemSelectionModel::currentRowChanged,
        this,
        &MainWindow::onAirportSelectionChanged
    );

    connect(
        newAirportButton,
        &QPushButton::clicked,
        this,
        &MainWindow::onNewAirport
    );

    connect(
        deleteAirportButton,
        &QPushButton::clicked,
        this,
        [this]() {
            deleteCurrentRow(
                mAirportTable,
                mAirportModel,
                mAirportProxy,
                mAirportEditor,
                [this](qint32 rawId) {
                    return mProject.airports().remove(AirportId(rawId));
                },
                [this]() {
                    mCurrentAirportId = AirportId::invalid();
                },
                [this]() {
                    auto& table = mProject.airports();
                    if (!table.order().isEmpty())
                            table.renumberFrom(table.order().first().value());
                }
            );
        }
    );

    connect(mAirportEditor, &AirportEditorWidget::valueEdited, this, &MainWindow::onAirportEdited);

    // RUNWAY ---
    mRunwayModel = new GenericTableModel(
        orderFnFor(&mProject.runways()),
        {
            idColumn(),
            textColumn(
                QStringLiteral("Ident Airport"),
                &mProject.runways(),
                &UserRunway::airportIdent
            ),
            textColumn(
                QStringLiteral("Ident Threshold"),
                &mProject.runways(),
                &UserRunway::thresholdIdent
            ),
            doubleColumn(
                QStringLiteral("Élévation (m)"),
                &mProject.runways(),
                &UserRunway::elevationInMeters,
                6
            ),
            doubleColumn(
                QStringLiteral("Gradient"),
                &mProject.runways(),
                &UserRunway::gradient,
                6
            ),
            doubleColumn(
                QStringLiteral("Cap"),
                &mProject.runways(),
                &UserRunway::course,
                6
            ),
            doubleColumn(
                QStringLiteral("Longueur (m)"),
                &mProject.runways(),
                &UserRunway::lengthInMeters,
                6
            ),
            doubleColumn(
                QStringLiteral("Décalage seuil"),
                &mProject.runways(),
                &UserRunway::displacedInMeters,
                6
            ),
            doubleColumn(
                QStringLiteral("Stopway"),
                &mProject.runways(),
                &UserRunway::stopwayInMeters,
                6
            ),
            doubleColumn(
                QStringLiteral("Survol seuil"),
                &mProject.runways(),
                &UserRunway::crossInMeters,
                6
            ),
        },
        this
    );

    mRunwayTable = new QTableView(this);
    mRunwayProxy = new QSortFilterProxyModel(this);
    mRunwayProxy->setSourceModel(mRunwayModel);
    mRunwayTable->setModel(mRunwayProxy);
    mRunwayTable->setSortingEnabled(true);
    mRunwayTable->sortByColumn(1, Qt::AscendingOrder);
    mRunwayTable->horizontalHeader()->setStretchLastSection(true);
    mRunwayTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    mRunwayTable->setSelectionMode(QAbstractItemView::SingleSelection);
    mRunwayEditor = new RunwayEditorWidget(this);
    mRunwayEditor->setEnabled(false);

    auto* newRunwayButton = new QPushButton(QStringLiteral("Nouvelle piste"), this);
    auto* deleteRunwayButton = new QPushButton(QStringLiteral("Supprimer"), this);

    tabs->addTab(
        buildTabLayout(
            newRunwayButton,
            deleteRunwayButton,
            mRunwayTable,
            mRunwayEditor
        ),
        QStringLiteral("Runway")
    );

    connect(
        mRunwayTable->selectionModel(),
        &QItemSelectionModel::currentRowChanged,
        this,
        &MainWindow::onRunwaySelectionChanged
    );
    connect(
        newRunwayButton,
        &QPushButton::clicked,
        this,
        &MainWindow::onNewRunway
    );

    connect(
        deleteRunwayButton,
        &QPushButton::clicked,
        this,
        [this]() {
            deleteCurrentRow(
                mRunwayTable,
                mRunwayModel,
                mRunwayProxy,
                mRunwayEditor,
                [this](qint32 rawId) { return mProject.runways().remove(RunwayId(rawId)); },
                [this]() { mCurrentRunwayId = RunwayId::invalid(); },
                [this]() {
                            auto& table = mProject.runways();
                            if (!table.order().isEmpty())
                                 table.renumberFrom(table.order().first().value());
                }
            );
        }
    );

    connect(mRunwayEditor, &RunwayEditorWidget::valueEdited, this, &MainWindow::onRunwayEdited);


    // NAVAID ---
    mNavaidModel = new GenericTableModel(
        orderFnFor(&mProject.navaids()),
        {
            idColumn(),
            textColumn(
                QStringLiteral("Type"),
                &mProject.navaids(),
                &UserNavaid::type
            ),
            textColumn(
                QStringLiteral("Ident Point"),
                &mProject.navaids(),
                &UserNavaid::pointIdent
            ),
            textColumn(
                QStringLiteral("Navaid associé"),
                &mProject.navaids(),
                &UserNavaid::associatedNavaidIdent
            ),
            doubleColumn(
                QStringLiteral("Élévation (m)"),
                &mProject.navaids(),
                &UserNavaid::elevationInMeters,
                6
            ),
            doubleColumn(
                QStringLiteral("Déc. magn."),
                &mProject.navaids(),
                &UserNavaid::declination,
                6
            ),
            uintColumn(
                QStringLiteral("Fréquence"),
                &mProject.navaids(),
                &UserNavaid::frequencyMHzTimes100
            ),
            textColumn(
                QStringLiteral("Cat"),
                &mProject.navaids(),
                &UserNavaid::category
            ),
            doubleColumn(
                QStringLiteral("Cap"),
                &mProject.navaids(),
                &UserNavaid::course,
                6
            ),
            doubleColumn(
                QStringLiteral("Angle"),
                &mProject.navaids(),
                &UserNavaid::angle,
                6
            ),
            textColumn(
                QStringLiteral("Ident Runway"),
                &mProject.navaids(),
                &UserNavaid::runwayIdent
            ),
        },
        this
    );

    mNavaidTable = new QTableView(this);

    mNavaidProxy = new QSortFilterProxyModel(this);
    mNavaidProxy->setSourceModel(mNavaidModel);

    mNavaidTable->setModel(mNavaidProxy);
    mNavaidTable->setSortingEnabled(true);
    mNavaidTable->sortByColumn(1, Qt::AscendingOrder);
    mNavaidTable->horizontalHeader()->setStretchLastSection(true);
    mNavaidTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    mNavaidTable->setSelectionMode(QAbstractItemView::SingleSelection);

    mNavaidEditor = new NavaidEditorWidget(this);
    mNavaidEditor->setEnabled(false);

    auto* newNavaidButton = new QPushButton(QStringLiteral("Nouveau navaid"), this);
    auto* deleteNavaidButton = new QPushButton(QStringLiteral("Supprimer"), this);

    tabs->addTab(
        buildTabLayout(
            newNavaidButton,
            deleteNavaidButton,
            mNavaidTable,
            mNavaidEditor
        ),
        QStringLiteral("Navaid")
    );

    connect(mNavaidTable->selectionModel(),
            &QItemSelectionModel::currentRowChanged,
            this,
            &MainWindow::onNavaidSelectionChanged
    );

    connect(
        newNavaidButton,
        &QPushButton::clicked,
        this,
        &MainWindow::onNewNavaid
    );

    connect(
        deleteNavaidButton,
        &QPushButton::clicked,
        this,
        [this]() {
            deleteCurrentRow(
                mNavaidTable,
                mNavaidModel,
                mNavaidProxy,
                mNavaidEditor,
                [this](qint32 rawId) {
                    return mProject.navaids().remove(NavaidId(rawId));
                },
                [this]() {
                    mCurrentNavaidId = NavaidId::invalid();
                },
                [this]() {
                             auto& table = mProject.navaids();
                             if (!table.order().isEmpty())
                                 table.renumberFrom(table.order().first().value());
                }
            );
        }
    );

    connect(
        mNavaidEditor,
        &NavaidEditorWidget::valueEdited,
        this,
        &MainWindow::onNavaidEdited
    );


    // LEGSEQUENCE ---
    mLegSequenceModel = new GenericTableModel(
        orderFnFor(&mProject.legSequences()),
        {
            idColumn(),
            textColumn(
                    QStringLiteral("Ident"),
                    &mProject.legSequences(),
                    &UserLegSequence::ident
            ),
            textColumn(
                    QStringLiteral("ILS/RNAV"),
                    &mProject.legSequences(),
                    &UserLegSequence::ilsOrRnav
            ),
            textColumn(
                    QStringLiteral("Type procédure"),
                    &mProject.legSequences(),
                    &UserLegSequence::procedureKind
            ),
            doubleColumn(
                    QStringLiteral("Alt. transition (ft)"),
                    &mProject.legSequences(),
                    &UserLegSequence::altitudeLevelTransInFeet,
                    6
            ),
        },
        this
    );

    mLegSequenceTable = new QTableView(this);
    mLegSequenceProxy = new QSortFilterProxyModel(this);
    mLegSequenceProxy->setSourceModel(mLegSequenceModel);
    mLegSequenceTable->setModel(mLegSequenceProxy);
    mLegSequenceTable->setSortingEnabled(true);
    mLegSequenceTable->sortByColumn(1, Qt::AscendingOrder);
    mLegSequenceTable->horizontalHeader()->setStretchLastSection(true);
    mLegSequenceTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    mLegSequenceTable->setSelectionMode(QAbstractItemView::SingleSelection);
    mLegSequenceEditor = new LegSequenceEditorWidget(this);
    mLegSequenceEditor->setEnabled(false);

    auto* newLegSequenceButton = new QPushButton(QStringLiteral("Nouvelle séquence"), this);
    auto* deleteLegSequenceButton = new QPushButton(QStringLiteral("Supprimer"), this);

    tabs->addTab(
        buildTabLayout(
            newLegSequenceButton,
            deleteLegSequenceButton,
            mLegSequenceTable,
            mLegSequenceEditor
        ),
        QStringLiteral("LegSequence")
    );

    connect(
        mLegSequenceTable->selectionModel(),
        &QItemSelectionModel::currentRowChanged,
        this,
        &MainWindow::onLegSequenceSelectionChanged
    );

    connect(
        newLegSequenceButton,
        &QPushButton::clicked,
        this,
        &MainWindow::onNewLegSequence
    );

    connect(
        deleteLegSequenceButton,
        &QPushButton::clicked,
        this,
        [this]() {
            deleteCurrentRow(
                mLegSequenceTable,
                mLegSequenceModel,
                mLegSequenceProxy,
                mLegSequenceEditor,
                [this](qint32 rawId) { return mProject.legSequences().remove(LegSequenceId(rawId)); },
                [this]() { mCurrentLegSequenceId = LegSequenceId::invalid(); },
                [this]() {
                            auto& table = mProject.legSequences();
                            if (!table.order().isEmpty())
                                 table.renumberFrom(table.order().first().value());
                }
            );
        }
    );

    connect(
        mLegSequenceEditor,
        &LegSequenceEditorWidget::valueEdited,
        this,
        &MainWindow::onLegSequenceEdited
    );


    // LEG ---
    mLegModel = new GenericTableModel(
        orderFnFor(&mProject.legs()),
        {
            idColumn(),
            textColumn(
                QStringLiteral("Code path"),
                &mProject.legs(),
                &UserLeg::codePath
            ),
            textColumn(
                QStringLiteral("Ident séquence"),
                &mProject.legs(),
                &UserLeg::legSequenceIdent
            ),
            textColumn(
                QStringLiteral("Ident point"),
                &mProject.legs(),
                &UserLeg::pointIdent
            ),
            textColumn(
                QStringLiteral("Description WP"),
                &mProject.legs(),
                &UserLeg::wpDescription
            ),
            doubleColumn(
                QStringLiteral("Cap"),
                &mProject.legs(),
                &UserLeg::course,
                6
            ),
            doubleColumn(
                QStringLiteral("Distance (m)"),
                &mProject.legs(),
                &UserLeg::distanceInMeters,
                6
            ),
            textColumn(
                QStringLiteral("Ident navaid"),
                &mProject.legs(),
                &UserLeg::navaidIdent
            ),
            doubleColumn(
                QStringLiteral("Cap navaid"),
                &mProject.legs(),
                &UserLeg::navaidCourse,
                6
            ),
            doubleColumn(
                QStringLiteral("Distance navaid (m)"),
                &mProject.legs(),
                &UserLeg::navaidDistanceInMeters,
                6
            ),
            doubleColumn(
                QStringLiteral("Alt. min (ft)"),
                &mProject.legs(),
                &UserLeg::altitudeLimitMinInFeet,
                6
            ),
            doubleColumn(
                QStringLiteral("Alt. max (ft)"),
                &mProject.legs(),
                &UserLeg::altitudeLimitMaxInFeet,
                6
            ),
            doubleColumn(
                QStringLiteral("Lim. vitesse air"),
                &mProject.legs(),
                &UserLeg::airSpeedLimit,
                6
            ),
            doubleColumn(
                QStringLiteral("Path"),
                &mProject.legs(),
                &UserLeg::path,
                6
            ),
            intColumn(
                QStringLiteral("Sens virage"),
                &mProject.legs(),
                &UserLeg::turnDir
            ),
            doubleColumn(
                QStringLiteral("RNP (m)"),
                &mProject.legs(),
                &UserLeg::rnpInMeters,
                6
            )
        },
        this
    );

    mLegTable = new QTableView(this);

    mLegProxy = new QSortFilterProxyModel(this);
    mLegProxy->setSourceModel(mLegModel);

    mLegTable->setModel(mLegProxy);
    mLegTable->setSortingEnabled(true);
    mLegTable->sortByColumn(1, Qt::AscendingOrder);
    mLegTable->horizontalHeader()->setStretchLastSection(true);
    mLegTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    mLegTable->setSelectionMode(QAbstractItemView::SingleSelection);

    mLegEditor = new LegEditorWidget(this);
    mLegEditor->setEnabled(false);

    auto* newLegButton = new QPushButton(QStringLiteral("Nouveau leg"), this);
    auto* deleteLegButton = new QPushButton(QStringLiteral("Supprimer"), this);

    tabs->addTab(buildTabLayout(newLegButton, deleteLegButton, mLegTable, mLegEditor), QStringLiteral("Leg"));

    connect(
        mLegTable->selectionModel(),
        &QItemSelectionModel::currentRowChanged,
        this,
        &MainWindow::onLegSelectionChanged
    );

    connect(newLegButton,
            &QPushButton::clicked,
            this,
            &MainWindow::onNewLeg
    );

    connect(
        deleteLegButton,
        &QPushButton::clicked,
        this,
        [this]() {
            deleteCurrentRow(
                mLegTable,
                mLegModel,
                mLegProxy,
                mLegEditor,
                [this](qint32 rawId) {
                    return mProject.legs().remove(LegId(rawId));
                },
                [this]() {
                    mCurrentLegId = LegId::invalid();
                },
                [this]() {
                    auto& table = mProject.legs();
                    if (!table.order().isEmpty())
                            table.renumberFrom(table.order().first().value());
                }
            );
        }
    );

    connect(
        mLegEditor,
        &LegEditorWidget::valueEdited,
        this,
        &MainWindow::onLegEdited
    );


    // APPROACH ---
    mApproachModel = new GenericTableModel(
        orderFnFor(&mProject.approaches()),
        {
            idColumn(),
            textColumn(
                QStringLiteral("Ident Runway"),
                &mProject.approaches(),
                &UserApproach::runwayIdent
            ),
            textColumn(
                QStringLiteral("Ident séquence"),
                &mProject.approaches(),
                &UserApproach::legSequenceIdent
            ),
            doubleColumn(
                QStringLiteral("DH (ft)"),
                &mProject.approaches(),
                &UserApproach::decisionHeightInFeet,
                6
            ),
            doubleColumn(
                QStringLiteral("MDA (ft)"),
                &mProject.approaches(),
                &UserApproach::minimumDescentInFeet,
                6
            ),
        },
        this
    );

    mApproachTable = new QTableView(this);
    mApproachProxy = new QSortFilterProxyModel(this);
    mApproachProxy->setSourceModel(mApproachModel);
    mApproachTable->setModel(mApproachProxy);
    mApproachTable->setSortingEnabled(true);
    mApproachTable->sortByColumn(1, Qt::AscendingOrder);
    mApproachTable->horizontalHeader()->setStretchLastSection(true);
    mApproachTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    mApproachTable->setSelectionMode(QAbstractItemView::SingleSelection);
    mApproachEditor = new ApproachEditorWidget(this);
    mApproachEditor->setEnabled(false);

    auto* newApproachButton = new QPushButton(QStringLiteral("Nouvelle approche"), this);
    auto* deleteApproachButton = new QPushButton(QStringLiteral("Supprimer"), this);

    tabs->addTab(
        buildTabLayout(
            newApproachButton,
            deleteApproachButton,
            mApproachTable,
            mApproachEditor
        ),
        QStringLiteral("Approach")
    );

    connect(
        mApproachTable->selectionModel(),
        &QItemSelectionModel::currentRowChanged,
        this,
        &MainWindow::onApproachSelectionChanged
    );

    connect(
        newApproachButton,
        &QPushButton::clicked,
        this,
        &MainWindow::onNewApproach
    );

    connect(
        deleteApproachButton,
        &QPushButton::clicked,
        this,
        [this]() {
            deleteCurrentRow(
                mApproachTable,
                mApproachModel,
                mApproachProxy,
                mApproachEditor,
                [this](qint32 rawId) {
                    return mProject.approaches().remove(ApproachId(rawId));
                },
                [this]() {
                    mCurrentApproachId = ApproachId::invalid();
                },
                [this]() {
                    auto& table = mProject.approaches();
                    if (!table.order().isEmpty())
                            table.renumberFrom(table.order().first().value());
                }
            );
        }
    );

    connect(
        mApproachEditor,
        &ApproachEditorWidget::valueEdited,
        this,
        &MainWindow::onApproachEdited
    );


    // APPROACHTRANSITION ---
    mApproachTransitionModel = new GenericTableModel(
        orderFnFor(&mProject.approachTransitions()),
        {
            idColumn(),
            textColumn(
                QStringLiteral("Ident Approach"),
                &mProject.approachTransitions(),
                &UserApproachTransition::approachIdent
            ),
            textColumn(
                QStringLiteral("Ident séquence"),
                &mProject.approachTransitions(),
                &UserApproachTransition::legSequenceIdent
            ),
        },
        this
    );

    mApproachTransitionTable = new QTableView(this);
    mApproachTransitionProxy = new QSortFilterProxyModel(this);
    mApproachTransitionProxy->setSourceModel(mApproachTransitionModel);
    mApproachTransitionTable->setModel(mApproachTransitionProxy);
    mApproachTransitionTable->setSortingEnabled(true);
    mApproachTransitionTable->sortByColumn(1, Qt::AscendingOrder);
    mApproachTransitionTable->horizontalHeader()->setStretchLastSection(true);
    mApproachTransitionTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    mApproachTransitionTable->setSelectionMode(QAbstractItemView::SingleSelection);
    mApproachTransitionEditor = new ApproachTransitionEditorWidget(this);
    mApproachTransitionEditor->setEnabled(false);

    auto* newApproachTransitionButton = new QPushButton(QStringLiteral("Nouvelle transition"), this);
    auto* deleteApproachTransitionButton = new QPushButton(QStringLiteral("Supprimer"), this);

    tabs->addTab(
        buildTabLayout(
            newApproachTransitionButton,
            deleteApproachTransitionButton,
            mApproachTransitionTable,
            mApproachTransitionEditor
        ),
        QStringLiteral("ApproachTransition")
    );

    connect(
        mApproachTransitionTable->selectionModel(),
        &QItemSelectionModel::currentRowChanged,
        this,
        &MainWindow::onApproachTransitionSelectionChanged
    );

    connect(
        newApproachTransitionButton,
        &QPushButton::clicked,
        this,
        &MainWindow::onNewApproachTransition
    );

    connect(
        deleteApproachTransitionButton,
        &QPushButton::clicked,
        this,
        [this]() {
            deleteCurrentRow(
                mApproachTransitionTable,
                mApproachTransitionModel,
                mApproachTransitionProxy,
                mApproachTransitionEditor,
                [this](qint32 rawId) {
                    return mProject.approachTransitions().remove(ApproachTransitionId(rawId));
                },
                [this]() {
                    mCurrentApproachTransitionId = ApproachTransitionId::invalid();
                },
                [this]() {
                    auto& table = mProject.approachTransitions();
                    if (!table.order().isEmpty())
                            table.renumberFrom(table.order().first().value());
                }
            );
        }
    );

    connect(
        mApproachTransitionEditor,
        &ApproachTransitionEditorWidget::valueEdited,
        this,
        &MainWindow::onApproachTransitionEdited
    );


    // PROCEDURE SID ---
    mSidProcedureModel = new GenericTableModel(
        orderFnFor(&mProject.sidProcedures()),
        {
            idColumn(),
            textColumn(
                QStringLiteral("Ident Airport"),
                &mProject.sidProcedures(),
                &UserProcedure::airportIdent
            ),
            textColumn(
                QStringLiteral("Ident séquence"),
                &mProject.sidProcedures(),
                &UserProcedure::legSequenceIdent
            )
        },
        this
    );

    mSidProcedureTable = new QTableView(this);
    mSidProcedureProxy = new QSortFilterProxyModel(this);
    mSidProcedureProxy->setSourceModel(mSidProcedureModel);
    mSidProcedureTable->setModel(mSidProcedureProxy);
    mSidProcedureTable->setSortingEnabled(true);
    mSidProcedureTable->sortByColumn(1, Qt::AscendingOrder);
    mSidProcedureTable->horizontalHeader()->setStretchLastSection(true);
    mSidProcedureTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    mSidProcedureTable->setSelectionMode(QAbstractItemView::SingleSelection);
    mSidProcedureEditor = new ProcedureEditorWidget(this);
    mSidProcedureEditor->setEnabled(false);

    auto* newSidProcedureButton = new QPushButton(QStringLiteral("Nouvelle SID"), this);
    auto* deleteSidProcedureButton = new QPushButton(QStringLiteral("Supprimer"), this);

    tabs->addTab(
        buildTabLayout(
            newSidProcedureButton,
            deleteSidProcedureButton,
            mSidProcedureTable,
            mSidProcedureEditor
        ),
        QStringLiteral("Procedure SID")
    );

    connect(
        mSidProcedureTable->selectionModel(),
        &QItemSelectionModel::currentRowChanged,
        this,
        &MainWindow::onSidProcedureSelectionChanged
    );

    connect(
        newSidProcedureButton,
        &QPushButton::clicked,
        this,
        &MainWindow::onNewSidProcedure
    );

    connect(
        deleteSidProcedureButton,
        &QPushButton::clicked,
        this,
        [this]() {
            deleteCurrentRow(
                mSidProcedureTable,
                mSidProcedureModel,
                mSidProcedureProxy,
                mSidProcedureEditor,
                [this](qint32 rawId) {
                    return mProject.sidProcedures().remove(ProcedureId(rawId));
                },
                [this]() {
                    mCurrentSidProcedureId = ProcedureId::invalid();
                },
                [this]() {
                    auto& table = mProject.sidProcedures();
                    if (!table.order().isEmpty())
                            table.renumberFrom(table.order().first().value());
                }
            );
        }
    );

    connect(
        mSidProcedureEditor,
        &ProcedureEditorWidget::valueEdited,
        this,
        &MainWindow::onSidProcedureEdited
    );


    // PROCEDURE STAR ---
    mStarProcedureModel = new GenericTableModel(
        orderFnFor(&mProject.starProcedures()), {
            idColumn(),
            textColumn(
                QStringLiteral("Ident Airport"),
                &mProject.starProcedures(),
                &UserProcedure::airportIdent
            ),
            textColumn(
                QStringLiteral("Ident séquence"),
                &mProject.starProcedures(),
                &UserProcedure::legSequenceIdent
            ),
        },
        this
    );

    mStarProcedureTable = new QTableView(this);
    mStarProcedureProxy = new QSortFilterProxyModel(this);
    mStarProcedureProxy->setSourceModel(mStarProcedureModel);
    mStarProcedureTable->setModel(mStarProcedureProxy);
    mStarProcedureTable->setSortingEnabled(true);
    mStarProcedureTable->sortByColumn(1, Qt::AscendingOrder);
    mStarProcedureTable->horizontalHeader()->setStretchLastSection(true);
    mStarProcedureTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    mStarProcedureTable->setSelectionMode(QAbstractItemView::SingleSelection);
    mStarProcedureEditor = new ProcedureEditorWidget(this);
    mStarProcedureEditor->setEnabled(false);

    auto* newStarProcedureButton = new QPushButton(QStringLiteral("Nouvelle STAR"), this);
    auto* deleteStarProcedureButton = new QPushButton(QStringLiteral("Supprimer"), this);

    tabs->addTab(
        buildTabLayout(
            newStarProcedureButton,
            deleteStarProcedureButton,
            mStarProcedureTable,
            mStarProcedureEditor
        ),
        QStringLiteral("Procedure STAR")
    );

    connect(
        mStarProcedureTable->selectionModel(),
        &QItemSelectionModel::currentRowChanged,
        this,
        &MainWindow::onStarProcedureSelectionChanged
    );

    connect(
        newStarProcedureButton,
        &QPushButton::clicked,
        this,
        &MainWindow::onNewStarProcedure
    );

    connect(
        deleteStarProcedureButton,
        &QPushButton::clicked,
        this,
        [this]() {
            deleteCurrentRow(
                mStarProcedureTable,
                mStarProcedureModel,
                mStarProcedureProxy,
                mStarProcedureEditor,
                [this](qint32 rawId) { return mProject.starProcedures().remove(ProcedureId(rawId)); },
                [this]() { mCurrentStarProcedureId = ProcedureId::invalid(); },
                [this]() {
                    auto& table = mProject.starProcedures();
                    if (!table.order().isEmpty())
                            table.renumberFrom(table.order().first().value());
                }
            );
        }
    );

    connect(
        mStarProcedureEditor,
        &ProcedureEditorWidget::valueEdited,
        this,
        &MainWindow::onStarProcedureEdited
    );

    // --- ProcedureTransition SID ---
    mSidProcedureTransitionModel = new GenericTableModel(
        orderFnFor(&mProject.sidProcedureTransitions()),
        {
            idColumn(),
            textColumn(
                QStringLiteral("Ident Procedure"),
                &mProject.sidProcedureTransitions(),
                &UserProcedureTransition::procedureIdent
            ),
            textColumn(
                QStringLiteral("Ident séquence"),
                &mProject.sidProcedureTransitions(),
                &UserProcedureTransition::legSequenceIdent
            ),
        },
        this
    );

    mSidProcedureTransitionTable = new QTableView(this);
    mSidProcedureTransitionProxy = new QSortFilterProxyModel(this);
    mSidProcedureTransitionProxy->setSourceModel(mSidProcedureTransitionModel);
    mSidProcedureTransitionTable->setModel(mSidProcedureTransitionProxy);
    mSidProcedureTransitionTable->setSortingEnabled(true);
    mSidProcedureTransitionTable->sortByColumn(1, Qt::AscendingOrder);
    mSidProcedureTransitionTable->horizontalHeader()->setStretchLastSection(true);
    mSidProcedureTransitionTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    mSidProcedureTransitionTable->setSelectionMode(QAbstractItemView::SingleSelection);
    mSidProcedureTransitionEditor = new ProcedureTransitionEditorWidget(this);
    mSidProcedureTransitionEditor->setEnabled(false);

    auto* newSidProcedureTransitionButton =
        new QPushButton(QStringLiteral("Nouvelle transition SID"), this);
    auto* deleteSidProcedureTransitionButton =
        new QPushButton(QStringLiteral("Supprimer"), this);

    tabs->addTab(
        buildTabLayout(
            newSidProcedureTransitionButton,
            deleteSidProcedureTransitionButton,
            mSidProcedureTransitionTable,
            mSidProcedureTransitionEditor
        ),
        QStringLiteral("ProcTrans SID")
    );

    connect(
        mSidProcedureTransitionTable->selectionModel(),
        &QItemSelectionModel::currentRowChanged,
        this,
        &MainWindow::onSidProcedureTransitionSelectionChanged
    );

    connect(
        newSidProcedureTransitionButton,
        &QPushButton::clicked,
        this,
        &MainWindow::onNewSidProcedureTransition
    );

    connect(
        deleteSidProcedureTransitionButton,
        &QPushButton::clicked,
        this,
        [this]() {
            deleteCurrentRow(
                mSidProcedureTransitionTable,
                mSidProcedureTransitionModel,
                mSidProcedureTransitionProxy,
                mSidProcedureTransitionEditor,
                [this](qint32 rawId) {
                    return mProject.sidProcedureTransitions().remove(ProcedureTransitionId(rawId));
                },
                [this]() {
                    mCurrentSidProcedureTransitionId = ProcedureTransitionId::invalid();
                },
                [this]() {
                    auto& table = mProject.sidProcedureTransitions();
                    if (!table.order().isEmpty())
                         table.renumberFrom(table.order().first().value());
                }
            );
        }
    );

    connect(
        mSidProcedureTransitionEditor,
        &ProcedureTransitionEditorWidget::valueEdited,
        this,
        &MainWindow::onSidProcedureTransitionEdited
    );

    // --- ProcedureTransition STAR ---
    mStarProcedureTransitionModel = new GenericTableModel(
        orderFnFor(&mProject.starProcedureTransitions()), {
            idColumn(),
            textColumn(
                QStringLiteral("Ident Procedure"),
                &mProject.starProcedureTransitions(),
                &UserProcedureTransition::procedureIdent
            ),
            textColumn(
                QStringLiteral("Ident séquence"),
                &mProject.starProcedureTransitions(),
                &UserProcedureTransition::legSequenceIdent
            ),
        },
        this
    );

    mStarProcedureTransitionTable = new QTableView(this);
    mStarProcedureTransitionProxy = new QSortFilterProxyModel(this);
    mStarProcedureTransitionProxy->setSourceModel(mStarProcedureTransitionModel);
    mStarProcedureTransitionTable->setModel(mStarProcedureTransitionProxy);
    mStarProcedureTransitionTable->setSortingEnabled(true);
    mStarProcedureTransitionTable->sortByColumn(1, Qt::AscendingOrder);
    mStarProcedureTransitionTable->horizontalHeader()->setStretchLastSection(true);
    mStarProcedureTransitionTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    mStarProcedureTransitionTable->setSelectionMode(QAbstractItemView::SingleSelection);
    mStarProcedureTransitionEditor = new ProcedureTransitionEditorWidget(this);
    mStarProcedureTransitionEditor->setEnabled(false);

    auto* newStarProcedureTransitionButton = new QPushButton(QStringLiteral("Nouvelle transition STAR"), this);
    auto* deleteStarProcedureTransitionButton = new QPushButton(QStringLiteral("Supprimer"), this);

    tabs->addTab(
        buildTabLayout(
            newStarProcedureTransitionButton,
            deleteStarProcedureTransitionButton,
            mStarProcedureTransitionTable,
            mStarProcedureTransitionEditor
        ),
        QStringLiteral("ProcTrans STAR")
    );

    connect(mStarProcedureTransitionTable->selectionModel(),
            &QItemSelectionModel::currentRowChanged,
            this,
            &MainWindow::onStarProcedureTransitionSelectionChanged
    );

    connect(
        newStarProcedureTransitionButton,
        &QPushButton::clicked,
        this,
        &MainWindow::onNewStarProcedureTransition
    );

    connect(
        deleteStarProcedureTransitionButton,
        &QPushButton::clicked,
        this,
        [this]() {
            deleteCurrentRow(
                mStarProcedureTransitionTable,
                mStarProcedureTransitionModel,
                mStarProcedureTransitionProxy,
                mStarProcedureTransitionEditor,
                [this](qint32 rawId) {
                    return mProject.starProcedureTransitions().remove(ProcedureTransitionId(rawId));
                },
                [this]() {
                    mCurrentStarProcedureTransitionId = ProcedureTransitionId::invalid();
                },
                [this]() {
                    auto& table = mProject.starProcedureTransitions();
                    if (!table.order().isEmpty())
                            table.renumberFrom(table.order().first().value());
                }
            );
        }
    );

    connect(
        mStarProcedureTransitionEditor,
        &ProcedureTransitionEditorWidget::valueEdited,
        this,
        &MainWindow::onStarProcedureTransitionEdited
    );


    // RUWAY PROCEDURETRANSITION SID ---
    mSidRunwayProcedureTransitionModel = new GenericTableModel(
        orderFnFor(&mProject.sidRunwayProcedureTransitions()),
        {
            idColumn(),
            textColumn(
                QStringLiteral("Ident Runway"),
                &mProject.sidRunwayProcedureTransitions(),
                &UserRunwayProcedureTransition::runwayIdent
            ),
            textColumn(
                QStringLiteral("Ident Procedure"),
                &mProject.sidRunwayProcedureTransitions(),
                &UserRunwayProcedureTransition::procedureIdent
            ),
            textColumn(
                QStringLiteral("Ident séquence"),
                &mProject.sidRunwayProcedureTransitions(),
                &UserRunwayProcedureTransition::legSequenceIdent
            ),
        },
        this
    );

    mSidRunwayProcedureTransitionTable = new QTableView(this);
    mSidRunwayProcedureTransitionProxy = new QSortFilterProxyModel(this);
    mSidRunwayProcedureTransitionProxy->setSourceModel(mSidRunwayProcedureTransitionModel);
    mSidRunwayProcedureTransitionTable->setModel(mSidRunwayProcedureTransitionProxy);
    mSidRunwayProcedureTransitionTable->setSortingEnabled(true);
    mSidRunwayProcedureTransitionTable->sortByColumn(1, Qt::AscendingOrder);
    mSidRunwayProcedureTransitionTable->horizontalHeader()->setStretchLastSection(true);
    mSidRunwayProcedureTransitionTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    mSidRunwayProcedureTransitionTable->setSelectionMode(QAbstractItemView::SingleSelection);
    mSidRunwayProcedureTransitionEditor = new RunwayProcedureTransitionEditorWidget(this);
    mSidRunwayProcedureTransitionEditor->setEnabled(false);

    auto* newSidRunwayProcedureTransitionButton = new QPushButton(QStringLiteral("Nouvelle RPT SID"), this);
    auto* deleteSidRunwayProcedureTransitionButton = new QPushButton(QStringLiteral("Supprimer"), this);

    tabs->addTab(
        buildTabLayout(
            newSidRunwayProcedureTransitionButton,
            deleteSidRunwayProcedureTransitionButton,
            mSidRunwayProcedureTransitionTable,
        mSidRunwayProcedureTransitionEditor
        ),
        QStringLiteral("RunProcTrans SID")
    );

    connect(
        mSidRunwayProcedureTransitionTable->selectionModel(),
        &QItemSelectionModel::currentRowChanged,
        this,
        &MainWindow::onSidRunwayProcedureTransitionSelectionChanged
    );

    connect(
        newSidRunwayProcedureTransitionButton,
        &QPushButton::clicked,
        this,
        &MainWindow::onNewSidRunwayProcedureTransition
    );

    connect(
        deleteSidRunwayProcedureTransitionButton,
        &QPushButton::clicked,
        this,
        [this]() {
            deleteCurrentRow(
                mSidRunwayProcedureTransitionTable,
                mSidRunwayProcedureTransitionModel,
                mSidRunwayProcedureTransitionProxy,
                mSidRunwayProcedureTransitionEditor,
                [this](qint32 rawId) {
                    return mProject.sidRunwayProcedureTransitions().remove(RunwayProcedureTransitionId(rawId));
                },
                [this]() {
                    mCurrentSidRunwayProcedureTransitionId = RunwayProcedureTransitionId::invalid();
                },
                [this]() {
                    auto& table = mProject.sidRunwayProcedureTransitions();
                    if (!table.order().isEmpty())
                                table.renumberFrom(table.order().first().value());
                }
            );
        }
    );

    connect(
        mSidRunwayProcedureTransitionEditor,
        &RunwayProcedureTransitionEditorWidget::valueEdited,
        this,
        &MainWindow::onSidRunwayProcedureTransitionEdited
    );


    // RUNWAY PROCEDURE TRANSITION STAR ---
    mStarRunwayProcedureTransitionModel = new GenericTableModel(
        orderFnFor(&mProject.starRunwayProcedureTransitions()), {
            idColumn(),
            textColumn(
                QStringLiteral("Ident Runway"),
                &mProject.starRunwayProcedureTransitions(),
                &UserRunwayProcedureTransition::runwayIdent
            ),
            textColumn(
                QStringLiteral("Ident Procedure"),
                &mProject.starRunwayProcedureTransitions(),
                &UserRunwayProcedureTransition::procedureIdent
            ),
            textColumn(
                QStringLiteral("Ident séquence"),
                &mProject.starRunwayProcedureTransitions(),
                &UserRunwayProcedureTransition::legSequenceIdent
            ),
        },
        this
    );

    mStarRunwayProcedureTransitionTable = new QTableView(this);
    mStarRunwayProcedureTransitionProxy = new QSortFilterProxyModel(this);
    mStarRunwayProcedureTransitionProxy->setSourceModel(mStarRunwayProcedureTransitionModel);
    mStarRunwayProcedureTransitionTable->setModel(mStarRunwayProcedureTransitionProxy);
    mStarRunwayProcedureTransitionTable->setSortingEnabled(true);
    mStarRunwayProcedureTransitionTable->sortByColumn(1, Qt::AscendingOrder);
    mStarRunwayProcedureTransitionTable->horizontalHeader()->setStretchLastSection(true);
    mStarRunwayProcedureTransitionTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    mStarRunwayProcedureTransitionTable->setSelectionMode(QAbstractItemView::SingleSelection);
    mStarRunwayProcedureTransitionEditor = new RunwayProcedureTransitionEditorWidget(this);
    mStarRunwayProcedureTransitionEditor->setEnabled(false);

    auto* newStarRunwayProcedureTransitionButton = new QPushButton(QStringLiteral("Nouvelle RPT STAR"), this);
    auto* deleteStarRunwayProcedureTransitionButton = new QPushButton(QStringLiteral("Supprimer"), this);

    tabs->addTab(
        buildTabLayout(
            newStarRunwayProcedureTransitionButton,
            deleteStarRunwayProcedureTransitionButton,
            mStarRunwayProcedureTransitionTable, mStarRunwayProcedureTransitionEditor
        ),
        QStringLiteral("RunProcTrans STAR")
    );

    connect(
        mStarRunwayProcedureTransitionTable->selectionModel(),
        &QItemSelectionModel::currentRowChanged,
        this,
        &MainWindow::onStarRunwayProcedureTransitionSelectionChanged
    );

    connect(
        newStarRunwayProcedureTransitionButton,
        &QPushButton::clicked,
        this,
        &MainWindow::onNewStarRunwayProcedureTransition
    );

    connect(
        deleteStarRunwayProcedureTransitionButton,
        &QPushButton::clicked,
        this,
        [this]() {
            deleteCurrentRow(
                mStarRunwayProcedureTransitionTable,
                mStarRunwayProcedureTransitionModel,
                mStarRunwayProcedureTransitionProxy,
                mStarRunwayProcedureTransitionEditor,
                [this](qint32 rawId) {
                    return mProject.starRunwayProcedureTransitions().remove(RunwayProcedureTransitionId(rawId));
                },
                [this]() {
                    mCurrentStarRunwayProcedureTransitionId = RunwayProcedureTransitionId::invalid();
                },
                [this]() {
                    auto& table = mProject.starRunwayProcedureTransitions();
                    if (!table.order().isEmpty())
                            table.renumberFrom(table.order().first().value());
                }
            );
        }
    );

    connect(
        mStarRunwayProcedureTransitionEditor,
        &RunwayProcedureTransitionEditorWidget::valueEdited,
        this,
        &MainWindow::onStarRunwayProcedureTransitionEdited
    );

    setCentralWidget(tabs);
    setWindowTitle(QStringLiteral("FF777 NavStudio — aucun projet ouvert"));

    // Occupe la largeur de l'écran dès le lancement — showMaximized() plutôt
    // qu'un calcul manuel de géométrie (fiable quel que soit l'écran/la
    // configuration multi-moniteurs, barre de menu/dock déjà pris en compte).
    showMaximized();

} // !Constructeur


// -----------------------------------------------------------------------------------------------------------
// Libère l'interface construite par uic.
MainWindow::~MainWindow()
{
    delete ui;
}


// -----------------------------------------------------------------------------------------------------------
// Affiche la boîte de dialogue « À propos... ».
void MainWindow::onAbout()
{
    QMessageBox about;
    about.setWindowTitle(QStringLiteral("À propos de FF777 NavStudio"));

    // Dans le code on utilise l'alias de la ressource (son chemin virtuel) càd ':/app_icon.png'
    // cf. le fichier 'docs.qrc' qui pointe vers la ressource réelle '../../icones/small_app_icon.png'
    about.setIconPixmap(QPixmap(QStringLiteral(":/app_icon.png")));

    // Le titre principal (Texte affiché en grand en haut)
    about.setText(QStringLiteral("<h3>FF777 NavStudio</h3> par MortyMars"));
    about.setTextFormat(Qt::RichText);

    // Le corps du texte (Où le gras fonctionnera nativement)
    about.setInformativeText(
        QStringLiteral(
            R"(
                <div style="font-size: 13pt;">
                    <p><b>Version :</b> v0.9b du 31 août 2026</p>
                    <p><b>Finalité :</b> Studio d'édition de données de navigation (SIDs, STARs, Approaches,
                    Runways, Navaids...) à l'usage du Boeing 777 de FlightFactor.</p>
                    <p>Il permet de créer, de modifier et de maintenir à jour un jeu de procédures d'un
                    aéroport fictif, de l'indexer sur le fichier mondial et de l'encoder dans un 'nav1.db'
                    augmenté, lisible par le FF777.</p>
                </div>
            )"
        )
    );

    about.exec();

}

// -----------------------------------------------------------------------------------------------------------
// Ouvre le document Markdown d'aide dans une boîte de dialogue rendant le
// Markdown (rôle Qt :: MarkdownText du QTextBrowser) — à défaut d'un
// navigateur externe, l'aide reste lisible dans l'application elle-même.
void MainWindow::onDocumentation()
{
    // Dans le code on utilise l'alias de la ressource (son chemin virtuel) càd ':/Readme.md'
    // cf. le fichier 'docs.qrc' qui pointe vers la ressource réelle '../../Readme.md'
    QFile docFile(QStringLiteral(":/Readme.md"));

    if (!docFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(
            this,
            QStringLiteral("Documentation"),
            QStringLiteral("Documents Markdown introuvable.")
        );
        return;
    }

    const QString markdown = QString::fromUtf8(docFile.readAll());

    auto* textBrowser = new QTextBrowser(this);

    textBrowser->setOpenExternalLinks(true);
    textBrowser->setMarkdown(markdown);

    QDialog dialog(this);

    dialog.setWindowTitle(QStringLiteral("Finalité de l'application — FF777 NavStudio"));
    dialog.resize(720, 560);

    auto* layout = new QVBoxLayout(&dialog);
    auto* closeButton = new QPushButton(QStringLiteral("Fermer"));

    layout->addWidget(textBrowser);
    layout->addWidget(closeButton, 0, Qt::AlignRight);

    connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);

    dialog.exec();
}

// -----------------------------------------------------------------------------------------------------------
// Assemble la mise en page d'un onglet : boutons + table à gauche,
// éditeur à droite, séparés par un splitter.
QWidget* MainWindow::buildTabLayout(
            QPushButton* newButton,
            QPushButton* deleteButton,
            QTableView* table,
            QWidget* editor
            )
{
    auto* buttonsLayout = new QHBoxLayout;
    buttonsLayout->addWidget(newButton);
    buttonsLayout->addWidget(deleteButton);

    auto* leftLayout = new QVBoxLayout;
    leftLayout->addLayout(buttonsLayout);
    leftLayout->addWidget(table);
    auto* leftWidget = new QWidget(this);
    leftWidget->setLayout(leftLayout);

    auto* splitter = new EvenSplitter(Qt::Horizontal, this);
    splitter->addWidget(leftWidget);
    splitter->addWidget(editor);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);
    return splitter;
}

// -----------------------------------------------------------------------------------------------------------
// Retourne l'id brut de la ligne sélectionnée dans la vue.
// Id brut de la ligne sélectionnée dans la vue — passe par le proxy de tri
// car la sélection de la QTableView porte un index PROXY, pas un index du
// GenericTableModel sous-jacent (dont l'ordre, lui, reste l'ordre
// d'insertion, celui que le Writer doit suivre — cf. TableColumnHelpers.h,
// GenericTableModel.h). -1 si rien n'est sélectionné.
qint32 MainWindow::currentRawId(QTableView* table, GenericTableModel* model, QSortFilterProxyModel* proxy) const
{
    const QModelIndex proxyIndex = table->currentIndex();

    if (!proxyIndex.isValid())
        return -1;

    const QModelIndex sourceIndex = proxy->mapToSource(proxyIndex);

    return model->idAt(sourceIndex.row());
}

// -----------------------------------------------------------------------------------------------------------
// Sélectionne dans la vue la ligne du modèle source passée en argument.
// Sélectionne dans la vue la ligne portant sourceRow dans le modèle SOURCE —
// passe par le proxy pour la retrouver une fois triée (elle n'apparaît plus
// forcément en dernière position dès que le tri alphabétique est actif,
// contrairement à l'hypothèse valable avant l'introduction du proxy).
void MainWindow::selectSourceRow(QTableView* table, GenericTableModel* model, QSortFilterProxyModel* proxy, int sourceRow)
{
    const QModelIndex proxyIndex = proxy->mapFromSource(model->index(sourceRow, 0));
    table->setCurrentIndex(proxyIndex);
}

// -----------------------------------------------------------------------------------------------------------
// Supprime la ligne actuellement sélectionnée dans le tableau donné.
// Supprime la ligne actuellement sélectionnée dans table, si une l'est.
// Générique sur les 15 onglets — même geste partout (retirer de la table
// EntityTable via removeFn, invalider l'id courant via clearCurrentId,
// COMBLER LE TROU laissé par la suppression via compactFn, désactiver
// l'éditeur, recharger le modèle) sans dupliquer la logique 15 fois. Le
// type concret de l'id diffère par onglet, d'où les std::function plutôt
// qu'un template ici (le .cpp n'a pas besoin d'être une unité de
// compilation générique).
//
// compactFn renumérote la table à partir de SON PROPRE premier id restant
// (jamais en dessous — cf. EntityTable::renumberFrom) : la suppression ne
// fait donc jamais reculer la continuité avec le fichier mondial, elle ne
// fait que refermer les trous internes laissés par la ligne retirée.
void MainWindow::deleteCurrentRow(
        QTableView* table,
        GenericTableModel* model,
        QSortFilterProxyModel* proxy, QWidget* editor,
        const std::function<bool(qint32)>& removeFn,
        const std::function<void()>& clearCurrentId,
        const std::function<void()>& compactFn
        )
{
    const qint32 rawId = currentRawId(table, model, proxy);

    if (rawId < 0)
        return;

    if (!removeFn(rawId))
        return;

    clearCurrentId();

    compactFn();

    editor->setEnabled(false);

    model->reload();
}

// -----------------------------------------------------------------------------------------------------------
// Construit l'aperçu (ou l'erreur) d'une entité à partir du résultat de régénération.
QString MainWindow::previewFor(EntityKind kind, qint32 rawId, const RegenerationResult& result) const
{
    for (const ConversionFailure& f : result.conversionFailures) {
        if (f.kind == kind && f.userId == rawId)
        return QStringLiteral("⚠ ") + f.errors.join(QStringLiteral(" ; "));
    }

    switch (kind) {

    case EntityKind::Waypoint:
        if (const Waypoint* w =
            result.repository.waypoints().find(WaypointId(rawId))) {
                return NavDataWriter::formatWaypointLine(WaypointId(rawId), *w);
        }
        break;

    case EntityKind::Airport:
        if (const Airport* a =
            result.repository.airports().find(AirportId(rawId))) {
                return NavDataWriter::formatAirportLine(AirportId(rawId), *a);
        }
        break;

    case EntityKind::Runway:
        if (const Runway* r =
            result.repository.runways().find(RunwayId(rawId))) {
                return NavDataWriter::formatRunwayLine(RunwayId(rawId), *r);
        }
        break;

    case EntityKind::Navaid:
        if (const Navaid* n =
            result.repository.navaids().find(NavaidId(rawId))) {
                return NavDataWriter::formatNavaidLine(NavaidId(rawId), *n);
        }
        break;

    case EntityKind::LegSequence:
        if (const LegSequence* ls =
            result.repository.legSequences().find(LegSequenceId(rawId))) {
                return NavDataWriter::formatLegSequenceLine(LegSequenceId(rawId), *ls);
        }
        break;

    case EntityKind::Leg:
        if (const Leg* l =
            result.repository.legs().find(LegId(rawId))) {
                return NavDataWriter::formatLegLine(LegId(rawId), *l);
        }
        break;

    case EntityKind::Approach:
        if (const Approach* a =
            result.repository.approaches().find(ApproachId(rawId))) {
                return NavDataWriter::formatApproachLine(ApproachId(rawId), *a);
        }
        break;

    case EntityKind::ApproachTransition:
        if (const ApproachTransition* t =
            result.repository.approachTransitions().find(ApproachTransitionId(rawId))) {
                return NavDataWriter::formatApproachTransitionLine(ApproachTransitionId(rawId), *t);
        }
        break;

    case EntityKind::SidProcedure:
        if (const Procedure* p =
            result.repository.procedures(ProcedureKind::Sid).find(ProcedureId(rawId))) {
                return NavDataWriter::formatProcedureLine(ProcedureId(rawId), *p);
        }
        break;

    case EntityKind::StarProcedure:
        if (const Procedure* p =
            result.repository.procedures(ProcedureKind::Star).find(ProcedureId(rawId))) {
                return NavDataWriter::formatProcedureLine(ProcedureId(rawId), *p);
        }
        break;

    case EntityKind::SidProcedureTransition:
        if (const ProcedureTransition* t =
            result.repository.procedureTransitions(ProcedureKind::Sid).find(ProcedureTransitionId(rawId))) {
                return NavDataWriter::formatProcedureTransitionLine(ProcedureTransitionId(rawId), *t);
        }
        break;

    case EntityKind::StarProcedureTransition:
        if (const ProcedureTransition* t =
            result.repository.procedureTransitions(ProcedureKind::Star).find(ProcedureTransitionId(rawId))) {
                return NavDataWriter::formatProcedureTransitionLine(ProcedureTransitionId(rawId), *t);
        }
        break;

    case EntityKind::SidRunwayProcedureTransition:
        if (const RunwayProcedureTransition* t =
            result.repository.runwayProcedureTransitions(ProcedureKind::Sid).find(RunwayProcedureTransitionId(rawId))) {
                return NavDataWriter::formatRunwayProcedureTransitionLine(RunwayProcedureTransitionId(rawId), *t);
        }
        break;

    case EntityKind::StarRunwayProcedureTransition:
        if (const RunwayProcedureTransition* t =
            result.repository.runwayProcedureTransitions(ProcedureKind::Star).find(RunwayProcedureTransitionId(rawId))) {
                return NavDataWriter::formatRunwayProcedureTransitionLine(RunwayProcedureTransitionId(rawId), *t);
        }
        break;

    default:
        break;
    }
    return QStringLiteral("⚠ (aucune donnée)");
}

// ===========================================================================================================
// Point — auto-suffisant, aucune régénération nécessaire (PointEditorWidget
// calcule lui-même son aperçu, cf. UserToModelConverter::convertPoint qui ne
// résout aucun ident).
// -----------------------------------------------------------------------------------------------------------
// Met à jour l'éditeur de point lors d'un changement de sélection dans le tableau.
void MainWindow::onPointSelectionChanged()
{
    const qint32 rawId = currentRawId(mPointTable, mPointModel, mPointProxy);

    if (rawId < 0) {
        mCurrentPointId = PointId::invalid();
        mPointEditor->setEnabled(false);
        return;
    }

    mCurrentPointId = PointId(rawId);

    if (const UserPoint* p = mProject.points().find(mCurrentPointId)) {
        mPointEditor->setEnabled(true);
        mPointEditor->setValue(mCurrentPointId, *p);
    }
}

// -----------------------------------------------------------------------------------------------------------
// Ajoute un nouveau point et le sélectionne pour édition immédiate.
void MainWindow::onNewPoint()
{
    mProject.points().add(UserPoint{});

    mPointModel->reload();

    selectSourceRow(
        mPointTable,
        mPointModel,
        mPointProxy,
        mPointModel->rowCount() - 1
    );

    mPointEditor->focusIdent();
}

// -----------------------------------------------------------------------------------------------------------
// Met à jour le point courant à partir des valeurs de l'éditeur.
void MainWindow::onPointEdited()
{
    if (!mCurrentPointId.isValid())
        return;

    mProject.points().update(mCurrentPointId, mPointEditor->value());

    mPointModel->notifyRowChanged(mCurrentPointId.value());
}


// ===========================================================================================================
// Waypoint / Airport / Runway / Navaid — ont besoin d'IdentResolver, donc
// d'une régénération complète du projet à chaque frappe (dataset petit,
// coût négligeable) pour afficher un aperçu réellement résolu.
// -----------------------------------------------------------------------------------------------------------
// Met à jour l'éditeur de waypoint à la sélection, avec aperçu régénéré.
void MainWindow::onWaypointSelectionChanged()
{
    const qint32 rawId = currentRawId(mWaypointTable, mWaypointModel, mWaypointProxy);

    if (rawId < 0) {
        mCurrentWaypointId = WaypointId::invalid();
        mWaypointEditor->setEnabled(false);
        return;
    }

    mCurrentWaypointId = WaypointId(rawId);

    if (const UserWaypoint* w = mProject.waypoints().find(mCurrentWaypointId)) {
        mWaypointEditor->setEnabled(true);
        mWaypointEditor->setValue(mCurrentWaypointId, *w);
        const RegenerationResult result = Regenerator().regenerate(mProject);
        mWaypointEditor->setPreviewLine(
            previewFor(
                EntityKind::Waypoint,
                mCurrentWaypointId.value(),
                result
            )
        );
    }
}

// -----------------------------------------------------------------------------------------------------------
// Ajoute un nouveau waypoint et le sélectionne pour édition.
void MainWindow::onNewWaypoint()
{
    mProject.waypoints().add(UserWaypoint{});

    mWaypointModel->reload();

    selectSourceRow(
        mWaypointTable,
        mWaypointModel,
        mWaypointProxy,
        mWaypointModel->rowCount() - 1
    );

    mWaypointEditor->focusFirstField();
}

// -----------------------------------------------------------------------------------------------------------
// Met à jour le waypoint courant et régénère l'aperçu résolu.
void MainWindow::onWaypointEdited()
{
    if (!mCurrentWaypointId.isValid())
        return;

    mProject.waypoints().update(mCurrentWaypointId, mWaypointEditor->value());

    mWaypointModel->notifyRowChanged(mCurrentWaypointId.value());

    const RegenerationResult result = Regenerator().regenerate(mProject);

    mWaypointEditor->setPreviewLine(
        previewFor(
            EntityKind::Waypoint,
            mCurrentWaypointId.value(),
            result
        )
    );
}

// -----------------------------------------------------------------------------------------------------------
// Met à jour l'éditeur d'aéroport à la sélection, avec aperçu régénéré.
void MainWindow::onAirportSelectionChanged()
{
    const qint32 rawId = currentRawId(mAirportTable, mAirportModel, mAirportProxy);

    if (rawId < 0) {
        mCurrentAirportId = AirportId::invalid();
        mAirportEditor->setEnabled(false);
        return;
    }

    mCurrentAirportId = AirportId(rawId);

    if (const UserAirport* a = mProject.airports().find(mCurrentAirportId)) {

        mAirportEditor->setEnabled(true);

        mAirportEditor->setValue(mCurrentAirportId, *a);

        const RegenerationResult result = Regenerator().regenerate(mProject);

        mAirportEditor->setPreviewLine(
            previewFor(
                EntityKind::Airport,
                mCurrentAirportId.value(),
                result
            )
        );
    }
}


// -----------------------------------------------------------------------------------------------------------
// Ajoute un nouvel aéroport et le sélectionne pour édition immédiate.
void MainWindow::onNewAirport()
{
    mProject.airports().add(UserAirport{});

    mAirportModel->reload();

    selectSourceRow(
        mAirportTable,
        mAirportModel,
        mAirportProxy,
        mAirportModel->rowCount() - 1
    );

    mAirportEditor->focusFirstField();
}


// -----------------------------------------------------------------------------------------------------------
// Met à jour l'aéroport courant et régénère l'aperçu résolu.
void MainWindow::onAirportEdited()
{
    if (!mCurrentAirportId.isValid())
        return;

    mProject.airports().update(mCurrentAirportId, mAirportEditor->value());

    mAirportModel->notifyRowChanged(mCurrentAirportId.value());

    const RegenerationResult result = Regenerator().regenerate(mProject);

    mAirportEditor->setPreviewLine(
        previewFor(
            EntityKind::Airport,
            mCurrentAirportId.value(),
            result
        )
    );
}


// -----------------------------------------------------------------------------------------------------------
// Met à jour l'éditeur de piste à la sélection, avec aperçu régénéré.
void MainWindow::onRunwaySelectionChanged()
{
    const qint32 rawId = currentRawId(mRunwayTable, mRunwayModel, mRunwayProxy);

    if (rawId < 0) {
        mCurrentRunwayId = RunwayId::invalid();
        mRunwayEditor->setEnabled(false);
        return;
    }

    mCurrentRunwayId = RunwayId(rawId);

    if (const UserRunway* r = mProject.runways().find(mCurrentRunwayId)) {
        mRunwayEditor->setEnabled(true);
        mRunwayEditor->setValue(mCurrentRunwayId, *r);
        const RegenerationResult result = Regenerator().regenerate(mProject);
        mRunwayEditor->setPreviewLine(
            previewFor(
                EntityKind::Runway,
                mCurrentRunwayId.value(),
                result
            )
        );
    }
}

// -----------------------------------------------------------------------------------------------------------
// Ajoute une nouvelle piste et la sélectionne pour édition immédiate.
void MainWindow::onNewRunway()
{
    mProject.runways().add(UserRunway{});

    mRunwayModel->reload();

    selectSourceRow(
        mRunwayTable,
        mRunwayModel,
        mRunwayProxy,
        mRunwayModel->rowCount() - 1
    );

    mRunwayEditor->focusFirstField();
}

// -----------------------------------------------------------------------------------------------------------
// Met à jour la piste courante et régénère l'aperçu résolu.
void MainWindow::onRunwayEdited()
{
    if (!mCurrentRunwayId.isValid())
        return;

    mProject.runways().update(mCurrentRunwayId, mRunwayEditor->value());

    mRunwayModel->notifyRowChanged(mCurrentRunwayId.value());

    const RegenerationResult result = Regenerator().regenerate(mProject);

    mRunwayEditor->setPreviewLine(
        previewFor(
            EntityKind::Runway,
            mCurrentRunwayId.value(),
            result
        )
    );
}

// -----------------------------------------------------------------------------------------------------------
// Met à jour l'éditeur de navaid à la sélection, avec aperçu régénéré.
void MainWindow::onNavaidSelectionChanged()
{
    const qint32 rawId = currentRawId(mNavaidTable, mNavaidModel, mNavaidProxy);

    if (rawId < 0) {
        mCurrentNavaidId = NavaidId::invalid();
        mNavaidEditor->setEnabled(false);
        return;
    }

    mCurrentNavaidId = NavaidId(rawId);

    if (const UserNavaid* n = mProject.navaids().find(mCurrentNavaidId)) {

        mNavaidEditor->setEnabled(true);

        mNavaidEditor->setValue(mCurrentNavaidId, *n);

        const RegenerationResult result = Regenerator().regenerate(mProject);

        mNavaidEditor->setPreviewLine(
            previewFor(
                EntityKind::Navaid,
                mCurrentNavaidId.value(),
                result
            )
        );
    }
}


// -----------------------------------------------------------------------------------------------------------
// Ajoute un nouveau navaid et le sélectionne pour édition immédiate.
void MainWindow::onNewNavaid()
{
    mProject.navaids().add(UserNavaid{});

    mNavaidModel->reload();

    selectSourceRow(
        mNavaidTable,
        mNavaidModel,
        mNavaidProxy,
        mNavaidModel->rowCount() - 1
    );

    mNavaidEditor->focusFirstField();
}


// -----------------------------------------------------------------------------------------------------------
// Met à jour le navaid courant et régénère l'aperçu résolu.
void MainWindow::onNavaidEdited()
{
    if (!mCurrentNavaidId.isValid())
        return;

    mProject.navaids().update(mCurrentNavaidId, mNavaidEditor->value());

    mNavaidModel->notifyRowChanged(mCurrentNavaidId.value());

    const RegenerationResult result = Regenerator().regenerate(mProject);

    mNavaidEditor->setPreviewLine(
        previewFor(
            EntityKind::Navaid,
            mCurrentNavaidId.value(),
            result
        )
    );
}


// -----------------------------------------------------------------------------------------------------------
// Met à jour l'éditeur de séquence de legs à la sélection, avec aperçu régénéré.
void MainWindow::onLegSequenceSelectionChanged()
{
    const qint32 rawId = currentRawId(mLegSequenceTable, mLegSequenceModel, mLegSequenceProxy);

    if (rawId < 0) {
        mCurrentLegSequenceId = LegSequenceId::invalid();
        mLegSequenceEditor->setEnabled(false);
        return;
    }

    mCurrentLegSequenceId = LegSequenceId(rawId);

    if (const UserLegSequence* ls = mProject.legSequences().find(mCurrentLegSequenceId)) {

        mLegSequenceEditor->setEnabled(true);

        mLegSequenceEditor->setValue(mCurrentLegSequenceId, *ls);

        const RegenerationResult result = Regenerator().regenerate(mProject);

        mLegSequenceEditor->setPreviewLine(
            previewFor(
                EntityKind::LegSequence,
                mCurrentLegSequenceId.value(),
                result
            )
        );
    }
}


// -----------------------------------------------------------------------------------------------------------
// Ajoute une nouvelle séquence de legs et la sélectionne pour édition immédiate.
void MainWindow::onNewLegSequence()
{
    mProject.legSequences().add(UserLegSequence{});

    mLegSequenceModel->reload();

    selectSourceRow(
        mLegSequenceTable,
        mLegSequenceModel,
        mLegSequenceProxy,
        mLegSequenceModel->rowCount() - 1
    );

    mLegSequenceEditor->focusFirstField();
}

// -----------------------------------------------------------------------------------------------------------
// Met à jour la séquence de legs courante et régénère l'aperçu résolu.
void MainWindow::onLegSequenceEdited()
{
    if (!mCurrentLegSequenceId.isValid())
        return;

    mProject.legSequences().update(mCurrentLegSequenceId, mLegSequenceEditor->value());

    mLegSequenceModel->notifyRowChanged(mCurrentLegSequenceId.value());

    const RegenerationResult result = Regenerator().regenerate(mProject);

    mLegSequenceEditor->setPreviewLine(
        previewFor(
            EntityKind::LegSequence,
            mCurrentLegSequenceId.value(),
            result
        )
    );
}

// -----------------------------------------------------------------------------------------------------------
// Met à jour l'éditeur de leg à la sélection, avec aperçu régénéré.
void MainWindow::onLegSelectionChanged()
{
    const qint32 rawId = currentRawId(mLegTable, mLegModel, mLegProxy);

    if (rawId < 0) {
        mCurrentLegId = LegId::invalid();
        mLegEditor->setEnabled(false);
        return;
    }
    mCurrentLegId = LegId(rawId);

    if (const UserLeg* l = mProject.legs().find(mCurrentLegId)) {

        mLegEditor->setEnabled(true);

        mLegEditor->setValue(mCurrentLegId, *l);

        const RegenerationResult result = Regenerator().regenerate(mProject);

        mLegEditor->setPreviewLine(
            previewFor(
                EntityKind::Leg,
                mCurrentLegId.value(),
                result
            )
        );
    }
}


// -----------------------------------------------------------------------------------------------------------
// Ajoute un nouveau leg et le sélectionne pour édition immédiate.
void MainWindow::onNewLeg()
{
    mProject.legs().add(UserLeg{});
    mLegModel->reload();
    selectSourceRow(mLegTable, mLegModel, mLegProxy, mLegModel->rowCount() - 1);
    mLegEditor->focusFirstField();
}


// -----------------------------------------------------------------------------------------------------------
// Met à jour le leg courant et régénère l'aperçu résolu.
void MainWindow::onLegEdited()
{
    if (!mCurrentLegId.isValid())
        return;

    mProject.legs().update(mCurrentLegId, mLegEditor->value());

    mLegModel->notifyRowChanged(mCurrentLegId.value());

    const RegenerationResult result = Regenerator().regenerate(mProject);

    mLegEditor->setPreviewLine(
        previewFor(
            EntityKind::Leg,
            mCurrentLegId.value(),
            result
        )
    );
}


// ===========================================================================================================
// APPROACH
// -----------------------------------------------------------------------------------------------------------
// Met à jour l'éditeur d'approche à la sélection, avec aperçu régénéré.
void MainWindow::onApproachSelectionChanged()
{
    const qint32 rawId = currentRawId(mApproachTable, mApproachModel, mApproachProxy);

    if (rawId < 0) {
        mCurrentApproachId = ApproachId::invalid();
        mApproachEditor->setEnabled(false);
        return;
    }

    mCurrentApproachId = ApproachId(rawId);

    if (const UserApproach* a = mProject.approaches().find(mCurrentApproachId)) {

        mApproachEditor->setEnabled(true);

        mApproachEditor->setValue(mCurrentApproachId, *a);

        const RegenerationResult result = Regenerator().regenerate(mProject);

        mApproachEditor->setPreviewLine(
            previewFor(
                EntityKind::Approach,
                mCurrentApproachId.value(),
                result
            )
        );
    }
}


// -----------------------------------------------------------------------------------------------------------
// Ajoute une nouvelle approche et la sélectionne pour édition immédiate.
void MainWindow::onNewApproach()
{
    mProject.approaches().add(UserApproach{});

    mApproachModel->reload();

    selectSourceRow(
        mApproachTable,
        mApproachModel,
        mApproachProxy,
        mApproachModel->rowCount() - 1
    );

    mApproachEditor->focusFirstField();
}


// -----------------------------------------------------------------------------------------------------------
// Met à jour l'approche courante et régénère l'aperçu résolu.
void MainWindow::onApproachEdited()
{
    if (!mCurrentApproachId.isValid())
        return;

    mProject.approaches().update(mCurrentApproachId, mApproachEditor->value());

    mApproachModel->notifyRowChanged(mCurrentApproachId.value());

    const RegenerationResult result = Regenerator().regenerate(mProject);

    mApproachEditor->setPreviewLine(
        previewFor(
            EntityKind::Approach,
            mCurrentApproachId.value(),
            result
        )
    );
}


// ===========================================================================================================
// APPROACHTRANSITION
// -----------------------------------------------------------------------------------------------------------
// Met à jour l'éditeur de transition d'approche à la sélection, avec aperçu.
void MainWindow::onApproachTransitionSelectionChanged()
{
    const qint32 rawId = currentRawId(mApproachTransitionTable, mApproachTransitionModel, mApproachTransitionProxy);

    if (rawId < 0) {
        mCurrentApproachTransitionId = ApproachTransitionId::invalid();
        mApproachTransitionEditor->setEnabled(false);
        return;
    }

    mCurrentApproachTransitionId = ApproachTransitionId(rawId);

    if (const UserApproachTransition* t = mProject.approachTransitions().find(mCurrentApproachTransitionId)) {

        mApproachTransitionEditor->setEnabled(true);

        mApproachTransitionEditor->setValue(mCurrentApproachTransitionId, *t);

        const RegenerationResult result = Regenerator().regenerate(mProject);

        mApproachTransitionEditor->setPreviewLine(
            previewFor(
                EntityKind::ApproachTransition,
                mCurrentApproachTransitionId.value(),
                result
            )
        );
    }
}

// -----------------------------------------------------------------------------------------------------------
// Ajoute une nouvelle transition d'approche et la sélectionne pour édition.
void MainWindow::onNewApproachTransition()
{
    mProject.approachTransitions().add(UserApproachTransition{});

    mApproachTransitionModel->reload();

    selectSourceRow(
        mApproachTransitionTable,
        mApproachTransitionModel,
        mApproachTransitionProxy,
        mApproachTransitionModel->rowCount() - 1
    );

    mApproachTransitionEditor->focusFirstField();
}

// -----------------------------------------------------------------------------------------------------------
// Met à jour la transition d'approche courante et régénère l'aperçu résolu.
void MainWindow::onApproachTransitionEdited()
{
    if (!mCurrentApproachTransitionId.isValid())
        return;

    mProject.approachTransitions().update(mCurrentApproachTransitionId, mApproachTransitionEditor->value());

    mApproachTransitionModel->notifyRowChanged(mCurrentApproachTransitionId.value());

    const RegenerationResult result = Regenerator().regenerate(mProject);

    mApproachTransitionEditor->setPreviewLine(
        previewFor(
            EntityKind::ApproachTransition,
            mCurrentApproachTransitionId.value(),
            result
        )
    );
}


// ===========================================================================================================
// PROCEDURE SID
// -----------------------------------------------------------------------------------------------------------
// Met à jour l'éditeur de procédure SID à la sélection, avec aperçu régénéré.
void MainWindow::onSidProcedureSelectionChanged()
{
    const qint32 rawId = currentRawId(mSidProcedureTable, mSidProcedureModel, mSidProcedureProxy);

    if (rawId < 0) {
        mCurrentSidProcedureId = ProcedureId::invalid();
        mSidProcedureEditor->setEnabled(false);
        return;
    }

    mCurrentSidProcedureId = ProcedureId(rawId);

    if (const UserProcedure* p = mProject.sidProcedures().find(mCurrentSidProcedureId)) {

        mSidProcedureEditor->setEnabled(true);

        mSidProcedureEditor->setValue(mCurrentSidProcedureId, *p);

        const RegenerationResult result = Regenerator().regenerate(mProject);

        mSidProcedureEditor->setPreviewLine(
            previewFor(
                EntityKind::SidProcedure,
                mCurrentSidProcedureId.value(),
                result
            )
        );
    }
}

// -----------------------------------------------------------------------------------------------------------
// Ajoute une nouvelle procédure SID et la sélectionne pour édition immédiate.
void MainWindow::onNewSidProcedure()
{
    mProject.sidProcedures().add(UserProcedure{});

    mSidProcedureModel->reload();

    selectSourceRow(
        mSidProcedureTable,
        mSidProcedureModel,
        mSidProcedureProxy,
        mSidProcedureModel->rowCount() - 1
    );

    mSidProcedureEditor->focusFirstField();
}

// -----------------------------------------------------------------------------------------------------------
// Met à jour la procédure SID courante et régénère l'aperçu résolu.
void MainWindow::onSidProcedureEdited()
{
    if (!mCurrentSidProcedureId.isValid())
        return;

    mProject.sidProcedures().update(mCurrentSidProcedureId, mSidProcedureEditor->value());

    mSidProcedureModel->notifyRowChanged(mCurrentSidProcedureId.value());

    const RegenerationResult result = Regenerator().regenerate(mProject);

    mSidProcedureEditor->setPreviewLine(
        previewFor(
            EntityKind::SidProcedure,
            mCurrentSidProcedureId.value(),
            result
        )
    );
}


// ===========================================================================================================
// PROCEDURE STAR
// -----------------------------------------------------------------------------------------------------------
// Met à jour l'éditeur de procédure STAR à la sélection, avec aperçu régénéré.
void MainWindow::onStarProcedureSelectionChanged()
{
    const qint32 rawId = currentRawId(mStarProcedureTable, mStarProcedureModel, mStarProcedureProxy);

    if (rawId < 0) {
            mCurrentStarProcedureId = ProcedureId::invalid();
            mStarProcedureEditor->setEnabled(false);
            return;
    }

    mCurrentStarProcedureId = ProcedureId(rawId);

    if (const UserProcedure* p = mProject.starProcedures().find(mCurrentStarProcedureId)) {

            mStarProcedureEditor->setEnabled(true);

            mStarProcedureEditor->setValue(mCurrentStarProcedureId, *p);

            const RegenerationResult result = Regenerator().regenerate(mProject);

            mStarProcedureEditor->setPreviewLine(
                previewFor(EntityKind::StarProcedure,
                           mCurrentStarProcedureId.value(),
                           result
                )
            );
    }
}


// -----------------------------------------------------------------------------------------------------------
// Ajoute une nouvelle procédure STAR et la sélectionne pour édition immédiate.
void MainWindow::onNewStarProcedure()
{
    mProject.starProcedures().add(UserProcedure{});

    mStarProcedureModel->reload();

    selectSourceRow(
        mStarProcedureTable,
        mStarProcedureModel,
        mStarProcedureProxy,
        mStarProcedureModel->rowCount() - 1
    );

    mStarProcedureEditor->focusFirstField();
}


// -----------------------------------------------------------------------------------------------------------
// Met à jour la procédure STAR courante et régénère l'aperçu résolu.
void MainWindow::onStarProcedureEdited()
{
    if (!mCurrentStarProcedureId.isValid())
        return;

    mProject.starProcedures().update(mCurrentStarProcedureId, mStarProcedureEditor->value());

    mStarProcedureModel->notifyRowChanged(mCurrentStarProcedureId.value());

    const RegenerationResult result = Regenerator().regenerate(mProject);

    mStarProcedureEditor->setPreviewLine(
        previewFor(
            EntityKind::StarProcedure,
            mCurrentStarProcedureId.value(),
            result
        )
    );
}


// ===========================================================================================================
// PROCEDURETRANSITION SID
// -----------------------------------------------------------------------------------------------------------
// Met à jour l'éditeur de transition SID à la sélection, avec aperçu régénéré.
void MainWindow::onSidProcedureTransitionSelectionChanged()
{
    const qint32 rawId = currentRawId(mSidProcedureTransitionTable, mSidProcedureTransitionModel, mSidProcedureTransitionProxy);
    if (rawId < 0) {
        mCurrentSidProcedureTransitionId = ProcedureTransitionId::invalid();
        mSidProcedureTransitionEditor->setEnabled(false);
        return;
    }
    mCurrentSidProcedureTransitionId = ProcedureTransitionId(rawId);
    if (const UserProcedureTransition* t =
            mProject.sidProcedureTransitions().find(mCurrentSidProcedureTransitionId)) {

        mSidProcedureTransitionEditor->setEnabled(true);

        mSidProcedureTransitionEditor->setValue(mCurrentSidProcedureTransitionId, *t);

        const RegenerationResult result = Regenerator().regenerate(mProject);

        mSidProcedureTransitionEditor->setPreviewLine(
            previewFor(
                EntityKind::SidProcedureTransition,
                mCurrentSidProcedureTransitionId.value(),
                result
            )
        );
    }
}

// -----------------------------------------------------------------------------------------------------------
// Ajoute une nouvelle transition SID et la sélectionne pour édition immédiate.
void MainWindow::onNewSidProcedureTransition()
{
    mProject.sidProcedureTransitions().add(UserProcedureTransition{});

    mSidProcedureTransitionModel->reload();

    selectSourceRow(
        mSidProcedureTransitionTable,
        mSidProcedureTransitionModel,
        mSidProcedureTransitionProxy,
        mSidProcedureTransitionModel->rowCount() - 1
    );

    mSidProcedureTransitionEditor->focusFirstField();
}


// -----------------------------------------------------------------------------------------------------------
// Met à jour la transition SID courante et régénère l'aperçu résolu.
void MainWindow::onSidProcedureTransitionEdited()
{
    if (!mCurrentSidProcedureTransitionId.isValid())
        return;

    mProject.sidProcedureTransitions().update(
        mCurrentSidProcedureTransitionId,
        mSidProcedureTransitionEditor->value()
    );

    mSidProcedureTransitionModel->notifyRowChanged(mCurrentSidProcedureTransitionId.value());

    const RegenerationResult result = Regenerator().regenerate(mProject);

    mSidProcedureTransitionEditor->setPreviewLine(
        previewFor(EntityKind::SidProcedureTransition,
        mCurrentSidProcedureTransitionId.value(),
        result)
    );
}


// ===========================================================================================================
// PROCEDURETRANSITION STAR
// -----------------------------------------------------------------------------------------------------------
// Met à jour l'éditeur de transition STAR à la sélection, avec aperçu régénéré.
void MainWindow::onStarProcedureTransitionSelectionChanged()
{
    const qint32 rawId = currentRawId(mStarProcedureTransitionTable, mStarProcedureTransitionModel, mStarProcedureTransitionProxy);

    if (rawId < 0) {
        mCurrentStarProcedureTransitionId = ProcedureTransitionId::invalid();
        mStarProcedureTransitionEditor->setEnabled(false);
        return;
    }

    mCurrentStarProcedureTransitionId = ProcedureTransitionId(rawId);

    if (const UserProcedureTransition* t =
            mProject.starProcedureTransitions().find(mCurrentStarProcedureTransitionId)) {

        mStarProcedureTransitionEditor->setEnabled(true);

        mStarProcedureTransitionEditor->setValue(mCurrentStarProcedureTransitionId, *t);

        const RegenerationResult result = Regenerator().regenerate(mProject);

        mStarProcedureTransitionEditor->setPreviewLine(
                previewFor(EntityKind::StarProcedureTransition,
                mCurrentStarProcedureTransitionId.value(),
                result)
        );
    }
}

// -----------------------------------------------------------------------------------------------------------
// Ajoute une nouvelle transition STAR et la sélectionne pour édition immédiate.
void MainWindow::onNewStarProcedureTransition()
{
    mProject.starProcedureTransitions().add(UserProcedureTransition{});

    mStarProcedureTransitionModel->reload();

    selectSourceRow(
        mStarProcedureTransitionTable,
        mStarProcedureTransitionModel,
        mStarProcedureTransitionProxy,
        mStarProcedureTransitionModel->rowCount() - 1
    );

    mStarProcedureTransitionEditor->focusFirstField();
}


// -----------------------------------------------------------------------------------------------------------
// Met à jour la transition STAR courante et régénère l'aperçu résolu.
void MainWindow::onStarProcedureTransitionEdited()
{
    if (!mCurrentStarProcedureTransitionId.isValid())
        return;

    mProject.starProcedureTransitions().update(
        mCurrentStarProcedureTransitionId,
        mStarProcedureTransitionEditor->value()
    );

    mStarProcedureTransitionModel->notifyRowChanged(mCurrentStarProcedureTransitionId.value());

    const RegenerationResult result = Regenerator().regenerate(mProject);

    mStarProcedureTransitionEditor->setPreviewLine(
        previewFor(EntityKind::StarProcedureTransition,
        mCurrentStarProcedureTransitionId.value(), result)
    );
}


// ===========================================================================================================
// RUNWAYPROCEDURETRANSITION SID
// -----------------------------------------------------------------------------------------------------------
// Met à jour l'éditeur de transition de piste SID à la sélection, avec aperçu.
void MainWindow::onSidRunwayProcedureTransitionSelectionChanged()
{
    const qint32 rawId = currentRawId(mSidRunwayProcedureTransitionTable, mSidRunwayProcedureTransitionModel,
                                      mSidRunwayProcedureTransitionProxy);
    if (rawId < 0) {
        mCurrentSidRunwayProcedureTransitionId = RunwayProcedureTransitionId::invalid();
        mSidRunwayProcedureTransitionEditor->setEnabled(false);
        return;
    }
    mCurrentSidRunwayProcedureTransitionId = RunwayProcedureTransitionId(rawId);
    if (const UserRunwayProcedureTransition* t =
            mProject.sidRunwayProcedureTransitions().find(mCurrentSidRunwayProcedureTransitionId)) {

                mSidRunwayProcedureTransitionEditor->setEnabled(true);
                mSidRunwayProcedureTransitionEditor->setValue(mCurrentSidRunwayProcedureTransitionId, *t);
                const RegenerationResult result = Regenerator().regenerate(mProject);
                mSidRunwayProcedureTransitionEditor->setPreviewLine(
                    previewFor(
                        EntityKind::SidRunwayProcedureTransition,
                        mCurrentSidRunwayProcedureTransitionId.value(),
                        result
                    )
                );
    }
}

// -----------------------------------------------------------------------------------------------------------
// Ajoute une nouvelle transition de piste SID et la sélectionne pour édition.
void MainWindow::onNewSidRunwayProcedureTransition()
{
    mProject.sidRunwayProcedureTransitions().add(UserRunwayProcedureTransition{});
    mSidRunwayProcedureTransitionModel->reload();

    selectSourceRow(
        mSidRunwayProcedureTransitionTable,
        mSidRunwayProcedureTransitionModel,
        mSidRunwayProcedureTransitionProxy,
        mSidRunwayProcedureTransitionModel->rowCount() - 1
    );

    mSidRunwayProcedureTransitionEditor->focusFirstField();
}

// -----------------------------------------------------------------------------------------------------------
// Met à jour la transition de piste SID courante et régénère l'aperçu résolu.
void MainWindow::onSidRunwayProcedureTransitionEdited()
{
    if (!mCurrentSidRunwayProcedureTransitionId.isValid())
        return;
    mProject.sidRunwayProcedureTransitions().update(
        mCurrentSidRunwayProcedureTransitionId,
        mSidRunwayProcedureTransitionEditor->value()
    );
    mSidRunwayProcedureTransitionModel->notifyRowChanged(mCurrentSidRunwayProcedureTransitionId.value());
    const RegenerationResult result = Regenerator().regenerate(mProject);
    mSidRunwayProcedureTransitionEditor->setPreviewLine(
        previewFor(
            EntityKind::SidRunwayProcedureTransition,
            mCurrentSidRunwayProcedureTransitionId.value(),
            result
        )
    );
}


// ===========================================================================================================
// RUNWAYPROCEDURETRANSITION STAR
// -----------------------------------------------------------------------------------------------------------
// Met à jour l'éditeur de transition de piste STAR à la sélection, avec aperçu.
void MainWindow::onStarRunwayProcedureTransitionSelectionChanged()
{
    const qint32 rawId = currentRawId(
        mStarRunwayProcedureTransitionTable,
        mStarRunwayProcedureTransitionModel,
        mStarRunwayProcedureTransitionProxy
    );
    if (rawId < 0) {
        mCurrentStarRunwayProcedureTransitionId = RunwayProcedureTransitionId::invalid();
        mStarRunwayProcedureTransitionEditor->setEnabled(false);
        return;
    }
    mCurrentStarRunwayProcedureTransitionId = RunwayProcedureTransitionId(rawId);
    if (const UserRunwayProcedureTransition* t =
            mProject.starRunwayProcedureTransitions().find(mCurrentStarRunwayProcedureTransitionId)) {

        mStarRunwayProcedureTransitionEditor->setEnabled(true);
        mStarRunwayProcedureTransitionEditor->setValue(mCurrentStarRunwayProcedureTransitionId, *t);
        const RegenerationResult result = Regenerator().regenerate(mProject);

        mStarRunwayProcedureTransitionEditor->setPreviewLine(
            previewFor(
                EntityKind::StarRunwayProcedureTransition,
                mCurrentStarRunwayProcedureTransitionId.value(),
                result
            )
        );
    }
}


// -----------------------------------------------------------------------------------------------------------
// Ajoute une nouvelle transition de piste STAR et la sélectionne pour édition.
void MainWindow::onNewStarRunwayProcedureTransition()
{
    mProject.starRunwayProcedureTransitions().add(UserRunwayProcedureTransition{});
    mStarRunwayProcedureTransitionModel->reload();

    selectSourceRow(
        mStarRunwayProcedureTransitionTable,
        mStarRunwayProcedureTransitionModel,
        mStarRunwayProcedureTransitionProxy,
        mStarRunwayProcedureTransitionModel->rowCount() - 1
    );

    mStarRunwayProcedureTransitionEditor->focusFirstField();
}

// -----------------------------------------------------------------------------------------------------------
// Met à jour la transition de piste STAR courante et régénère l'aperçu résolu.
void MainWindow::onStarRunwayProcedureTransitionEdited()
{
    if (!mCurrentStarRunwayProcedureTransitionId.isValid())
        return;

    mProject.starRunwayProcedureTransitions().update(
                    mCurrentStarRunwayProcedureTransitionId,
                    mStarRunwayProcedureTransitionEditor->value()
    );

    mStarRunwayProcedureTransitionModel->notifyRowChanged(mCurrentStarRunwayProcedureTransitionId.value());
    const RegenerationResult result = Regenerator().regenerate(mProject);

    mStarRunwayProcedureTransitionEditor->setPreviewLine(
                previewFor(EntityKind::StarRunwayProcedureTransition,
                mCurrentStarRunwayProcedureTransitionId.value(),
                result)
    );
}


// ===========================================================================================================
// Gestion de projet — inchangée, sauf reload() de tous les modèles au lieu
// d'un seul.
// -----------------------------------------------------------------------------------------------------------
// Charge un projet depuis la base et réinitialise toute l'interface.
void MainWindow::loadProjectIntoUi(qint64 id, const QString& name)
{
    QString err;
    const std::optional<UserProject> loaded = mStore.loadProject(id, &err);
    if (!loaded) {
        QMessageBox::critical(
            this,
            QStringLiteral("Erreur"),
            QStringLiteral("Chargement du projet impossible : %1").arg(err)
        );
        return;
    }

    mProject = *loaded;
    mCurrentProjectId = id;
    mSaveAction->setEnabled(true);
    mReloadWorldAction->setEnabled(true);
    mExportAction->setEnabled(true);
    mDecodeWorldAction->setEnabled(true);
    mIntegrateWorldAction->setEnabled(true);
    setWindowTitle(QStringLiteral("FF777 NavStudio — %1").arg(name));

    mCurrentPointId = PointId::invalid();
    mCurrentWaypointId = WaypointId::invalid();
    mCurrentAirportId = AirportId::invalid();
    mCurrentRunwayId = RunwayId::invalid();
    mCurrentNavaidId = NavaidId::invalid();
    mCurrentLegSequenceId = LegSequenceId::invalid();
    mCurrentLegId = LegId::invalid();
    mCurrentApproachId = ApproachId::invalid();
    mCurrentApproachTransitionId = ApproachTransitionId::invalid();
    mCurrentSidProcedureId = ProcedureId::invalid();
    mCurrentStarProcedureId = ProcedureId::invalid();
    mCurrentSidProcedureTransitionId = ProcedureTransitionId::invalid();
    mCurrentStarProcedureTransitionId = ProcedureTransitionId::invalid();
    mCurrentSidRunwayProcedureTransitionId = RunwayProcedureTransitionId::invalid();
    mCurrentStarRunwayProcedureTransitionId = RunwayProcedureTransitionId::invalid();

    mPointEditor->setEnabled(false);
    mWaypointEditor->setEnabled(false);
    mAirportEditor->setEnabled(false);
    mRunwayEditor->setEnabled(false);
    mNavaidEditor->setEnabled(false);
    mLegSequenceEditor->setEnabled(false);
    mLegEditor->setEnabled(false);
    mApproachEditor->setEnabled(false);
    mApproachTransitionEditor->setEnabled(false);
    mSidProcedureEditor->setEnabled(false);
    mStarProcedureEditor->setEnabled(false);
    mSidProcedureTransitionEditor->setEnabled(false);
    mStarProcedureTransitionEditor->setEnabled(false);
    mSidRunwayProcedureTransitionEditor->setEnabled(false);
    mStarRunwayProcedureTransitionEditor->setEnabled(false);

    mPointModel->reload();
    mWaypointModel->reload();
    mAirportModel->reload();
    mRunwayModel->reload();
    mNavaidModel->reload();
    mLegSequenceModel->reload();
    mLegModel->reload();
    mApproachModel->reload();
    mApproachTransitionModel->reload();
    mSidProcedureModel->reload();
    mStarProcedureModel->reload();
    mSidProcedureTransitionModel->reload();
    mStarProcedureTransitionModel->reload();
    mSidRunwayProcedureTransitionModel->reload();
    mStarRunwayProcedureTransitionModel->reload();
}

// -----------------------------------------------------------------------------------------------------------
// Ouvre le dialogue d'extraction d'aéroport ; le signal projectCreated()
// recharge l'éditeur comme un projet normal quand un nouveau projet a été
// créé à partir des données extraites.
void MainWindow::onExtractAirport()
{
    AirportExtractDialog dialog(mStore, this);
    connect(&dialog, &AirportExtractDialog::projectCreated, this, &MainWindow::loadProjectIntoUi);
    dialog.exec();
}

// -----------------------------------------------------------------------------------------------------------
// Ouvre le dialogue de création, lit un éventuel fichier mondial et crée le projet.
void MainWindow::onNewProject()
{
    bool ok = false;
    const QString name = QInputDialog::getText(
                            this,
                            QStringLiteral("Nouveau projet"),
                            QStringLiteral("Nom du projet (ex. ident de l'aéroport fictif) :"),
                            QLineEdit::Normal, QString(), &ok).trimmed();

    if (!ok || name.isEmpty())
        return;

    for (const ProjectSummary& p : mStore.listProjects()) {
        if (p.name == name) {
            QMessageBox::warning(
                    this,
                    QStringLiteral("Nouveau projet"),
                    QStringLiteral("Un projet nommé \"%1\" existe déjà.").arg(name)
            );
            return;
        }
    }

    StartingIndices indices;
    const QString worldFile = QFileDialog::getOpenFileName(
        this, QStringLiteral("Fichier mondial (optionnel — Annuler pour démarrer à l'index 1)"),
        navstud::tools::Nav1DbPipeline::workingDir());
    if (!worldFile.isEmpty()) {
        const WorldIndexReader reader;
        const WorldIndexResult result = reader.readStartingIndices(worldFile);
        if (!result.success) {
            QMessageBox::warning(
                    this,
                    QStringLiteral("Fichier mondial"),
                    QStringLiteral("Lecture impossible (%1) — démarrage à 1 pour toutes les catégories.")
                                            .arg(result.errorMessage)
            );
        } else {
            indices = result.indices;
            if (!result.missingSections.isEmpty()) {
                QMessageBox::warning(
                    this,
                    QStringLiteral("Fichier mondial"),
                    QStringLiteral("Sections non trouvées (valeur 1 conservée) : %1")
                                            .arg(result.missingSections.join(QStringLiteral(", ")))
                );
            }
        }
    }

    QString err;
    const qint64 id = mStore.createProject(name, indices, &err);
    if (id < 0) {
        QMessageBox::critical(
                this,
                QStringLiteral("Erreur"),
                QStringLiteral("Création du projet impossible : %1").arg(err)
        );
        return;
    }

    loadProjectIntoUi(id, name);
}

// -----------------------------------------------------------------------------------------------------------
// Liste les projets enregistrés et charge celui choisi par l'utilisateur.
void MainWindow::onOpenProject()
{
    const QVector<ProjectSummary> projects = mStore.listProjects();
    if (projects.isEmpty()) {
        QMessageBox::information(
                this,
                QStringLiteral("Ouvrir un projet"),
                QStringLiteral("Aucun projet enregistré pour l'instant.")
        );
        return;
    }

    QStringList labels;
    for (const ProjectSummary& p : projects)
        labels << QStringLiteral("%1 (modifié le %2)").arg(p.name, p.updatedAt);

    bool ok = false;
    const QString chosen =  QInputDialog::getItem(
                                    this,
                                    QStringLiteral("Ouvrir un projet"),
                                    QStringLiteral("Projet :"),
                                    labels, 0, false, &ok
                            );
    if (!ok)
        return;

    const int idx = labels.indexOf(chosen);
    if (idx < 0)
        return;

    loadProjectIntoUi(projects.at(idx).id, projects.at(idx).name);
}

// -----------------------------------------------------------------------------------------------------------
// Enregistre le projet courant dans la base de données.
void MainWindow::onSaveProject()
{
    if (mCurrentProjectId < 0)
        return;

    QString err;
    if (!mStore.saveProject(mCurrentProjectId, mProject, &err))
        QMessageBox::critical(
            this,
            QStringLiteral("Erreur"),
            QStringLiteral("Sauvegarde impossible : %1").arg(err)
        );
    else
        statusBar()->showMessage(QStringLiteral("Projet enregistré."), 3000);
}

// -----------------------------------------------------------------------------------------------------------
// Demande un nouveau fichier mondial puis réaligne le projet dessus.
void MainWindow::onReloadWorldFile()
{
    if (mCurrentProjectId < 0)
        return;

    const QString worldFile =
                QFileDialog::getOpenFileName(
                    this,
                    QStringLiteral("Nouveau fichier mondial (mise à jour mensuelle)"),
                    navstud::tools::Nav1DbPipeline::workingDir()
                );
    if (worldFile.isEmpty())
        return;

    applyWorldFile(worldFile);
}

// -----------------------------------------------------------------------------------------------------------
// Renumérote le projet à partir des compteurs lus dans le fichier mondial.
bool MainWindow::applyWorldFile(const QString& worldFile)
{
    const WorldIndexReader reader;
    const WorldIndexResult result = reader.readStartingIndices(worldFile);
    if (!result.success) {
        QMessageBox::warning(
            this,
            QStringLiteral("Fichier mondial"),
            QStringLiteral("Lecture impossible (%1).").arg(result.errorMessage)
        );
        return false;
    }
    if (!result.missingSections.isEmpty()) {
        QMessageBox::warning(
            this,
            QStringLiteral("Fichier mondial"),
            QStringLiteral("Sections non trouvées (non modifiées pour ces catégories) : %1")
                                     .arg(result.missingSections.join(QStringLiteral(", ")))
        );
    }

    const auto reply = QMessageBox::question(
        this,
        QStringLiteral("Recharger le fichier mondial 'Nav1.txt'"),
        QStringLiteral("Tous les id du projet vont être renumérotés pour repartir des nouveaux compteurs "
                       "(\"# Count:\") lus dans ce fichier — le contenu de chaque ligne n'est pas modifié, "
                       "seul son numéro change. Le projet sera enregistré immédiatement après. Continuer ?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No
    );

    if (reply != QMessageBox::Yes)
        return false;

    mProject.renumberFrom(result.indices);

    QString err;
    if (!mStore.updateStartingIndices(mCurrentProjectId, result.indices, &err)) {
        QMessageBox::warning(
            this,
            QStringLiteral("Fichier mondial"),
            QStringLiteral("Renumérotation effectuée en mémoire, mais échec de la mise"
                           " à jour des compteurs de départ en base : %1").arg(err));
    }
    if (!mStore.saveProject(mCurrentProjectId, mProject, &err)) {
        QMessageBox::critical(
            this,
            QStringLiteral("Erreur"),
            QStringLiteral("Renumérotation effectuée en mémoire, mais échec de la sauvegarde : %1").arg(err)
        );
        return false;
    }

    // Toute sélection en cours référence un id qui n'existe plus tel quel —
    // même geste de remise à zéro que loadProjectIntoUi().
    mCurrentPointId = PointId::invalid();
    mCurrentWaypointId = WaypointId::invalid();
    mCurrentAirportId = AirportId::invalid();
    mCurrentRunwayId = RunwayId::invalid();
    mCurrentNavaidId = NavaidId::invalid();
    mCurrentLegSequenceId = LegSequenceId::invalid();
    mCurrentLegId = LegId::invalid();
    mCurrentApproachId = ApproachId::invalid();
    mCurrentApproachTransitionId = ApproachTransitionId::invalid();
    mCurrentSidProcedureId = ProcedureId::invalid();
    mCurrentStarProcedureId = ProcedureId::invalid();
    mCurrentSidProcedureTransitionId = ProcedureTransitionId::invalid();
    mCurrentStarProcedureTransitionId = ProcedureTransitionId::invalid();
    mCurrentSidRunwayProcedureTransitionId = RunwayProcedureTransitionId::invalid();
    mCurrentStarRunwayProcedureTransitionId = RunwayProcedureTransitionId::invalid();

    mPointEditor->setEnabled(false);
    mWaypointEditor->setEnabled(false);
    mAirportEditor->setEnabled(false);
    mRunwayEditor->setEnabled(false);
    mNavaidEditor->setEnabled(false);
    mLegSequenceEditor->setEnabled(false);
    mLegEditor->setEnabled(false);
    mApproachEditor->setEnabled(false);
    mApproachTransitionEditor->setEnabled(false);
    mSidProcedureEditor->setEnabled(false);
    mStarProcedureEditor->setEnabled(false);
    mSidProcedureTransitionEditor->setEnabled(false);
    mStarProcedureTransitionEditor->setEnabled(false);
    mSidRunwayProcedureTransitionEditor->setEnabled(false);
    mStarRunwayProcedureTransitionEditor->setEnabled(false);

    mPointModel->reload();
    mWaypointModel->reload();
    mAirportModel->reload();
    mRunwayModel->reload();
    mNavaidModel->reload();
    mLegSequenceModel->reload();
    mLegModel->reload();
    mApproachModel->reload();
    mApproachTransitionModel->reload();
    mSidProcedureModel->reload();
    mStarProcedureModel->reload();
    mSidProcedureTransitionModel->reload();
    mStarProcedureTransitionModel->reload();
    mSidRunwayProcedureTransitionModel->reload();
    mStarRunwayProcedureTransitionModel->reload();

    statusBar()->showMessage(QStringLiteral("Fichier mondial rechargé — projet renuméroté et enregistré."), 5000);
    return true;
}

// -----------------------------------------------------------------------------------------------------------
// Régénère le projet puis écrit les 15 fichiers d'export dans le dossier de l'app.
void MainWindow::onExportFiles()
{
    if (mCurrentProjectId < 0)
        return;

    using namespace navstud::tools;

    // Écrit désormais les 15 fichiers dans le dossier où tourne l'application.
    const QString outputDir = Nav1DbPipeline::workingDir();

    const RegenerationResult result = Regenerator().regenerate(mProject);
    if (!result.isValid()) {
        const auto reply = QMessageBox::warning(
            this, QStringLiteral("Alertes en cours"),
            QStringLiteral("%1 ligne(s) non résolue(s) ou en erreur — elles seront simplement absentes des "
                            "fichiers générés. Exporter quand même ?")
                .arg(result.alertCount()),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (reply != QMessageBox::Yes)
            return;
    }

    const NavDataWriter writer;
    const QVector<NavDataWriter::FileResult> results = writer.writeAll(result.repository, QDir(outputDir));

    QStringList failures;
    int totalLines = 0;
    for (const NavDataWriter::FileResult& fr : results) {
        if (fr.success)
            totalLines += fr.linesWritten;
        else
            failures << QStringLiteral("%1 : %2").arg(fr.fileName, fr.errorMessage);
    }

    if (!failures.isEmpty()) {
        QMessageBox::critical(
            this,
            QStringLiteral("Erreur d'export"),
            QStringLiteral("Échec sur %1 fichier(s) :\n%2").arg(failures.size()).arg(failures.join(QStringLiteral("\n")))
        );
        return;
    }

    statusBar()->showMessage(
        QStringLiteral("%1 fichiers écrits dans %2 (%3 lignes au total).").arg(results.size()).arg(outputDir).arg(totalLines), 6000);
}

namespace {

// -----------------------------------------------------------------------------------------------------------
// Lit les lignes non vides d'un fichier d'import texte.
// Lit un fichier d'import (un enregistrement par ligne, champs séparés par
// '|', généré par le script d'extraction Excel) — lignes vides ignorées.
QStringList readImportLines(const QString& dir, const QString& fileName)
{
    QStringList lines;
    QFile file(dir + QLatin1Char('/') + fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return lines;
    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    while (!in.atEnd()) {
        const QString line = in.readLine();
        if (!line.trimmed().isEmpty())
            lines << line;
    }
    return lines;
}

} // namespace

// -----------------------------------------------------------------------------------------------------------
// ONIMPORTFROMTEXTFILES
// LA MÉTHODE EST MAINTENUE POUR LE CAS OÙ, MAIS LE MENU LIÉ EST DÉSACTIVÉ POUR ÉVITER D'ÉCRASER LA BDD
// Elle importe en une fois un projet (LFFA) à partir des 15 fichiers texte pré-extraits (un par structure).
// Ça n'est PAS une fonctionnalité d'import récurrente : conçue pour peupler un projet une seule fois à partir
// de données existantes (ex. classeur Excel), avec des id EXPLICITES déjà alignés sur la continuité mondiale
// (pas besoin de StartingIndices ici, cf. UserProject.h).
// Format de chaque ligne, champs séparés par '|', id en premier :
//   points.txt                id|ident|lat|lon|magVar|holdCourse|holdDist|holdTime|holdSide
//   waypoints.txt             id|pointIdent
//   airports.txt              id|pointIdent|elevation|speedLimit|altLimit|transAlt|transLevel
//   runways.txt               id|airportIdent|thresholdIdent|elevation|gradient|course|length|displaced|stopway|cross
//   navaids.txt               id|type|pointIdent|associatedNavaidIdent|elevation|declination|frequency|category|course
//                             |angle|runwayIdent
//   legsequences.txt          id|ident|ilsOrRnav|procedureKind|altitudeLevelTransInFeet
//   legs.txt                  id|legSequenceIdent|navaidIdent|pointIdent|wpDescription|codePath|course|distance|navaidCourse
//                             |navaidDistance|altMin|altMax|airSpeed|path|turnDir|rnp
//   approaches.txt            id|runwayIdent|legSequenceIdent|decisionHeightInFeet|minimumDescentInFeet
//   approachtransitions.txt   id|approachIdent|legSequenceIdent
//   procedures_sid/star.txt   id|airportIdent|legSequenceIdent
//   proctrans_sid/star.txt    id|procedureIdent|legSequenceIdent
//   runproctrans_sid/star.txt id|runwayIdent|procedureIdent|legSequenceIdent
void MainWindow::onImportFromTextFiles()
{
    const QString name = QInputDialog::getText(this, QStringLiteral("Nouveau projet importé"), QStringLiteral("Nom du projet :"));
    if (name.isEmpty())
        return;

    const QString dir = QFileDialog::getExistingDirectory(
        this, QStringLiteral("Dossier contenant les 15 fichiers d'import (points.txt, legs.txt, ...)"),
        navstud::tools::Nav1DbPipeline::workingDir());
    if (dir.isEmpty())
        return;

    UserProject project; // StartingIndices par défaut : sans importance, tous les id sont explicites ici
    int totalLines = 0;

    for (const QString& line : readImportLines(dir, QStringLiteral("points.txt"))) {
        const QStringList f = line.split(QLatin1Char('|'));
        UserPoint p;
        p.ident = f.at(1);
        p.latitude = f.at(2).toDouble();
        p.longitude = f.at(3).toDouble();
        p.magVar = f.at(4).toDouble();
        p.holdCourse = f.at(5).toDouble();
        p.holdDistInMeters = f.at(6).toDouble();
        p.holdTime = f.at(7).toDouble();
        p.holdSide = static_cast<qint8>(f.at(8).toInt());
        project.points().add(p, PointId(f.at(0).toInt()));
        ++totalLines;
    }

    for (const QString& line : readImportLines(dir, QStringLiteral("waypoints.txt"))) {
        const QStringList f = line.split(QLatin1Char('|'));
        UserWaypoint wp;
        wp.pointIdent = f.at(1);
        project.waypoints().add(wp, WaypointId(f.at(0).toInt()));
        ++totalLines;
    }

    for (const QString& line : readImportLines(dir, QStringLiteral("airports.txt"))) {
        const QStringList f = line.split(QLatin1Char('|'));
        UserAirport a;
        a.pointIdent = f.at(1);
        a.elevationInMeters = f.at(2).toDouble();
        a.limitSpeedInMetersPerSec = f.at(3).toDouble();
        a.limitAltitudeInMeters = f.at(4).toDouble();
        a.transitionAltitudeInMeters = f.at(5).toDouble();
        a.transitionLevelInMeters = f.at(6).toDouble();
        project.airports().add(a, AirportId(f.at(0).toInt()));
        ++totalLines;
    }

    for (const QString& line : readImportLines(dir, QStringLiteral("runways.txt"))) {
        const QStringList f = line.split(QLatin1Char('|'));
        UserRunway r;
        r.airportIdent = f.at(1);
        r.thresholdIdent = f.at(2);
        r.elevationInMeters = f.at(3).toDouble();
        r.gradient = f.at(4).toDouble();
        r.course = f.at(5).toDouble();
        r.lengthInMeters = f.at(6).toDouble();
        r.displacedInMeters = f.at(7).toDouble();
        r.stopwayInMeters = f.at(8).toDouble();
        r.crossInMeters = f.at(9).toDouble();
        project.runways().add(r, RunwayId(f.at(0).toInt()));
        ++totalLines;
    }

    for (const QString& line : readImportLines(dir, QStringLiteral("navaids.txt"))) {
        const QStringList f = line.split(QLatin1Char('|'));
        UserNavaid n;
        n.type = f.at(1);
        n.pointIdent = f.at(2);
        n.associatedNavaidIdent = f.at(3);
        n.elevationInMeters = f.at(4).toDouble();
        n.declination = f.at(5).toDouble();
        n.frequencyMHzTimes100 = f.at(6).toUInt();
        n.category = f.at(7);
        n.course = f.at(8).toDouble();
        n.angle = f.at(9).toDouble();
        n.runwayIdent = f.at(10);
        project.navaids().add(n, NavaidId(f.at(0).toInt()));
        ++totalLines;
    }

    for (const QString& line : readImportLines(dir, QStringLiteral("legsequences.txt"))) {
        const QStringList f = line.split(QLatin1Char('|'));
        UserLegSequence ls;
        ls.ident = f.at(1);
        ls.ilsOrRnav = f.at(2);
        ls.procedureKind = f.at(3);
        ls.altitudeLevelTransInFeet = f.at(4).toDouble();
        project.legSequences().add(ls, LegSequenceId(f.at(0).toInt()));
        ++totalLines;
    }

    for (const QString& line : readImportLines(dir, QStringLiteral("legs.txt"))) {
        const QStringList f = line.split(QLatin1Char('|'));
        UserLeg l;
        l.legSequenceIdent = f.at(1);
        l.navaidIdent = f.at(2);
        l.pointIdent = f.at(3);
        l.wpDescription = f.at(4);
        l.codePath = f.at(5);
        l.course = f.at(6).toDouble();
        l.distanceInMeters = f.at(7).toDouble();
        l.navaidCourse = f.at(8).toDouble();
        l.navaidDistanceInMeters = f.at(9).toDouble();
        const double altMinMeters = f.at(10).toDouble();
        const double altMaxMeters = f.at(11).toDouble();
        l.altitudeLimitMinInFeet = (altMinMeters == -1.0) ? -1.0 : altMinMeters * 3.28084;
        l.altitudeLimitMaxInFeet = (altMaxMeters == -1.0) ? -1.0 : altMaxMeters * 3.28084;
        l.airSpeedLimit = f.at(12).toDouble();
        l.path = f.at(13).toDouble();
        l.turnDir = static_cast<qint8>(f.at(14).toInt());
        l.rnpInMeters = f.at(15).toDouble();
        project.legs().add(l, LegId(f.at(0).toInt()));
        ++totalLines;
    }

    for (const QString& line : readImportLines(dir, QStringLiteral("approaches.txt"))) {
        const QStringList f = line.split(QLatin1Char('|'));
        UserApproach a;
        a.runwayIdent = f.at(1);
        a.legSequenceIdent = f.at(2);
        a.decisionHeightInFeet = f.at(3).toDouble();
        a.minimumDescentInFeet = f.at(4).toDouble();
        project.approaches().add(a, ApproachId(f.at(0).toInt()));
        ++totalLines;
    }

    for (const QString& line : readImportLines(dir, QStringLiteral("approachtransitions.txt"))) {
        const QStringList f = line.split(QLatin1Char('|'));
        UserApproachTransition t;
        t.approachIdent = f.at(1);
        t.legSequenceIdent = f.at(2);
        project.approachTransitions().add(t, ApproachTransitionId(f.at(0).toInt()));
        ++totalLines;
    }

    const QVector<QPair<QString, EntityTable<ProcedureTag, UserProcedure>*>> procedureTargets = {
        { QStringLiteral("procedures_sid.txt"), &project.sidProcedures() },
        { QStringLiteral("procedures_star.txt"), &project.starProcedures() },
    };
    for (const auto& [fileName, table] : procedureTargets) {
        for (const QString& line : readImportLines(dir, fileName)) {
            const QStringList f = line.split(QLatin1Char('|'));
            UserProcedure p;
            p.airportIdent = f.at(1);
            p.legSequenceIdent = f.at(2);
            table->add(p, ProcedureId(f.at(0).toInt()));
            ++totalLines;
        }
    }

    const QVector<QPair<QString, EntityTable<ProcedureTransitionTag, UserProcedureTransition>*>> procTransTargets = {
        { QStringLiteral("proctrans_sid.txt"), &project.sidProcedureTransitions() },
        { QStringLiteral("proctrans_star.txt"), &project.starProcedureTransitions() },
    };
    for (const auto& [fileName, table] : procTransTargets) {
        for (const QString& line : readImportLines(dir, fileName)) {
            const QStringList f = line.split(QLatin1Char('|'));
            UserProcedureTransition t;
            t.procedureIdent = f.at(1);
            t.legSequenceIdent = f.at(2);
            table->add(t, ProcedureTransitionId(f.at(0).toInt()));
            ++totalLines;
        }
    }

    const QVector<QPair<QString, EntityTable<RunwayProcedureTransitionTag, UserRunwayProcedureTransition>*>> rptTargets = {
        { QStringLiteral("runproctrans_sid.txt"), &project.sidRunwayProcedureTransitions() },
        { QStringLiteral("runproctrans_star.txt"), &project.starRunwayProcedureTransitions() },
    };
    for (const auto& [fileName, table] : rptTargets) {
        for (const QString& line : readImportLines(dir, fileName)) {
            const QStringList f = line.split(QLatin1Char('|'));
            UserRunwayProcedureTransition t;
            t.runwayIdent = f.at(1);
            t.procedureIdent = f.at(2);
            t.legSequenceIdent = f.at(3);
            table->add(t, RunwayProcedureTransitionId(f.at(0).toInt()));
            ++totalLines;
        }
    }

    if (totalLines == 0) {
        QMessageBox::warning(this, QStringLiteral("Import"),
                              QStringLiteral("Aucune ligne lue — vérifie que le dossier contient bien les 15 fichiers "
                                             "attendus (points.txt, waypoints.txt, ...)."));
        return;
    }

    mProject = project; // ensures the freshly-built project also drives the save below
    QString err;

    // Un projet du même nom existe-t-il déjà ?
    // La contrainte UNIQUE sur projects.name ferait sinon échouer createProject() sèchement.
    // On propose d'écraser son contenu plutôt que de forcer un renommage.
    qint64 targetId = -1;
    for (const ProjectSummary& summary : mStore.listProjects()) {
        if (summary.name == name) {
            targetId = summary.id;
            break;
        }
    }

    if (targetId >= 0) {
        const auto reply = QMessageBox::question(
            this, QStringLiteral("Projet déjà existant"),
            QStringLiteral("Un projet nommé \"%1\" existe déjà — écraser entièrement son contenu avec les données "
                            "importées ?")
                .arg(name),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (reply != QMessageBox::Yes)
            return;
    } else {
        targetId = mStore.createProject(name, StartingIndices(), &err);
        if (targetId < 0) {
            QMessageBox::critical(this, QStringLiteral("Erreur"), QStringLiteral("Création du projet impossible : %1").arg(err));
            return;
        }
    }

    if (!mStore.saveProject(targetId, project, &err)) {
        QMessageBox::critical(this, QStringLiteral("Erreur"), QStringLiteral("Sauvegarde impossible : %1").arg(err));
        return;
    }

    loadProjectIntoUi(targetId, name); // recharge depuis la base (source de vérité), remplace mProject ci-dessus
    statusBar()->showMessage(QStringLiteral("Import terminé : %1 lignes réparties sur 15 structures.").arg(totalLines), 6000);

} // !onImportFromTextFiles


// -----------------------------------------------------------------------------------------------------------
// Décoder nav1.db -> nav1.txt, puis recharger ce fichier mondial.
// (opération 2 puis 3 de la chaîne)
void MainWindow::onDecodeWorldFile()
{
    if (mCurrentProjectId < 0)
        return;

    using namespace navstud::tools;

    if (QFile::exists(Nav1DbPipeline::nav1TxtPath()))
        QFile::remove(Nav1DbPipeline::nav1TxtPath());

    QString errorMessage;
    QString detail;
    if (!Nav1DbPipeline::decode(&errorMessage, &detail)) {
        QMessageBox::critical(this, QStringLiteral("Décodage nav1.db"),
                              QStringLiteral("Décodage impossible : %1").arg(errorMessage));
        return;
    }

    // Op 3 : réaligner les id du projet sur les nouveaux compteurs lus.
    if (!applyWorldFile(Nav1DbPipeline::nav1TxtPath()))
        return;

    statusBar()->showMessage(QStringLiteral("nav1.db décodé ; fichier mondial rechargé."), 6000);
}


// -----------------------------------------------------------------------------------------------------------
// Compléter nav1.txt puis réencoder et déployer nav1.db.
// (opérations 4 -> 5 -> 6 du workflow)
void MainWindow::onIntegrateWorldFile()
{
    if (mCurrentProjectId < 0)
        return;

    using namespace navstud::tools;

    QString errorMessage;
    QString detail;
    if (!Nav1DbPipeline::integrateEncodeDeploy(&errorMessage, &detail)) {
        QMessageBox::critical(this, QStringLiteral("Réencodage nav1.db"),
                              QStringLiteral("Chaîne d'intégration/re-encodage impossible : %1").arg(errorMessage));
        return;
    }

    statusBar()->showMessage(detail, 8000);
    QMessageBox::information(this, QStringLiteral("Réencodage terminé"), detail);
}

