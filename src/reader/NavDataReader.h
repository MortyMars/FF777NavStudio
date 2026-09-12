#pragma once

// ============================================================================
// NavDataReader.h
// Lit les 15 fichiers texte cibles vers un ProjectRepository. Tolérant aux
// lignes malformées : une ligne invalide est consignée en erreur et ignorée,
// la lecture du fichier se poursuit — jamais d'abandon total sur un souci
// ponctuel. Ne fait AUCUNE validation métier (rôle du Validator, en aval,
// à lancer sur le repository une fois la lecture terminée).
// ============================================================================

#include "ProjectRepository.h"

#include <QDir>
#include <QString>
#include <QStringList>
#include <QVector>

namespace navstud::reader {

class NavDataReader
{
public:
    struct FileResult
    {
        QString     fileName;
        bool        success        = false; // false seulement si le fichier n'a pas pu être ouvert
        int         entitiesLoaded = 0;
        QStringList errors;                 // une entrée par ligne rejetée, format "ligne N : ..."
    };

    // Lit les 15 fichiers depuis inputDir vers repo. N'interrompt pas au
    // premier échec : tente tous les fichiers.
    QVector<FileResult> readAll(model::ProjectRepository& repo, const QDir& inputDir) const;

private:
    FileResult readPoints(model::ProjectRepository& repo, const QDir& dir) const;
    FileResult readWaypoints(model::ProjectRepository& repo, const QDir& dir) const;
    FileResult readNavaids(model::ProjectRepository& repo, const QDir& dir) const;
    FileResult readAirports(model::ProjectRepository& repo, const QDir& dir) const;
    FileResult readRunways(model::ProjectRepository& repo, const QDir& dir) const;
    FileResult readLegSequences(model::ProjectRepository& repo, const QDir& dir) const;
    FileResult readLegs(model::ProjectRepository& repo, const QDir& dir) const;
    FileResult readApproaches(model::ProjectRepository& repo, const QDir& dir) const;
    FileResult readApproachTransitions(model::ProjectRepository& repo, const QDir& dir) const;

    // Communes à SID/STAR : un seul corps, kind fixé à l'appel selon le
    // fichier lu (symétrique de NavDataWriter — le fichier source EST
    // l'information qui donne kind, puisque rien dans la ligne elle-même
    // ne le précise).
    FileResult readProcedures(model::ProjectRepository& repo, model::ProcedureKind kind,
                               const QString& fileName, const QDir& dir) const;
    FileResult readProcedureTransitions(model::ProjectRepository& repo, model::ProcedureKind kind,
                                         const QString& fileName, const QDir& dir) const;
    FileResult readRunwayProcedureTransitions(model::ProjectRepository& repo, model::ProcedureKind kind,
                                               const QString& fileName, const QDir& dir) const;

    struct RawLines
    {
        bool        fileExists = false;
        bool        opened     = false;
        QStringList lines;      // lignes non vides uniquement
        QString     errorMessage;
    };

    // Fichier absent ou vide -> pas une erreur (RawLines::lines vide, opened
    // à vrai si le fichier existait). Fichier existant mais illisible ->
    // errorMessage renseigné, opened à faux.
    RawLines readRawLines(const QString& fileName, const QDir& dir) const;
};

} // namespace navstud::reader
