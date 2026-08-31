#pragma once

// ============================================================================
// UserProject.h
// Pendant de model::ProjectRepository pour la SAISIE utilisateur : une table
// par structure, réutilisant model::EntityTable/model::IdAllocator tels
// quels (ils sont déjà génériques sur le type d'entité — nul besoin de les
// dupliquer pour UserXxx).
//
// CONSÉQUENCE IMPORTANTE DE CE CHOIX : puisque UserProject utilise le MÊME
// Tag (donc le même espace d'Id<Tag>) que le modèle cible, l'ID d'une ligne
// est attribué dès sa création côté saisie — pas seulement au moment où la
// régénération réussit à la convertir. Une ligne de saisie encore
// incomplète ou non résolvable connaît donc déjà le numéro qu'elle portera
// dans le fichier texte final, ce qui permet à l'UI d'afficher l'aperçu bas
// ("la ligne telle qu'elle sera créée") en même temps que le masque de
// saisie haut, sans attendre une régénération complète du projet.
//
// UserProject ne fait AUCUNE validation ni résolution — c'est le rôle du
// futur Regenerator, qui consomme un UserProject en lecture seule pour
// reconstruire un model::ProjectRepository.
// ============================================================================

#include "ProjectRepository.h" // réutilise model::EntityTable, model::IdAllocator, les Tag
#include "UserEntities.h"

namespace navstud::userdata {

// ============================================================================
// StartingIndices
// ------------------------------------------------------------------------------------------------------------------------------------------
// Un compteur de départ par table — c'est le point d'ancrage de la
// continuité d'indexation avec le fichier mondial (NB2 du projet) : chaque
// valeur correspond au "# Count: N" lu dans la section correspondante du
// fichier mondial. Vaut {1, 1, ...} par défaut (projet vierge / tests).
//
// Sid et Star ont des compteurs INDÉPENDANTS (cf. Entities.h, ProcedureKind)
// — confirmé par le fichier mondial où [DEPARTURES] et [ARRIVALS] ont
// chacune leur propre "# Count", d'où les deux champs distincts ci-dessous
// pour Procedure/ProcedureTransition/RunwayProcedureTransition.
// ============================================================================
struct StartingIndices
{
    qint32 point                          = 1;
    qint32 waypoint                       = 1;
    qint32 navaid                         = 1;
    qint32 airport                        = 1;
    qint32 runway                         = 1;
    qint32 legSequence                    = 1;
    qint32 leg                            = 1;
    qint32 approach                       = 1;
    qint32 approachTransition             = 1;
    qint32 sidProcedure                   = 1;
    qint32 starProcedure                  = 1;
    qint32 sidProcedureTransition         = 1;
    qint32 starProcedureTransition        = 1;
    qint32 sidRunwayProcedureTransition   = 1;
    qint32 starRunwayProcedureTransition  = 1;
};

class UserProject
{
public:
    explicit UserProject(const StartingIndices& start = StartingIndices())
        : mPoints(start.point)
        , mWaypoints(start.waypoint)
        , mNavaids(start.navaid)
        , mAirports(start.airport)
        , mRunways(start.runway)
        , mLegSequences(start.legSequence)
        , mLegs(start.leg)
        , mApproaches(start.approach)
        , mApproachTransitions(start.approachTransition)
        , mSidProcedures(start.sidProcedure)
        , mStarProcedures(start.starProcedure)
        , mSidProcedureTransitions(start.sidProcedureTransition)
        , mStarProcedureTransitions(start.starProcedureTransition)
        , mSidRunwayProcedureTransitions(start.sidRunwayProcedureTransition)
        , mStarRunwayProcedureTransitions(start.starRunwayProcedureTransition)
    {
    }

    model::EntityTable<model::PointTag, UserPoint>&             points()                { return mPoints; }
    const model::EntityTable<model::PointTag, UserPoint>&       points() const          { return mPoints; }

    model::EntityTable<model::WaypointTag, UserWaypoint>&       waypoints()             { return mWaypoints; }
    const model::EntityTable<model::WaypointTag, UserWaypoint>& waypoints() const       { return mWaypoints; }

