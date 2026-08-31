#pragma once

// ============================================================================
// ProjectRepository.h
// Stockage central du projet : une table par famille d'entité, un allocateur
// d'ID dédié par table. Aucune validation métier ici — c'est le rôle du
// Validator, en aval, qui consultera ce repository en lecture seule.
// ============================================================================

#include "Entities.h"

#include <QHash>
#include <QVector>

namespace navstud::model {

// ============================================================================
// IdAllocator<Tag>
// ------------------------------------------------------------------------------------------------------------------------------------------
// Distribue des Id<Tag> jamais réutilisés. Deux façons de peupler une table :
//   - allocate() : nouvel id, pour une saisie manuelle dans l'UI
//   - reserve(id) : un id déjà connu, importé depuis un .txt existant, le
//     fichier mondial, ou le classeur Excel — fait avancer le compteur pour
//     qu'un futur allocate() ne puisse jamais entrer en collision avec un id
//     importé.
// ============================================================================
template <typename Tag>
class IdAllocator
{
public:
    explicit IdAllocator(qint32 startAt = 1) noexcept : mNext(startAt) {}

    Id<Tag> allocate() noexcept { return Id<Tag>(mNext++); }

    void reserve(Id<Tag> id) noexcept
    {
        if (id.isValid() && id.value() >= mNext)
            mNext = id.value() + 1;
    }

private:
    qint32 mNext;
};

// ============================================================================
// EntityTable<Tag, Entity>
// ------------------------------------------------------------------------------------------------------------------------------------------
// Association Id<Tag> -> Entity, avec un ordre d'insertion préservé
// séparément du QHash (qui n'a aucun ordre garanti). Cet ordre est important :
// il pilote l'ordre des lignes dans le fichier _Xxx.txt généré par le Writer,
// pour que deux générations successives sans modification produisent un
// fichier identique (diff vide) — condition du test de non-régression par
// comparaison avec les .txt existants.
//
// Chaque EntityTable possède son PROPRE IdAllocator. Deux EntityTable
// différentes utilisant le même Tag (ex. Procedure côté Sid et côté Star,
// cf. ProjectRepository) ont donc des compteurs totalement indépendants —
// c'est voulu : un id 5 peut légitimement exister dans les deux tables
// simultanément sans collision, exactement comme le fait le fichier mondial
// avec ses sections [DEPARTURES]/[ARRIVALS] séparées.
// ============================================================================
template <typename Tag, typename Entity>
class EntityTable
{
public:
    explicit EntityTable(qint32 startAt = 1) : mAllocator(startAt) {}

    // Ajoute une nouvelle entité. Si id n'est pas fourni, un id est alloué.
    // Si id est fourni (import), il est réservé pour ne jamais être réémis.
    Id<Tag> add(Entity entity, Id<Tag> id = Id<Tag>::invalid())
    {
        const Id<Tag> assigned = id.isValid() ? id : mAllocator.allocate();
        mAllocator.reserve(assigned);
        mOrder.push_back(assigned);
        mEntities.insert(assigned, std::move(entity));
        return assigned;
    }

    bool remove(Id<Tag> id)
    {
        if (mEntities.remove(id) == 0)
            return false;
        mOrder.removeOne(id);
        return true;
    }

    const Entity* find(Id<Tag> id) const
    {
        const auto it = mEntities.constFind(id);
        return it == mEntities.constEnd() ? nullptr : &it.value();
    }

    Entity* find(Id<Tag> id)
    {
        const auto it = mEntities.find(id);
        return it == mEntities.end() ? nullptr : &it.value();
    }

    // Édition en place (usage typique : QAbstractTableModel::setData)
    bool update(Id<Tag> id, Entity entity)
    {
        auto it = mEntities.find(id);
        if (it == mEntities.end())
            return false;
        it.value() = std::move(entity);
        return true;
    }

    bool contains(Id<Tag> id) const { return mEntities.contains(id); }
    int  count() const { return mEntities.size(); }

    // Ordre d'insertion — c'est CET ordre que le Writer doit suivre.
    const QVector<Id<Tag>>& order() const { return mOrder; }

