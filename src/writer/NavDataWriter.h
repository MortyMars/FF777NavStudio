#pragma once

// ============================================================================
// NavDataWriter.h
// Traduit un ProjectRepository vers les 15 fichiers texte cibles. Aucune
// validation métier ici (rôle du futur Validator, en amont du writer) —
// le writer fait confiance aux données qu'on lui donne et se contente de
// les sérialiser au format exact.
// ============================================================================

#include "ProjectRepository.h"

#include <QDir>
#include <QString>
#include <QVector>

namespace navstud::writer {

class NavDataWriter
{
public:
    struct FileResult
    {
        QString fileName;
        bool    success      = false;
        int     linesWritten = 0;
        QString errorMessage;
    };

    // Écrit les 15 fichiers dans outputDir (créé si absent). N'interrompt
    // pas au premier échec : tente tous les fichiers, pour que l'appelant
    // voie d'un coup tout ce qui a échoué plutôt qu'un seul message.
    QVector<FileResult> writeAll(const model::ProjectRepository& repo, const QDir& outputDir) const;

    // Formatte UNE ligne isolée par structure — utilisées par les writeXxx()
    // internes ET par l'aperçu temps réel de l'UI, pour qu'il n'y ait jamais
    // qu'un seul endroit qui connaisse le format exact d'une ligne donnée.
    static QString formatPointLine(model::PointId id, const model::Point& point);
    static QString formatWaypointLine(model::WaypointId id, const model::Waypoint& waypoint);
    static QString formatNavaidLine(model::NavaidId id, const model::Navaid& navaid);
    static QString formatAirportLine(model::AirportId id, const model::Airport& airport);
    static QString formatRunwayLine(model::RunwayId id, const model::Runway& runway);
    static QString formatLegSequenceLine(model::LegSequenceId id, const model::LegSequence& legSequence);
    static QString formatLegLine(model::LegId id, const model::Leg& leg);
    static QString formatApproachLine(model::ApproachId id, const model::Approach& approach);
    static QString formatApproachTransitionLine(model::ApproachTransitionId id, const model::ApproachTransition& transition);
    static QString formatProcedureLine(model::ProcedureId id, const model::Procedure& procedure);
    static QString formatProcedureTransitionLine(model::ProcedureTransitionId id, const model::ProcedureTransition& transition);
    static QString formatRunwayProcedureTransitionLine(model::RunwayProcedureTransitionId id,
                                                         const model::RunwayProcedureTransition& transition);

private:
    FileResult writePoints(const model::ProjectRepository& repo, const QDir& dir) const;
    FileResult writeWaypoints(const model::ProjectRepository& repo, const QDir& dir) const;
    FileResult writeNavaids(const model::ProjectRepository& repo, const QDir& dir) const;
    FileResult writeAirports(const model::ProjectRepository& repo, const QDir& dir) const;
    FileResult writeRunways(const model::ProjectRepository& repo, const QDir& dir) const;
    FileResult writeLegSequences(const model::ProjectRepository& repo, const QDir& dir) const;
    FileResult writeLegs(const model::ProjectRepository& repo, const QDir& dir) const;
    FileResult writeApproaches(const model::ProjectRepository& repo, const QDir& dir) const;
    FileResult writeApproachTransitions(const model::ProjectRepository& repo, const QDir& dir) const;

    // Communes à SID/STAR : un seul corps filtré par ProcedureKind pour les
    // deux fichiers de sortie (cf. ProcedureKind dans Entities.h — le champ
    // kind lui-même n'est jamais écrit dans la ligne texte).
    FileResult writeProcedures(const model::ProjectRepository& repo, model::ProcedureKind kind,
                                const QString& fileName, const QDir& dir) const;
    FileResult writeProcedureTransitions(const model::ProjectRepository& repo, model::ProcedureKind kind,
                                          const QString& fileName, const QDir& dir) const;
    FileResult writeRunwayProcedureTransitions(const model::ProjectRepository& repo, model::ProcedureKind kind,
                                                const QString& fileName, const QDir& dir) const;

    // Écrit `lines` dans dir/fileName (UTF-8, \n). Une liste vide est
    // autorisée et n'est PAS une erreur (cas SID sans transition de piste,
    // cf. Entities.h) — le fichier est créé, correctement nommé, vide.
    FileResult writeLines(const QStringList& lines, const QString& fileName, const QDir& dir) const;
};

} // namespace navstud::writer
