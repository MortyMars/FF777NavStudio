#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "ProjectStore.h"
#include "Regenerator.h"
#include "UserProject.h"

#include <QMainWindow>
#include <QPushButton>

#include <functional>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class QAction;
class QTableView;
class QSortFilterProxyModel;
class GenericTableModel;
class PointEditorWidget;
class WaypointEditorWidget;
class AirportEditorWidget;
class RunwayEditorWidget;
class NavaidEditorWidget;
class LegSequenceEditorWidget;
class LegEditorWidget;
class ApproachEditorWidget;
class ApproachTransitionEditorWidget;
class ProcedureEditorWidget;
class ProcedureTransitionEditorWidget;
class RunwayProcedureTransitionEditorWidget;
class AirportExtractDialog;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onPointSelectionChanged();
    void onNewPoint();
    void onPointEdited();

    void onWaypointSelectionChanged();
    void onNewWaypoint();
    void onWaypointEdited();

    void onAirportSelectionChanged();
    void onNewAirport();
    void onAirportEdited();

    void onRunwaySelectionChanged();
    void onNewRunway();
    void onRunwayEdited();

    void onNavaidSelectionChanged();
    void onNewNavaid();
    void onNavaidEdited();

    void onLegSequenceSelectionChanged();
    void onNewLegSequence();
    void onLegSequenceEdited();

    void onLegSelectionChanged();
    void onNewLeg();
    void onLegEdited();

    void onApproachSelectionChanged();
    void onNewApproach();
    void onApproachEdited();

    void onApproachTransitionSelectionChanged();
    void onNewApproachTransition();
    void onApproachTransitionEdited();

    void onSidProcedureSelectionChanged();
    void onNewSidProcedure();
    void onSidProcedureEdited();

    void onStarProcedureSelectionChanged();
    void onNewStarProcedure();
    void onStarProcedureEdited();

    void onSidProcedureTransitionSelectionChanged();
    void onNewSidProcedureTransition();
    void onSidProcedureTransitionEdited();

    void onStarProcedureTransitionSelectionChanged();
    void onNewStarProcedureTransition();
    void onStarProcedureTransitionEdited();

    void onSidRunwayProcedureTransitionSelectionChanged();
    void onNewSidRunwayProcedureTransition();
    void onSidRunwayProcedureTransitionEdited();

    void onStarRunwayProcedureTransitionSelectionChanged();
    void onNewStarRunwayProcedureTransition();
    void onStarRunwayProcedureTransitionEdited();

    void onNewProject();
    void onOpenProject();
    void onSaveProject();
    void onReloadWorldFile();
    void onExportFiles();

    // Menu 'One shot', accédant à cette méthode, désactivé
    void onImportFromTextFiles();

    void onExtractAirport();
    void onDecodeWorldFile();
    void onIntegrateWorldFile();
    void onAbout();
    void onDocumentation();