    // Renumérote TOUTES les entités de cette table pour qu'elles reprennent
    // une séquence contiguë à partir de newStart, dans leur ordre
    // d'insertion ACTUEL (jamais réordonnées, jamais réécrites autrement que
    // sur leur id numérique) — utilisé quand le fichier mondial associé à un
    // projet existant est mis à jour et que ses "# Count:" ont avancé : le
    // projet déjà saisi doit se réaligner sur la nouvelle continuité, sans
    // perdre son contenu ni casser ses références (celles-ci passent
    // TOUJOURS par l'ident texte entre structures, jamais par cet id
    // numérique — cf. UserEntities.h, les quatre schémas de résolution).
    void renumberFrom(qint32 newStart)
    {
        QHash<Id<Tag>, Entity> renumbered;
        QVector<Id<Tag>>       renumberedOrder;
        renumbered.reserve(mEntities.size());
        renumberedOrder.reserve(mOrder.size());

        qint32 next = newStart;
        for (const Id<Tag>& oldId : mOrder) {
            const auto it = mEntities.constFind(oldId);
            if (it == mEntities.constEnd())
                continue; // ne devrait pas arriver (mOrder et mEntities toujours en phase), tolérant par prudence
            const Id<Tag> newId(next++);
            renumbered.insert(newId, it.value());
            renumberedOrder.push_back(newId);
        }

        mEntities  = std::move(renumbered);
        mOrder     = std::move(renumberedOrder);
        mAllocator = IdAllocator<Tag>(next); // prochain id disponible pour une saisie future
    }

private:
    QHash<Id<Tag>, Entity> mEntities;
    QVector<Id<Tag>>       mOrder;
    IdAllocator<Tag>       mAllocator;
};

// ============================================================================
// ProjectRepository
// ------------------------------------------------------------------------------------------------------------------------------------------
// Une table par fichier _Xxx.txt cible, SAUF Procedure, ProcedureTransition
// et RunwayProcedureTransition qui en ont chacune DEUX (Sid/Star) — cf. la
// correction dans Entities.h (ProcedureKind) : le fichier mondial confirme
// que SID et STAR ont des compteurs d'ID indépendants, pas un espace
// partagé. Chaque paire est exposée à la fois par des accesseurs nommés
// (sidProcedures()/starProcedures(), pratiques quand le kind est connu à la
// lecture du code) et par un accesseur paramétré (procedures(kind), pratique
// dans une boucle générique sur les deux kinds).
// ============================================================================
class ProjectRepository
{
public:
    EntityTable<PointTag, Point>&                             points()                     { return mPoints; }
    const EntityTable<PointTag, Point>&                       points() const               { return mPoints; }

    EntityTable<WaypointTag, Waypoint>&                       waypoints()                  { return mWaypoints; }
    const EntityTable<WaypointTag, Waypoint>&                 waypoints() const            { return mWaypoints; }

    EntityTable<NavaidTag, Navaid>&                           navaids()                    { return mNavaids; }
    const EntityTable<NavaidTag, Navaid>&                     navaids() const              { return mNavaids; }

    EntityTable<AirportTag, Airport>&                         airports()                   { return mAirports; }
    const EntityTable<AirportTag, Airport>&                   airports() const             { return mAirports; }

    EntityTable<RunwayTag, Runway>&                           runways()                    { return mRunways; }
    const EntityTable<RunwayTag, Runway>&                     runways() const              { return mRunways; }

    EntityTable<LegSequenceTag, LegSequence>&                 legSequences()               { return mLegSequences; }
    const EntityTable<LegSequenceTag, LegSequence>&           legSequences() const         { return mLegSequences; }

    EntityTable<LegTag, Leg>&                                 legs()                       { return mLegs; }
    const EntityTable<LegTag, Leg>&                           legs() const                 { return mLegs; }

    EntityTable<ApproachTag, Approach>&                       approaches()                 { return mApproaches; }
    const EntityTable<ApproachTag, Approach>&                 approaches() const           { return mApproaches; }

    EntityTable<ApproachTransitionTag, ApproachTransition>&   approachTransitions()        { return mApproachTransitions; }
    const EntityTable<ApproachTransitionTag, ApproachTransition>& approachTransitions() const { return mApproachTransitions; }

    // --- Procedure : deux tables indépendantes, Sid et Star ---
    EntityTable<ProcedureTag, Procedure>&       sidProcedures()        { return mSidProcedures; }
    const EntityTable<ProcedureTag, Procedure>& sidProcedures() const  { return mSidProcedures; }
    EntityTable<ProcedureTag, Procedure>&       starProcedures()       { return mStarProcedures; }
    const EntityTable<ProcedureTag, Procedure>& starProcedures() const { return mStarProcedures; }

    EntityTable<ProcedureTag, Procedure>& procedures(ProcedureKind kind)
    {
        return kind == ProcedureKind::Sid ? mSidProcedures : mStarProcedures;
    }
    const EntityTable<ProcedureTag, Procedure>& procedures(ProcedureKind kind) const
    {
        return kind == ProcedureKind::Sid ? mSidProcedures : mStarProcedures;
    }

    // --- ProcedureTransition : deux tables indépendantes, Sid et Star ---
    EntityTable<ProcedureTransitionTag, ProcedureTransition>&       sidProcedureTransitions()        { return mSidProcedureTransitions; }
    const EntityTable<ProcedureTransitionTag, ProcedureTransition>& sidProcedureTransitions() const  { return mSidProcedureTransitions; }
    EntityTable<ProcedureTransitionTag, ProcedureTransition>&       starProcedureTransitions()        { return mStarProcedureTransitions; }
    const EntityTable<ProcedureTransitionTag, ProcedureTransition>& starProcedureTransitions() const  { return mStarProcedureTransitions; }

