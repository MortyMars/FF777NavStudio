#ifndef NAV1DBPIPELINE_H
#define NAV1DBPIPELINE_H

// ============================================================================
// Nav1DbPipeline.h
// Pipelai intégré de manipulation du fichier mondial nav1.db :
//   - décodage nav1.db -> nav1.txt          (outil ParserFFDB/FileConverter)
//   - intégration des 15 fichiers _Xxx.txt produits par FF777NavStudio
//   - réencodage nav1.txt -> nav1-2.db      (même outil, sens inverse)
//   - déploiement nav1.db + fichier de contrôle vers la destination X-Plane
//
// Toutes les opérations travaillent dans le répertoire de l'application :
// c'est là que doivent se trouver nav1.db, nav1.txt et les 15 fichiers texte.
// ============================================================================

#include <QString>

namespace navstud::tools {

class Nav1DbPipeline
{
public:
    // --------------------------------------------------------------------------------------------------------------------------------
    // Chemins (répertoire de travail = dossier contenant le bundle .app)
    // --------------------------------------------------------------------------------------------------------------------------------
    static QString workingDir();

    // Décodage / encodage dans le dossier de travail
    static QString nav1DbPath();      // nav1.db   (source à décoder)
    static QString nav1TxtPath();     // nav1.txt  (décodé / à compléter)
    static QString nav1EncodedPath(); // nav1-2.db (résultat du réencodage)
    static QString nav1ControlPath(); // nav1-K.txt (fichier de contrôle)

    // Détermine le n° d'AIRAC ($AIRAC) à partir du 4ème champ de la ligne
    // CONFIG de nav1.txt (ex. `CONFIG 0 "JEP" "2608" ...` -> "2608").
    // Retourne une chaîne vide si le fichier ou la ligne CONFIG est absente.
    static QString readAirac(const QString& nav1TxtPath, QString* error = nullptr);

    // Destination finale du déploiement (dossier X-Plane)
    static QString destinationDir();

    // --------------------------------------------------------------------------------------------------------------------------------
    // Opérations
    // --------------------------------------------------------------------------------------------------------------------------------

    // Op 2 - décode nav1.db -> nav1.txt (dans le dossier de travail). La
    // première surcharge utilise le nav1.db du dossier de travail ; la seconde
    // accepte un fichier source explicite (choisi hors du dossier de travail).
    static bool decode(QString* errorMessage, QString* detail = nullptr);
    static bool decode(const QString& sourceDbPath, QString* errorMessage, QString* detail = nullptr);

    // Op 4 - intègre les 15 fichiers de données _Xxx.txt présents dans le
    // dossier de travail dans nav1.txt (met à jour les "# Count:").
    static bool integrate(QString* errorMessage, QString* detail = nullptr);

    // Op 5 - réencode nav1.txt -> nav1-2.db puis crée le nav1-K.txt contrôle.
    static bool encode(QString* errorMessage, QString* detail = nullptr);

    // Op 6 - copie nav1-2.db + nav1-K.txt vers la destination X-Plane.
    static bool deploy(QString* errorMessage, QString* detail = nullptr);

    // Événement groupé (ops 4->5->6) : intégration + réencodage + déploiement.
    static bool integrateEncodeDeploy(QString* errorMessage, QString* detail = nullptr);
};

} // namespace navstud::tools

#endif // NAV1DBPIPELINE_H