    model::EntityTable<model::NavaidTag, UserNavaid>&           navaids()               { return mNavaids; }
    const model::EntityTable<model::NavaidTag, UserNavaid>&     navaids() const         { return mNavaids; }

    model::EntityTable<model::AirportTag, UserAirport>&         airports()              { return mAirports; }
    const model::EntityTable<model::AirportTag, UserAirport>&   airports() const        { return mAirports; }

    model::EntityTable<model::RunwayTag, UserRunway>&           runways()               { return mRunways; }
    const model::EntityTable<model::RunwayTag, UserRunway>&     runways() const         { return mRunways; }

    model::EntityTable<model::LegSequenceTag, UserLegSequence>&       legSequences()       { return mLegSequences; }
    const model::EntityTable<model::LegSequenceTag, UserLegSequence>& legSequences() const { return mLegSequences; }

    model::EntityTable<model::LegTag, UserLeg>&                 legs()                  { return mLegs; }
    const model::EntityTable<model::LegTag, UserLeg>&           legs() const            { return mLegs; }

    model::EntityTable<model::ApproachTag, UserApproach>&             approaches()             { return mApproaches; }
    const model::EntityTable<model::ApproachTag, UserApproach>&       approaches() const       { return mApproaches; }

    model::EntityTable<model::ApproachTransitionTag, UserApproachTransition>&       approachTransitions()       { return mApproachTransitions; }
    const model::EntityTable<model::ApproachTransitionTag, UserApproachTransition>& approachTransitions() const { return mApproachTransitions; }

    // --- Procedure : deux tables indépendantes, Sid et Star (cf. StartingIndices) ---
    model::EntityTable<model::ProcedureTag, UserProcedure>&       sidProcedures()        { return mSidProcedures; }
    const model::EntityTable<model::ProcedureTag, UserProcedure>& sidProcedures() const  { return mSidProcedures; }
    model::EntityTable<model::ProcedureTag, UserProcedure>&       starProcedures()       { return mStarProcedures; }
    const model::EntityTable<model::ProcedureTag, UserProcedure>& starProcedures() const { return mStarProcedures; }

    model::EntityTable<model::ProcedureTag, UserProcedure>& procedures(model::ProcedureKind kind)
    {
        return kind == model::ProcedureKind::Sid ? mSidProcedures : mStarProcedures;
    }
    const model::EntityTable<model::ProcedureTag, UserProcedure>& procedures(model::ProcedureKind kind) const
    {
        return kind == model::ProcedureKind::Sid ? mSidProcedures : mStarProcedures;
    }

    // --- ProcedureTransition : deux tables indépendantes ---
    model::EntityTable<model::ProcedureTransitionTag, UserProcedureTransition>&       sidProcedureTransitions()       { return mSidProcedureTransitions; }
    const model::EntityTable<model::ProcedureTransitionTag, UserProcedureTransition>& sidProcedureTransitions() const { return mSidProcedureTransitions; }
    model::EntityTable<model::ProcedureTransitionTag, UserProcedureTransition>&       starProcedureTransitions()       { return mStarProcedureTransitions; }
    const model::EntityTable<model::ProcedureTransitionTag, UserProcedureTransition>& starProcedureTransitions() const { return mStarProcedureTransitions; }

    model::EntityTable<model::ProcedureTransitionTag, UserProcedureTransition>& procedureTransitions(model::ProcedureKind kind)
    {
        return kind == model::ProcedureKind::Sid ? mSidProcedureTransitions : mStarProcedureTransitions;
    }
    const model::EntityTable<model::ProcedureTransitionTag, UserProcedureTransition>& procedureTransitions(model::ProcedureKind kind) const
    {
        return kind == model::ProcedureKind::Sid ? mSidProcedureTransitions : mStarProcedureTransitions;
    }

    // --- RunwayProcedureTransition : deux tables indépendantes ---
    model::EntityTable<model::RunwayProcedureTransitionTag, UserRunwayProcedureTransition>&       sidRunwayProcedureTransitions()       { return mSidRunwayProcedureTransitions; }
    const model::EntityTable<model::RunwayProcedureTransitionTag, UserRunwayProcedureTransition>& sidRunwayProcedureTransitions() const { return mSidRunwayProcedureTransitions; }
    model::EntityTable<model::RunwayProcedureTransitionTag, UserRunwayProcedureTransition>&       starRunwayProcedureTransitions()       { return mStarRunwayProcedureTransitions; }
    const model::EntityTable<model::RunwayProcedureTransitionTag, UserRunwayProcedureTransition>& starRunwayProcedureTransitions() const { return mStarRunwayProcedureTransitions; }

