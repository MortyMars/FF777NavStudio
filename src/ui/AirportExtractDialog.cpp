#include "AirportExtractDialog.h"

#include "ExtractedProjectBuilder.h"
#include "Nav1DbPipeline.h"
#include "WorldIndexReader.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStringListModel>
#include <QVBoxLayout>
#include <QtConcurrent>

#include <memory>

#include <functional>

AirportExtractDialog::AirportExtractDialog(navstud::persistence::ProjectStore& store, QWidget* parent)
    : QDialog(parent)
    , mStore(store)
{
    setWindowTitle(QStringLiteral("Extraire un aéroport (nav1.txt -> projet)"));
    setMinimumWidth(640);

    auto* rootLayout = new QVBoxLayout(this);

    // --- Fichier mondial source + chargement ---
    auto* sourceRow = new QHBoxLayout;
    mSourceEdit = new QLineEdit(this);
    mSourceEdit->setPlaceholderText(QStringLiteral("Chemin du fichier mondial nav1.txt (livré avec le B777 / pipeline Standard)"));
    mBrowseSourceButton = new QPushButton(QStringLiteral("Parcourir..."), this);
    mLoadButton = new QPushButton(QStringLiteral("Charger"), this);
    mLoadButton->setEnabled(false);
    sourceRow->addWidget(mSourceEdit, 1);
    sourceRow->addWidget(mBrowseSourceButton);
    sourceRow->addWidget(mLoadButton);

    // --- Aéroport + fichier extrait ---
    mAirportCombo = new QComboBox(this);
    mAirportCombo->setEditable(true);
    mAirportCombo->setInsertPolicy(QComboBox::NoInsert);
    mAirportCombo->setMinimumContentsLength(12);
    mAirportCombo->setEnabled(false);
    mAirportCombo->setToolTip(QStringLiteral("Saisissez un code aéroport (ex. KJFK) — la liste filtre au fur et à mesure."));
    mAirportCombo->lineEdit()->setPlaceholderText(QStringLiteral("Charger le fichier mondial d'abord"));

    auto* outputRow = new QHBoxLayout;
    mOutputEdit = new QLineEdit(this);
    mOutputEdit->setPlaceholderText(QStringLiteral("Chemin du fichier extrait (proposé automatiquement)"));
    mBrowseOutputButton = new QPushButton(QStringLiteral("Parcourir..."), this);
    mBrowseOutputButton->setEnabled(false);
    outputRow->addWidget(mOutputEdit, 1);
    outputRow->addWidget(mBrowseOutputButton);

    auto* formLayout = new QFormLayout;
    formLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    formLayout->addRow(QStringLiteral("Aéroport :"), mAirportCombo);
    formLayout->addRow(QStringLiteral("Fichier extrait :"), outputRow);

    mCreateProjectCheck = new QCheckBox(QStringLiteral("Ajouter le projet dans la base de données existante "
                                                   "(projects.sqlite) après l'extraction"), this);
    mCreateProjectCheck->setChecked(true);
    mCreateProjectCheck->setEnabled(false);

    mStatusLabel = new QLabel(QStringLiteral("En attente du fichier mondial."), this);
    mStatusLabel->setWordWrap(true);

    mLogEdit = new QPlainTextEdit(this);
    mLogEdit->setReadOnly(true);
    mLogEdit->setMaximumHeight(150);

    auto* buttonRow = new QHBoxLayout;
    mExtractButton = new QPushButton(QStringLiteral("Extraire"), this);
    mExtractButton->setEnabled(false);
    mCloseButton = new QPushButton(QStringLiteral("Fermer"), this);
    buttonRow->addWidget(mExtractButton);
    buttonRow->addStretch(1);
    buttonRow->addWidget(mCloseButton);

    rootLayout->addLayout(sourceRow);
    rootLayout->addLayout(formLayout);
    rootLayout->addWidget(mCreateProjectCheck);
    rootLayout->addWidget(mStatusLabel);
    rootLayout->addWidget(mLogEdit);
    rootLayout->addLayout(buttonRow);

    // --- Connexions ---
    connect(mBrowseSourceButton, &QPushButton::clicked, this, &AirportExtractDialog::browseSourceFile);
    connect(mBrowseOutputButton, &QPushButton::clicked, this, &AirportExtractDialog::browseOutputFile);
    connect(mLoadButton, &QPushButton::clicked, this, &AirportExtractDialog::loadSource);
    connect(mAirportCombo, &QComboBox::currentTextChanged, this, &AirportExtractDialog::onAirportChanged);
    connect(mExtractButton, &QPushButton::clicked, this, &AirportExtractDialog::runExtract);
    connect(mCloseButton, &QPushButton::clicked, this, &AirportExtractDialog::reject);
    connect(mSourceEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
        mLoadButton->setEnabled(!text.trimmed().isEmpty());
    });

    connect(&mLoadWatcher, &QFutureWatcher<LoadOutcome>::finished, this, &AirportExtractDialog::onLoadFinished);
    connect(&mBuildWatcher, &QFutureWatcher<BuildOutcome>::finished, this, &AirportExtractDialog::onBuildFinished);
}

