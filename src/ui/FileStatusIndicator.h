#pragma once

// ============================================================================
// FileStatusIndicator.h
// Petit tableau incrusté (bas droite de la fenêtre) indiquant l'état des
// fichiers Nav1.db / Nav1.txt / Nav1-2.db et du jeu de fichiers .txt extraits.
//
// Les règles d'affichage suivent le document 'Etat_des_fichiers.md' :
//   - "N.C."          : littéral (non concerné / non créé)
//   - $AIRAC          : n° d'AIRAC lu dans la ligne CONFIG de nav1.txt
//   - $AIRAC "xxx"    : n° d'AIRAC + qualificatif (reduced / increased)
//   - $PROJET         : nom de l'aéroport/projet chargé
//   - inchangé        : la valeur précédente est conservée
// ============================================================================

#include <QFrame>
#include <QString>

class QLabel;

class FileStatusIndicator : public QFrame
{
    Q_OBJECT

    public:
        explicit FileStatusIndicator(QWidget* parent = nullptr);

        // Fixe le n° d'AIRAC courant ($AIRAC), relu à chaque évènement.
        void setAirAirac(const QString& airac);

        // Évènements du tableau 'Etat_des_fichiers.md'.
        void notifyAppOpened();
        void notifyAirportToProject();
        void notifyReloadWorld();
        void notifyExportTxt(const QString& project);
        void notifyDecodeNav1Db();
        void notifyIntegrate();

    private:
        // Formate une valeur de cellule : $AIRAC seul ou $AIRAC + qualificatif.
        QString airacValue(const QString& suffix = QString()) const;

        void refresh();

        QString mAirac;

        QString mNav1Db;    // état affiché de Nav1.db
        QString mNav1Txt;   // état affiché de Nav1.txt
        QString mNav12Db;   // état affiché de Nav1-2.db
        QString mTxtSet;    // état affiché du jeu de .txt

        QLabel* mNav1DbValue = nullptr;
        QLabel* mNav1TxtValue = nullptr;
        QLabel* mNav12DbValue = nullptr;
        QLabel* mTxtSetValue = nullptr;
};
