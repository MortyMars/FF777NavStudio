#pragma once

// ============================================================================
// AirportExtractDialog.h
// Boîte de dialogue « Extraire un aéroport (nav1.txt -> projet) ».
//
// 1. L'utilisateur choisit un fichier mondial nav1.txt (le même que celui
//    livré par le FlightFactor B777 pour le pipeline Standard) et le charge
//    en arrière-plan (NavDataBase) — la liste des aéroports se remplit.
// 2. Il choisit l'aéroport désiré et l'emplacement du fichier extrait, puis
//    lance l'extraction : les données rattachées DIRECTEMENT à cet aéroport
//    (points, navaids, runways, séquences d'approche/SID/STAR, transitions...)
//    sont recopiées VERBATIM dans un fichier d'extraction.
// 3. Le projet SQLite est ensuite construit (ExtractedProjectBuilder) et, si
//    la case est cochée, créé dans la base — le signal projectCreated() relance
//    alors MainWindow::loadProjectIntoUi().
//
// NavDataBase et WorldIndexReader (lecture du "# Count:" du fichier mondial
// pour amorcer les compteurs de départ) tournent dans des threads QtConcurrent
// afin que l'interface ne gèle pas sur un fichier de ~200 Mo.
// ============================================================================

#include "NavDataBase.h"     // navstud::extract::NavDataBase + ExtractStats
#include "ProjectStore.h"    // navstud::persistence::ProjectStore
#include "UserProject.h"     // userdata::UserProject / StartingIndices

#include <QDialog>
#include <QFutureWatcher>

#include <memory>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;

class AirportExtractDialog : public QDialog
{
    Q_OBJECT

    public:
        explicit AirportExtractDialog(
            navstud::persistence::ProjectStore& store,
            QWidget* parent = nullptr
        );

        ~AirportExtractDialog() override;

    signals:
        // Émise après qu'un projet a été créé/remplacé à partir de l'extraction.
        void projectCreated(qint64 id, const QString& name);

    private slots:
        void browseSourceFile();
        void browseOutputFile();
        void loadSource();
        void onAirportChanged();
        void runExtract();
        void onLoadFinished();
        void onBuildFinished();

    private:
        struct LoadOutcome
        {
            std::shared_ptr<navstud::extract::NavDataBase> db;
            QString error;
        };
        struct BuildOutcome
        {
            bool ok = false;
            QString error;
            QStringList warnings;
            QString outputPath;
            QString icao;
            navstud::extract::NavDataBase::ExtractStats stats;
            navstud::userdata::UserProject project;
            navstud::userdata::StartingIndices indices;
            bool indicesOk = false;
            QStringList missingSections;
        };

        void setBusy(bool busy);
        void appendLog(const QString& text);
        QString currentIcao() const;

        navstud::persistence::ProjectStore& mStore;

        std::shared_ptr<navstud::extract::NavDataBase> mDb;
        QStringList mAirportIdents;
        QFutureWatcher<LoadOutcome>  mLoadWatcher;
        QFutureWatcher<BuildOutcome> mBuildWatcher;

        QLineEdit*      mSourceEdit = nullptr;
        QPushButton*    mBrowseSourceButton = nullptr;
        QPushButton*    mLoadButton = nullptr;
        QComboBox*      mAirportCombo = nullptr;
        QLineEdit*      mOutputEdit = nullptr;
        QPushButton*    mBrowseOutputButton = nullptr;
        QCheckBox*      mCreateProjectCheck = nullptr;
        QLabel*         mStatusLabel = nullptr;
        QPlainTextEdit* mLogEdit = nullptr;
        QPushButton*    mExtractButton = nullptr;
        QPushButton*    mCloseButton = nullptr;

};