AirportExtractDialog::~AirportExtractDialog() = default;

// -----------------------------------------------------------------------------------------------------------
void AirportExtractDialog::browseSourceFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Fichier mondial nav1.txt"), navstud::tools::Nav1DbPipeline::workingDir(),
        QStringLiteral("NavData Text (*.txt);;Tous les fichiers (*)"));
    if (!path.isEmpty())
        mSourceEdit->setText(path);
}

// -----------------------------------------------------------------------------------------------------------
void AirportExtractDialog::browseOutputFile()
{
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Fichier extrait"), mOutputEdit->text(),
        QStringLiteral("NavData Text (*.txt);;Tous les fichiers (*)"));
    if (!path.isEmpty())
        mOutputEdit->setText(path);
}

// -----------------------------------------------------------------------------------------------------------
void AirportExtractDialog::loadSource()
{
    if (mLoadWatcher.isRunning())
        return;

    const QString path = mSourceEdit->text().trimmed();
    if (path.isEmpty()) {
        appendLog(QStringLiteral("Chemin du fichier mondial vide."));
        return;
    }

    setBusy(true);
    mDb.reset();
    mAirportCombo->clear();
    mAirportCombo->setEnabled(false);
    mOutputEdit->clear();
    mCreateProjectCheck->setEnabled(false);
    mExtractButton->setEnabled(false);
    mStatusLabel->setText(QStringLiteral("Lecture du fichier mondial (plusieurs secondes sur ~200 Mo)..."));
    QApplication::setOverrideCursor(Qt::WaitCursor);

    // Analyse du fichier en arrière-plan. NavDataBase possède tout le tableau
    // en mémoire (QByteArray) ; seul son pointeur franchit la barrière.
    mLoadWatcher.setFuture(QtConcurrent::run([path]() -> LoadOutcome {
        LoadOutcome outcome;
        auto db = std::make_shared<navstud::extract::NavDataBase>();
        QString error;
        if (!db->load(path, &error)) {
            outcome.error = error;
            return outcome;
        }
        outcome.db = std::move(db);
        return outcome;
    }));
}

// -----------------------------------------------------------------------------------------------------------
QString AirportExtractDialog::currentIcao() const
{
    return mAirportCombo->currentText().trimmed().toUpper();
}

// -----------------------------------------------------------------------------------------------------------
void AirportExtractDialog::onAirportChanged()
{
    const QString icao = currentIcao();

    const QFileInfo sourceInfo(mSourceEdit->text().trimmed());
    if (mDb && !icao.isEmpty() && sourceInfo.dir().exists()) {
        const QString suggested = sourceInfo.dir().filePath(icao + QStringLiteral("-extract.txt"));
        const QString previousAuto = mOutputEdit->property("lastSuggestion").toString();
        if (previousAuto.isEmpty() || mOutputEdit->text().trimmed().isEmpty()
            || mOutputEdit->text().trimmed() == previousAuto)
            mOutputEdit->setText(suggested);
        mOutputEdit->setProperty("lastSuggestion", suggested);
    }

    if (!mDb) {
        mStatusLabel->setText(QStringLiteral("Charger le fichier mondial d'abord."));
        mExtractButton->setEnabled(false);
        return;
    }
    if (icao.isEmpty()) {
        mStatusLabel->setText(QStringLiteral("Saisissez un code aéroport (ex. KJFK)."));
        mExtractButton->setEnabled(false);
        return;
    }

    const bool known = mAirportIdents.contains(icao, Qt::CaseInsensitive);
    mExtractButton->setEnabled(!mBuildWatcher.isRunning() && known);
    mStatusLabel->setText(known ? QStringLiteral("Aéroport %1 reconnu — lancez l'extraction.").arg(icao)
                                : QStringLiteral("Code « %1 » introuvable dans cette base.").arg(icao));
}

