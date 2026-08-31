#include "Regenerator.h"
#include "IdentResolver.h"
#include "UserToModelConverter.h"

namespace navstud::conversion {

using namespace navstud::model;
using namespace navstud::userdata;
using namespace navstud::validator;

// -----------------------------------------------------------------------------------------------------------
// Régénère l'intégralité du dépôt modèle depuis le projet utilisateur en
// enchaînant les conversions ordonnées (points, puis relais, etc.), puis
// valide le résultat et renvoie les diagnostics.
RegenerationResult Regenerator::regenerate(const UserProject& project) const
{
    RegenerationResult result;

    // --- Point --- (aucune résolution d'ident nécessaire)
    for (const PointId id : project.points().order()) {
        const UserPoint* userPoint = project.points().find(id);
        if (!userPoint)
            continue;
        const ConversionResult<Point> converted = convertPoint(*userPoint);
        if (converted.isOk()) {
            // id explicite : préserve l'égalité d'id entre UserProject et
            // ProjectRepository, cf. UserProject.h — pas de nouvel id alloué ici.
            result.repository.points().add(converted.value(), id);
        } else {
            result.conversionFailures.push_back(ConversionFailure{
                EntityKind::Point, id.value(), converted.errors() });
        }
    }

    // --- Waypoint --- (1 saut -> Point, déjà peuplé ci-dessus)
    {
        const IdentResolver resolver(result.repository);
        for (const WaypointId id : project.waypoints().order()) {
            const UserWaypoint* userWaypoint = project.waypoints().find(id);
            if (!userWaypoint)
                continue;
            const auto pointId = resolver.point(userWaypoint->pointIdent);
            if (pointId) {
                Waypoint w;
                w.pointId = *pointId;
                result.repository.waypoints().add(w, id);
            } else {
                result.conversionFailures.push_back(ConversionFailure{
                    EntityKind::Waypoint, id.value(),
                    { QStringLiteral("point \"%1\" introuvable").arg(userWaypoint->pointIdent) } });
            }
        }
    }

    // --- Airport --- (1 saut -> Point, indépendant de Waypoint)
    {
        const IdentResolver resolver(result.repository);
        for (const AirportId id : project.airports().order()) {
            const UserAirport* userAirport = project.airports().find(id);
            if (!userAirport)
                continue;
            const ConversionResult<Airport> converted = convertAirport(*userAirport, resolver);
            if (converted.isOk())
                result.repository.airports().add(converted.value(), id);
            else
                result.conversionFailures.push_back(ConversionFailure{
                    EntityKind::Airport, id.value(), converted.errors() });
        }
    }

    // --- Runway --- (2 sauts pt -> Airport, 1 saut -> Point ; Airport doit
    // déjà être peuplé, d'où un nouveau resolver reconstruit après lui)
    {
        const IdentResolver resolver(result.repository);
        for (const RunwayId id : project.runways().order()) {
            const UserRunway* userRunway = project.runways().find(id);
            if (!userRunway)
                continue;
            const ConversionResult<Runway> converted = convertRunway(*userRunway, resolver);
            if (converted.isOk())
                result.repository.runways().add(converted.value(), id);
            else
                result.conversionFailures.push_back(ConversionFailure{
                    EntityKind::Runway, id.value(), converted.errors() });
        }
    }

    // --- Navaid --- (1 saut -> Point, 2 sauts pt -> Runway et -> Navaid déjà
    // converti ; Runway doit déjà être peuplé, d'où un nouveau resolver).
    //
    // DEUX PASSES : associatedNavaidIdent ("GS/DME associé") peut référencer
    // un AUTRE navaid de la MÊME table, potentiellement saisi PLUS TARD dans
    // l'ordre du projet — contrairement à runwayIdent/pointIdent qui pointent
    // toujours vers une table déjà entièrement peuplée en amont. Un passage
    // unique échouerait donc pour toute association "en avant" (ex. le LOC
    // saisi avant le GS qu'il référence) : navaidViaPointIdent() scanne
    // repo.navaids() EN DIRECT (cf. IdentResolver.cpp), qui ne contient à cet
    // instant que les navaids déjà traités dans CETTE boucle.
    //
    // Passe 1 : convertit chaque ligne avec associatedNavaidIdent forcé à
    // "-1", pour peupler repo.navaids() intégralement, indépendamment de
    // l'ordre de saisie. Passe 2, une fois la table complète : reconvertit
    // (avec la vraie association cette fois) uniquement les lignes qui en
    // avaient une et qui ont réussi la passe 1, et corrige l'entité déjà
    // insérée si la résolution réussit désormais ; sinon, consigne l'échec
    // (⚠ dans l'aperçu, cf. previewFor) sans retirer l'entité déjà valide
    // par ailleurs.
    {
        const IdentResolver resolver(result.repository);
        for (const NavaidId id : project.navaids().order()) {
            const UserNavaid* userNavaid = project.navaids().find(id);
            if (!userNavaid)
                continue;
            UserNavaid deferred = *userNavaid;
            deferred.associatedNavaidIdent = QStringLiteral("-1"); // résolu en passe 2
            const ConversionResult<Navaid> converted = convertNavaid(deferred, resolver);
            if (converted.isOk())
                result.repository.navaids().add(converted.value(), id);
            else
                result.conversionFailures.push_back(ConversionFailure{
                    EntityKind::Navaid, id.value(), converted.errors() });
        }
    }
    {
        // Passe 2 : résolution des associations Navaid<->Navaid, contre la
        // table maintenant entièrement peuplée.
        const IdentResolver resolver(result.repository);
        for (const NavaidId id : project.navaids().order()) {
            const UserNavaid* userNavaid = project.navaids().find(id);
            if (!userNavaid)
                continue;
            if (userNavaid->associatedNavaidIdent == QStringLiteral("-1") || userNavaid->associatedNavaidIdent.isEmpty())
                continue; // rien à résoudre
            if (!result.repository.navaids().find(id))
                continue; // passe 1 déjà en échec pour cette ligne, rien à corriger ici
            const ConversionResult<Navaid> converted = convertNavaid(*userNavaid, resolver);
            if (converted.isOk())
                result.repository.navaids().update(id, converted.value());
            else
                result.conversionFailures.push_back(ConversionFailure{
                    EntityKind::Navaid, id.value(), converted.errors() });
        }
    }

    // --- LegSequence --- (aucune résolution d'ident, indépendante de Navaid)
    for (const LegSequenceId id : project.legSequences().order()) {
        const UserLegSequence* userLegSeq = project.legSequences().find(id);
        if (!userLegSeq)
            continue;
        const ConversionResult<LegSequence> converted = convertLegSequence(*userLegSeq);
        if (converted.isOk())
            result.repository.legSequences().add(converted.value(), id);
        else
            result.conversionFailures.push_back(ConversionFailure{
                EntityKind::LegSequence, id.value(), converted.errors() });
    }

    // --- Leg --- (direct -> LegSequence, 1 saut -> Point, 2 sauts pt -> Navaid ;
    // LegSequence et Navaid doivent déjà être peuplés)
    {
        const IdentResolver resolver(result.repository);
        for (const LegId id : project.legs().order()) {
            const UserLeg* userLeg = project.legs().find(id);
            if (!userLeg)
                continue;
            const ConversionResult<Leg> converted = convertLeg(*userLeg, resolver);
            if (converted.isOk())
                result.repository.legs().add(converted.value(), id);
            else
                result.conversionFailures.push_back(ConversionFailure{
                    EntityKind::Leg, id.value(), converted.errors() });
        }
    }

    // --- Procedure (Sid puis Star) --- (2 sauts pt -> Airport, direct -> LegSequence)
    {
        const IdentResolver resolver(result.repository);
        for (const ProcedureId id : project.sidProcedures().order()) {
            const UserProcedure* userProcedure = project.sidProcedures().find(id);
            if (!userProcedure)
                continue;
            const ConversionResult<Procedure> converted = convertProcedure(*userProcedure, resolver);
            if (converted.isOk())
                result.repository.sidProcedures().add(converted.value(), id);
            else
                result.conversionFailures.push_back(ConversionFailure{
                    EntityKind::SidProcedure, id.value(), converted.errors() });
        }
        for (const ProcedureId id : project.starProcedures().order()) {
            const UserProcedure* userProcedure = project.starProcedures().find(id);
            if (!userProcedure)
                continue;
            const ConversionResult<Procedure> converted = convertProcedure(*userProcedure, resolver);
            if (converted.isOk())
                result.repository.starProcedures().add(converted.value(), id);
            else
                result.conversionFailures.push_back(ConversionFailure{
                    EntityKind::StarProcedure, id.value(), converted.errors() });
        }
    }

    // --- Approach --- (2 sauts pt -> Runway, direct -> LegSequence)
    {
        const IdentResolver resolver(result.repository);
        for (const ApproachId id : project.approaches().order()) {
            const UserApproach* userApproach = project.approaches().find(id);
            if (!userApproach)
                continue;
            const ConversionResult<Approach> converted = convertApproach(*userApproach, resolver);
            if (converted.isOk())
                result.repository.approaches().add(converted.value(), id);
            else
                result.conversionFailures.push_back(ConversionFailure{
                    EntityKind::Approach, id.value(), converted.errors() });
        }
    }

    // --- ProcedureTransition (Sid puis Star) --- (2 sauts seq -> Procedure DU
    // MÊME kind, direct -> LegSequence ; Procedure doit déjà être peuplée)
    {
        const IdentResolver resolver(result.repository);
        for (const ProcedureTransitionId id : project.sidProcedureTransitions().order()) {
            const UserProcedureTransition* userTransition = project.sidProcedureTransitions().find(id);
            if (!userTransition)
                continue;
            const ConversionResult<ProcedureTransition> converted =
                convertProcedureTransition(*userTransition, ProcedureKind::Sid, resolver);
            if (converted.isOk())
                result.repository.sidProcedureTransitions().add(converted.value(), id);
            else
                result.conversionFailures.push_back(ConversionFailure{
                    EntityKind::SidProcedureTransition, id.value(), converted.errors() });
        }
        for (const ProcedureTransitionId id : project.starProcedureTransitions().order()) {
            const UserProcedureTransition* userTransition = project.starProcedureTransitions().find(id);
            if (!userTransition)
                continue;
            const ConversionResult<ProcedureTransition> converted =
                convertProcedureTransition(*userTransition, ProcedureKind::Star, resolver);
            if (converted.isOk())
                result.repository.starProcedureTransitions().add(converted.value(), id);
            else
                result.conversionFailures.push_back(ConversionFailure{
                    EntityKind::StarProcedureTransition, id.value(), converted.errors() });
        }
    }

    // --- ApproachTransition --- (2 sauts seq -> Approach, direct -> LegSequence)
    {
        const IdentResolver resolver(result.repository);
        for (const ApproachTransitionId id : project.approachTransitions().order()) {
            const UserApproachTransition* userTransition = project.approachTransitions().find(id);
            if (!userTransition)
                continue;
            const ConversionResult<ApproachTransition> converted = convertApproachTransition(*userTransition, resolver);
            if (converted.isOk())
                result.repository.approachTransitions().add(converted.value(), id);
            else
                result.conversionFailures.push_back(ConversionFailure{
                    EntityKind::ApproachTransition, id.value(), converted.errors() });
        }
    }

    // --- RunwayProcedureTransition (Sid puis Star) --- dernière catégorie,
    // dépend de Runway et de Procedure (du même kind), toutes deux déjà peuplées.
    {
        const IdentResolver resolver(result.repository);
        for (const RunwayProcedureTransitionId id : project.sidRunwayProcedureTransitions().order()) {
            const UserRunwayProcedureTransition* userTransition = project.sidRunwayProcedureTransitions().find(id);
            if (!userTransition)
                continue;
            const ConversionResult<RunwayProcedureTransition> converted =
                convertRunwayProcedureTransition(*userTransition, ProcedureKind::Sid, resolver);
            if (converted.isOk())
                result.repository.sidRunwayProcedureTransitions().add(converted.value(), id);
            else
                result.conversionFailures.push_back(ConversionFailure{
                    EntityKind::SidRunwayProcedureTransition, id.value(), converted.errors() });
        }
        for (const RunwayProcedureTransitionId id : project.starRunwayProcedureTransitions().order()) {
            const UserRunwayProcedureTransition* userTransition = project.starRunwayProcedureTransitions().find(id);
            if (!userTransition)
                continue;
            const ConversionResult<RunwayProcedureTransition> converted =
                convertRunwayProcedureTransition(*userTransition, ProcedureKind::Star, resolver);
            if (converted.isOk())
                result.repository.starRunwayProcedureTransitions().add(converted.value(), id);
            else
                result.conversionFailures.push_back(ConversionFailure{
                    EntityKind::StarRunwayProcedureTransition, id.value(), converted.errors() });
        }
    }

    const Validator validator;
    result.validationDiagnostics = validator.validate(result.repository);

    return result;
}

} // namespace navstud::conversion