private:
    void loadProjectIntoUi(qint64 id, const QString& name);
    // Corps du rechargement mondial, factorisé en un seul endroit pour être
    // appelable aussi bien depuis le menu que depuis la chaîne "décoder".
    bool applyWorldFile(const QString& worldFile);

    // Assemble bouton "Nouveau" + bouton "Supprimer" + table + éditeur dans
    // un splitter — layout commun à tous les onglets, factorisé pour ne pas
    // le répéter 15 fois.
    QWidget* buildTabLayout(QPushButton* newButton, QPushButton* deleteButton, QTableView* table, QWidget* editor);

    // Supprime la ligne sélectionnée de table/model, générique sur les 15
    // onglets — cf. définition dans mainwindow.cpp.
    void deleteCurrentRow(QTableView* table, GenericTableModel* model, QSortFilterProxyModel* proxy, QWidget* editor,
                           const std::function<bool(qint32)>& removeFn, const std::function<void()>& clearCurrentId,
                           const std::function<void()>& compactFn);

    // Id brut de la ligne sélectionnée dans la vue, en passant par le proxy
    // de tri (la sélection de la QTableView porte un index PROXY, pas un
    // index du GenericTableModel sous-jacent) — -1 si rien n'est sélectionné.
    qint32 currentRawId(QTableView* table, GenericTableModel* model, QSortFilterProxyModel* proxy) const;

    // Sélectionne dans la vue la ligne portant sourceRow dans le modèle
    // SOURCE (typiquement mModel->rowCount() - 1, la ligne qui vient d'être
    // ajoutée) — passe par le proxy pour trouver où elle apparaît une fois
    // triée, puisque sa position dans la vue n'est plus forcément la
    // dernière dès que le tri alphabétique est actif.
    void selectSourceRow(QTableView* table, GenericTableModel* model, QSortFilterProxyModel* proxy, int sourceRow);

    // Régénère mProject (Point n'en a pas besoin, mais Waypoint/Airport/
    // Runway/Navaid et tout ce qui suivra oui) et extrait soit la ligne
    // résolue pour (kind, rawId), soit son message d'échec de conversion —
    // un message d'erreur prime toujours sur une ligne partielle.
    QString previewFor(navstud::validator::EntityKind kind, qint32 rawId,
                        const navstud::conversion::RegenerationResult& result) const;

    Ui::MainWindow* ui;

    navstud::persistence::ProjectStore mStore;
    qint64   mCurrentProjectId = -1;
    QAction* mSaveAction = nullptr;
    QAction* mReloadWorldAction = nullptr;
    QAction* mExportAction = nullptr;
    QAction* mDecodeWorldAction = nullptr;
    QAction* mIntegrateWorldAction = nullptr;

    navstud::userdata::UserProject mProject;

    GenericTableModel*  mPointModel  = nullptr;
    QSortFilterProxyModel* mPointProxy = nullptr;
    QTableView*         mPointTable  = nullptr;
    PointEditorWidget*  mPointEditor = nullptr;
    navstud::model::PointId mCurrentPointId = navstud::model::PointId::invalid();

    GenericTableModel*     mWaypointModel  = nullptr;
    QSortFilterProxyModel* mWaypointProxy = nullptr;
    QTableView*            mWaypointTable  = nullptr;
    WaypointEditorWidget*  mWaypointEditor = nullptr;
    navstud::model::WaypointId mCurrentWaypointId = navstud::model::WaypointId::invalid();

    GenericTableModel*    mAirportModel  = nullptr;
    QSortFilterProxyModel* mAirportProxy = nullptr;
    QTableView*           mAirportTable  = nullptr;
    AirportEditorWidget*  mAirportEditor = nullptr;
    navstud::model::AirportId mCurrentAirportId = navstud::model::AirportId::invalid();

    GenericTableModel*   mRunwayModel  = nullptr;
    QSortFilterProxyModel* mRunwayProxy = nullptr;
    QTableView*          mRunwayTable  = nullptr;
    RunwayEditorWidget*  mRunwayEditor = nullptr;
    navstud::model::RunwayId mCurrentRunwayId = navstud::model::RunwayId::invalid();

    GenericTableModel*   mNavaidModel  = nullptr;
    QSortFilterProxyModel* mNavaidProxy = nullptr;
    QTableView*          mNavaidTable  = nullptr;
    NavaidEditorWidget*  mNavaidEditor = nullptr;
    navstud::model::NavaidId mCurrentNavaidId = navstud::model::NavaidId::invalid();

    GenericTableModel*        mLegSequenceModel  = nullptr;
    QSortFilterProxyModel* mLegSequenceProxy = nullptr;
    QTableView*                mLegSequenceTable  = nullptr;
    LegSequenceEditorWidget*   mLegSequenceEditor = nullptr;
    navstud::model::LegSequenceId  mCurrentLegSequenceId = navstud::model::LegSequenceId::invalid();

    GenericTableModel* mLegModel  = nullptr;
    QSortFilterProxyModel* mLegProxy = nullptr;
    QTableView*         mLegTable  = nullptr;
    LegEditorWidget*    mLegEditor = nullptr;
    navstud::model::LegId   mCurrentLegId = navstud::model::LegId::invalid();

    GenericTableModel*     mApproachModel  = nullptr;
    QSortFilterProxyModel* mApproachProxy = nullptr;
    QTableView*             mApproachTable  = nullptr;
    ApproachEditorWidget*   mApproachEditor = nullptr;
    navstud::model::ApproachId  mCurrentApproachId = navstud::model::ApproachId::invalid();

    GenericTableModel*                mApproachTransitionModel  = nullptr;
    QSortFilterProxyModel* mApproachTransitionProxy = nullptr;
    QTableView*                        mApproachTransitionTable  = nullptr;
    ApproachTransitionEditorWidget*    mApproachTransitionEditor = nullptr;
    navstud::model::ApproachTransitionId   mCurrentApproachTransitionId = navstud::model::ApproachTransitionId::invalid();

    GenericTableModel*      mSidProcedureModel  = nullptr;
    QSortFilterProxyModel* mSidProcedureProxy = nullptr;
    QTableView*               mSidProcedureTable  = nullptr;
    ProcedureEditorWidget*    mSidProcedureEditor = nullptr;
    navstud::model::ProcedureId   mCurrentSidProcedureId = navstud::model::ProcedureId::invalid();

    GenericTableModel*      mStarProcedureModel  = nullptr;
    QSortFilterProxyModel* mStarProcedureProxy = nullptr;
    QTableView*               mStarProcedureTable  = nullptr;
    ProcedureEditorWidget*    mStarProcedureEditor = nullptr;
    navstud::model::ProcedureId   mCurrentStarProcedureId = navstud::model::ProcedureId::invalid();

    GenericTableModel*                 mSidProcedureTransitionModel  = nullptr;
    QSortFilterProxyModel* mSidProcedureTransitionProxy = nullptr;
    QTableView*                          mSidProcedureTransitionTable  = nullptr;
    ProcedureTransitionEditorWidget*     mSidProcedureTransitionEditor = nullptr;
    navstud::model::ProcedureTransitionId    mCurrentSidProcedureTransitionId = navstud::model::ProcedureTransitionId::invalid();

    GenericTableModel*                 mStarProcedureTransitionModel  = nullptr;
    QSortFilterProxyModel* mStarProcedureTransitionProxy = nullptr;
    QTableView*                          mStarProcedureTransitionTable  = nullptr;
    ProcedureTransitionEditorWidget*     mStarProcedureTransitionEditor = nullptr;
    navstud::model::ProcedureTransitionId    mCurrentStarProcedureTransitionId = navstud::model::ProcedureTransitionId::invalid();

    GenericTableModel*                        mSidRunwayProcedureTransitionModel  = nullptr;
    QSortFilterProxyModel* mSidRunwayProcedureTransitionProxy = nullptr;
    QTableView*                                 mSidRunwayProcedureTransitionTable  = nullptr;
    RunwayProcedureTransitionEditorWidget*      mSidRunwayProcedureTransitionEditor = nullptr;
    navstud::model::RunwayProcedureTransitionId     mCurrentSidRunwayProcedureTransitionId = navstud::model::RunwayProcedureTransitionId::invalid();

    GenericTableModel*                        mStarRunwayProcedureTransitionModel  = nullptr;
    QSortFilterProxyModel* mStarRunwayProcedureTransitionProxy = nullptr;
    QTableView*                                 mStarRunwayProcedureTransitionTable  = nullptr;
    RunwayProcedureTransitionEditorWidget*      mStarRunwayProcedureTransitionEditor = nullptr;
    navstud::model::RunwayProcedureTransitionId     mCurrentStarRunwayProcedureTransitionId = navstud::model::RunwayProcedureTransitionId::invalid();
};
#endif // MAINWINDOW_H