// -----------------------------------------------------------------------------------------------------------
void AirportExtractDialog::runExtract()
{
    if (mBuildWatcher.isRunning())
        return;

    if (!mDb) {
        appendLog(QStringLiteral("Charger le fichier mondial d'abord."));
        return;
    }
    const QString icao = currentIcao();
    const QString outputPath = mOutputEdit->text().trimmed();
    if (icao.isEmpty() || outputPath.isEmpty()) {
        appendLog(QStringLiteral("Saisissez un code aéroport et un fichier de sortie."));
        return;
    }
    if (!mAirportIdents.contains(icao, Qt::CaseInsensitive)) {
        appendLog(QStringLiteral("Erreur : le code « %1 » est introuvable dans cette base.").arg(icao));
        return;
    }

    // Le tollé d'extraction ET la construction du projet ET la lecture des
    // compteurs (# Count: x) du fichier mondial se font dans un seul thread
    // de travail : tous les objets Core sont réentrants et ne touchent ni à
    // mDb ni à l'interface. La GUI ne fait ensuite que ProjectStore (SQLite).
    const std::shared_ptr<navstud::extract::NavDataBase> db = mDb;
    const QString sourcePath = mSourceEdit->text().trimmed();

    setBusy(true);
    mStatusLabel->setText(QStringLiteral("Extraction de l'aéroport %1 en cours...").arg(icao));
    QApplication::setOverrideCursor(Qt::WaitCursor);

    mBuildWatcher.setFuture(QtConcurrent::run([db, icao, outputPath, sourcePath]() -> BuildOutcome {
        BuildOutcome outcome;
        outcome.outputPath = outputPath;
        outcome.icao = icao;

        navstud::extract::NavDataBase::ExtractStats stats;
        QString error;
        if (!db->extractAirport(icao, outputPath, &error, &stats)) {
            outcome.error = error;
            return outcome;
        }
        outcome.stats = stats;

        const navstud::worldindex::WorldIndexReader reader;
        const navstud::worldindex::WorldIndexResult world = reader.readStartingIndices(sourcePath);
        outcome.indices = world.indices;
        outcome.indicesOk = world.success;
        outcome.missingSections = world.missingSections;

        const navstud::extract::ExtractedProjectBuilder builder;
        navstud::extract::ExtractBuildResult build = builder.build(outputPath);
        if (!build.success) {
            outcome.error = build.errorMessage;
            return outcome;
        }
        outcome.warnings = build.warnings;
        outcome.project = std::move(build.project);
        outcome.ok = true;
        return outcome;
    }));
}

// -----------------------------------------------------------------------------------------------------------
void AirportExtractDialog::onLoadFinished()
{
    QApplication::restoreOverrideCursor();
    setBusy(false);

    const LoadOutcome outcome = mLoadWatcher.result();
    if (!outcome.error.isEmpty()) {
        mStatusLabel->setText(QStringLiteral("Chargement impossible."));
        appendLog(QStringLiteral("Erreur : %1").arg(outcome.error));
        QMessageBox::warning(this, QStringLiteral("Chargement du fichier mondial"),
                             QStringLiteral("Lecture impossible : %1").arg(outcome.error));
        return;
    }

    mDb = outcome.db;

    QStringList idents = mDb->airportIdents();
    idents.sort();
    mAirportIdents = idents;

    auto* model = new QStringListModel(mAirportIdents, mAirportCombo);
    auto* completer = new QCompleter(model, mAirportCombo);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setCompletionMode(QCompleter::PopupCompletion);
    completer->setFilterMode(Qt::MatchContains);
    mAirportCombo->setModel(model);
    mAirportCombo->setCompleter(completer);
    mAirportCombo->lineEdit()->clear();
    mAirportCombo->lineEdit()->setPlaceholderText(QStringLiteral("Sélectionner l'aéroport"));
    mAirportCombo->setEnabled(!idents.isEmpty());
    mBrowseOutputButton->setEnabled(true);
    mCreateProjectCheck->setEnabled(true);

    if (idents.isEmpty()) {
        mStatusLabel->setText(QStringLiteral("Aucun aéroport détecté dans %1.").arg(mDb->source()));
        appendLog(QStringLiteral("Aucun aéroport détecté dans %1.").arg(mDb->source()));
        return;
    }

    mStatusLabel->setText(QStringLiteral("%1 aéroports détectés — saisissez un code ICAO (ex. KJFK) puis Extraire.").arg(idents.size()));
    appendLog(QStringLiteral("Fichier mondial chargé : %1 (%2 aéroports).").arg(mDb->source()).arg(idents.size()));
}