    model::EntityTable<model::RunwayProcedureTransitionTag, UserRunwayProcedureTransition>& runwayProcedureTransitions(model::ProcedureKind kind)
    {
        return kind == model::ProcedureKind::Sid ? mSidRunwayProcedureTransitions : mStarRunwayProcedureTransitions;
    }
    const model::EntityTable<model::RunwayProcedureTransitionTag, UserRunwayProcedureTransition>& runwayProcedureTransitions(model::ProcedureKind kind) const
    {
        return kind == model::ProcedureKind::Sid ? mSidRunwayProcedureTransitions : mStarRunwayProcedureTransitions;
    }

    // Renumérote LES 15 TABLES à partir de nouveaux compteurs de départ —
    // utilisé quand le fichier mondial associé au projet est mis à jour
    // (nouveau tirage mensuel) et que ses "# Count:" ont avancé depuis la
    // création du projet. Simple relais vers EntityTable::renumberFrom, une
    // fois par table ; l'ordre d'insertion de chaque table est préservé, et
    // aucune des références entre structures (toujours par ident texte,
    // jamais par cet id numérique) n'a besoin d'être touchée.
    void renumberFrom(const StartingIndices& newIndices)
    {
        mPoints.renumberFrom(newIndices.point);
        mWaypoints.renumberFrom(newIndices.waypoint);
        mNavaids.renumberFrom(newIndices.navaid);
        mAirports.renumberFrom(newIndices.airport);
        mRunways.renumberFrom(newIndices.runway);
        mLegSequences.renumberFrom(newIndices.legSequence);
        mLegs.renumberFrom(newIndices.leg);
        mApproaches.renumberFrom(newIndices.approach);
        mApproachTransitions.renumberFrom(newIndices.approachTransition);
        mSidProcedures.renumberFrom(newIndices.sidProcedure);
        mStarProcedures.renumberFrom(newIndices.starProcedure);
        mSidProcedureTransitions.renumberFrom(newIndices.sidProcedureTransition);
        mStarProcedureTransitions.renumberFrom(newIndices.starProcedureTransition);
        mSidRunwayProcedureTransitions.renumberFrom(newIndices.sidRunwayProcedureTransition);
        mStarRunwayProcedureTransitions.renumberFrom(newIndices.starRunwayProcedureTransition);
    }

private:
    model::EntityTable<model::PointTag, UserPoint>                             mPoints;
    model::EntityTable<model::WaypointTag, UserWaypoint>                       mWaypoints;
    model::EntityTable<model::NavaidTag, UserNavaid>                           mNavaids;
    model::EntityTable<model::AirportTag, UserAirport>                         mAirports;
    model::EntityTable<model::RunwayTag, UserRunway>                           mRunways;
    model::EntityTable<model::LegSequenceTag, UserLegSequence>                 mLegSequences;
    model::EntityTable<model::LegTag, UserLeg>                                 mLegs;
    model::EntityTable<model::ApproachTag, UserApproach>                       mApproaches;
    model::EntityTable<model::ApproachTransitionTag, UserApproachTransition>   mApproachTransitions;

    model::EntityTable<model::ProcedureTag, UserProcedure>                             mSidProcedures;
    model::EntityTable<model::ProcedureTag, UserProcedure>                             mStarProcedures;
    model::EntityTable<model::ProcedureTransitionTag, UserProcedureTransition>         mSidProcedureTransitions;
    model::EntityTable<model::ProcedureTransitionTag, UserProcedureTransition>         mStarProcedureTransitions;
    model::EntityTable<model::RunwayProcedureTransitionTag, UserRunwayProcedureTransition> mSidRunwayProcedureTransitions;
    model::EntityTable<model::RunwayProcedureTransitionTag, UserRunwayProcedureTransition> mStarRunwayProcedureTransitions;
};

} // namespace navstud::userdata