    EntityTable<ProcedureTransitionTag, ProcedureTransition>& procedureTransitions(ProcedureKind kind)
    {
        return kind == ProcedureKind::Sid ? mSidProcedureTransitions : mStarProcedureTransitions;
    }
    const EntityTable<ProcedureTransitionTag, ProcedureTransition>& procedureTransitions(ProcedureKind kind) const
    {
        return kind == ProcedureKind::Sid ? mSidProcedureTransitions : mStarProcedureTransitions;
    }

    // --- RunwayProcedureTransition : deux tables indépendantes, Sid et Star ---
    EntityTable<RunwayProcedureTransitionTag, RunwayProcedureTransition>&       sidRunwayProcedureTransitions()        { return mSidRunwayProcedureTransitions; }
    const EntityTable<RunwayProcedureTransitionTag, RunwayProcedureTransition>& sidRunwayProcedureTransitions() const  { return mSidRunwayProcedureTransitions; }
    EntityTable<RunwayProcedureTransitionTag, RunwayProcedureTransition>&       starRunwayProcedureTransitions()       { return mStarRunwayProcedureTransitions; }
    const EntityTable<RunwayProcedureTransitionTag, RunwayProcedureTransition>& starRunwayProcedureTransitions() const { return mStarRunwayProcedureTransitions; }

    EntityTable<RunwayProcedureTransitionTag, RunwayProcedureTransition>& runwayProcedureTransitions(ProcedureKind kind)
    {
        return kind == ProcedureKind::Sid ? mSidRunwayProcedureTransitions : mStarRunwayProcedureTransitions;
    }
    const EntityTable<RunwayProcedureTransitionTag, RunwayProcedureTransition>& runwayProcedureTransitions(ProcedureKind kind) const
    {
        return kind == ProcedureKind::Sid ? mSidRunwayProcedureTransitions : mStarRunwayProcedureTransitions;
    }

    // --------------------------------------------------------------------------------------------------------------------------------
    // Requêtes de traversée utiles au Writer et au Validator.
    // Ce ne sont volontairement PAS des règles de validation (aucune ici ne
    // signale d'erreur) — seulement des vues sur les tables, que le
    // Validator et le Writer peuvent tous les deux réutiliser sans dupliquer
    // la logique de traversée.
    // --------------------------------------------------------------------------------------------------------------------------------

    // Toutes les Procedure d'un kind donné, dans l'ordre d'insertion — c'est
    // la liste que le Writer sérialise dans _ProcedureSID.txt ou
    // _ProcedureSTAR.txt. Simple passe-plat vers procedures(kind).order()
    // depuis que kind sélectionne une table entière plutôt qu'un champ.
    QVector<ProcedureId> proceduresByKind(ProcedureKind kind) const
    {
        return procedures(kind).order();
    }

    // Toutes les RunwayProcedureTransition (du kind donné) référençant une
    // Procedure donnée. kind est requis : un ProcedureId seul est ambigu
    // depuis que Sid et Star ont des compteurs indépendants (le même id
    // peut exister des deux côtés) — cf. Validator, qui connaît toujours le
    // kind au moment de l'appel.
    QVector<RunwayProcedureTransitionId> runwayProcedureTransitionsFor(ProcedureKind kind, ProcedureId procedureId) const
    {
        QVector<RunwayProcedureTransitionId> result;
        const auto& table = runwayProcedureTransitions(kind);
        for (const RunwayProcedureTransitionId id : table.order()) {
            if (const RunwayProcedureTransition* t = table.find(id); t && t->procedureId == procedureId)
                result.push_back(id);
        }
        return result;
    }

private:
    EntityTable<PointTag, Point>                                         mPoints;
    EntityTable<WaypointTag, Waypoint>                                   mWaypoints;
    EntityTable<NavaidTag, Navaid>                                       mNavaids;
    EntityTable<AirportTag, Airport>                                     mAirports;
    EntityTable<RunwayTag, Runway>                                       mRunways;
    EntityTable<LegSequenceTag, LegSequence>                             mLegSequences;
    EntityTable<LegTag, Leg>                                             mLegs;
    EntityTable<ApproachTag, Approach>                                   mApproaches;
    EntityTable<ApproachTransitionTag, ApproachTransition>               mApproachTransitions;

    EntityTable<ProcedureTag, Procedure>                                 mSidProcedures;
    EntityTable<ProcedureTag, Procedure>                                 mStarProcedures;
    EntityTable<ProcedureTransitionTag, ProcedureTransition>             mSidProcedureTransitions;
    EntityTable<ProcedureTransitionTag, ProcedureTransition>             mStarProcedureTransitions;
    EntityTable<RunwayProcedureTransitionTag, RunwayProcedureTransition> mSidRunwayProcedureTransitions;
    EntityTable<RunwayProcedureTransitionTag, RunwayProcedureTransition> mStarRunwayProcedureTransitions;
};

} // namespace navstud::model