// -----------------------------------------------------------------------------------------------------------
void AirportExtractDialog::onBuildFinished()
{
    QApplication::restoreOverrideCursor();
    setBusy(false);

    const BuildOutcome outcome = mBuildWatcher.result();
    if (!outcome.ok) {
        mStatusLabel->setText(QStringLiteral("Extraction échouée."));
        appendLog(QStringLiteral("Erreur : %1").arg(outcome.error));
        QMessageBox::critical(this, QStringLiteral("Extraction"),
                              QStringLiteral("Extraction / construction impossible : %1").arg(outcome.error));
        return;
    }

    const auto& s = outcome.stats;
    appendLog(QStringLiteral("--- Extraction de %1 ---").arg(outcome.icao));
    appendLog(QStringLiteral("  Points=%1  Waypoints=%2  Navaids=%3  Airports=%4  Runways=%5")
                  .arg(s.points).arg(s.waypoints).arg(s.navaids).arg(s.airports).arg(s.runways));
    appendLog(QStringLiteral("  LegSequences=%1  Legs=%2  Departures=%3  Arrivals=%4  Approaches=%5")
                  .arg(s.legSequences).arg(s.legs).arg(s.departures).arg(s.arrivals).arg(s.approaches));
    appendLog(QStringLiteral("  AppTrans=%1  DepTrans=%2  ArrTrans=%3  RwyDep=%4  RwyArr=%5")
                  .arg(s.appTransitions).arg(s.depTransitions).arg(s.arrTransitions)
                  .arg(s.rwyDepTransitions).arg(s.rwyArrTransitions));

    for (const QString& warning : outcome.warnings)
        appendLog(QStringLiteral("  avertissement : %1").arg(warning));

    if (!mCreateProjectCheck->isChecked()) {
        mStatusLabel->setText(QStringLiteral("Extraction terminée — projet SQLite non créé (case décochée)."));
        appendLog(QStringLiteral("Fichier extrait : %1").arg(outcome.outputPath));
        return;
    }

    // --- Création / remplacement du projet SQLite sur le thread GUI ---
    // --- Ajout d'un NOUVEAU projet dans la base existante (jamais de
    //     remplacement : la base projects.sqlite et ses autres projets sont
    //     intacts, on n'ajoute qu'une nouvelle entrée). ---
    const QString baseName = outcome.icao;
    QStringList existingNames;
    for (const navstud::persistence::ProjectSummary& summary : mStore.listProjects())
        existingNames << summary.name;

    QString name = baseName;
    int counter = 2;
    while (existingNames.contains(name))
        name = QStringLiteral("%1 (%2)").arg(baseName).arg(counter++);

    QString err;
    const qint64 targetId = mStore.createProject(name, outcome.indices, &err);
    if (targetId < 0) {
        QMessageBox::critical(this, QStringLiteral("Erreur"),
                              QStringLiteral("Ajout du projet impossible : %1").arg(err));
        return;
    }

    if (!mStore.saveProject(targetId, outcome.project, &err)) {
        QMessageBox::critical(this, QStringLiteral("Erreur"),
                              QStringLiteral("Sauvegarde du projet impossible : %1").arg(err));
        return;
    }

    if (name != baseName)
        appendLog(QStringLiteral("  (un projet \"%1\" existait déjà — le nouveau projet est ajouté sous le nom \"%2\".)")
                      .arg(baseName, name));
    mStatusLabel->setText(QStringLiteral("Projet \"%1\" ajouté dans la base — rien n'a été remplacé.").arg(name));
    appendLog(QStringLiteral("Projet \"%1\" ajouté dans la base.").arg(name));
    emit projectCreated(targetId, name);
}

// -----------------------------------------------------------------------------------------------------------
void AirportExtractDialog::setBusy(bool busy)
{
    mBrowseSourceButton->setEnabled(!busy);
    mLoadButton->setEnabled(!busy && !mSourceEdit->text().trimmed().isEmpty());
    mBrowseOutputButton->setEnabled(!busy);
    mCreateProjectCheck->setEnabled(!busy && mDb != nullptr);
    mExtractButton->setEnabled(!busy && mDb != nullptr
                               && mAirportIdents.contains(mAirportCombo->currentText().trimmed(), Qt::CaseInsensitive));
    mCloseButton->setEnabled(!busy);
    if (!busy)
        QApplication::restoreOverrideCursor();
}

// -----------------------------------------------------------------------------------------------------------
void AirportExtractDialog::appendLog(const QString& text)
{
    mLogEdit->appendPlainText(text);
}