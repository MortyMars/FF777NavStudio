#include "IdentResolver.h"

namespace navstud::conversion {

using namespace navstud::model;

namespace {

// -----------------------------------------------------------------------------------------------------------
// Parcourt une table d'entités pour retrouver celle dont le champ membre
// vaut la clé recherchée, et retourne l'identifiant du propriétaire.
// Résolution propriétaire générique : parcourt table, retourne l'id de la
// première entité dont le champ désigné par fieldOfInterest (via un pointeur
// membre) vaut needle. Un simple parcours linéaire suffit à l'échelle d'un
// projet (quelques centaines d'entités au plus) — pas de sur-ingénierie.
template <typename OwnerTag, typename OwnerEntity, typename KeyTag>
std::optional<Id<OwnerTag>> findOwnerByField(const EntityTable<OwnerTag, OwnerEntity>& table,
                                              Id<KeyTag> OwnerEntity::* field,
                                              Id<KeyTag> needle)
{
    for (const Id<OwnerTag> id : table.order()) {
        const OwnerEntity* entity = table.find(id);
        if (entity && (entity->*field) == needle)
            return id;
    }
    return std::nullopt;
}

} // namespace

// -----------------------------------------------------------------------------------------------------------
// Construit le résolveur en indexant tous les points et séquences de legs
// du dépôt par identifiant.
IdentResolver::IdentResolver(const ProjectRepository& repo)
    : mRepo(repo)
{
    for (const PointId id : repo.points().order()) {
        if (const Point* p = repo.points().find(id))
            mPointsByIdent.insert(p->ident, id);
    }
    for (const LegSequenceId id : repo.legSequences().order()) {
        if (const LegSequence* ls = repo.legSequences().find(id))
            mLegSequencesByIdent.insert(ls->ident, id);
    }
}

// -----------------------------------------------------------------------------------------------------------
// Retourne l'id du point correspondant à l'identifiant donné, sinon vide.
std::optional<PointId> IdentResolver::point(const QString& ident) const
{
    const auto it = mPointsByIdent.constFind(ident);
    if (it == mPointsByIdent.constEnd())
        return std::nullopt;
    return it.value();
}

// -----------------------------------------------------------------------------------------------------------
// Retourne l'id de la séquence de legs correspondant à l'ident, sinon vide.
std::optional<LegSequenceId> IdentResolver::legSequence(const QString& ident) const
{
    const auto it = mLegSequencesByIdent.constFind(ident);
    if (it == mLegSequencesByIdent.constEnd())
        return std::nullopt;
    return it.value();
}

// -----------------------------------------------------------------------------------------------------------
// Retrouve l'aéroport dont le point associé porte l'ident donné.
std::optional<AirportId> IdentResolver::airportViaPointIdent(const QString& ident) const
{
    const auto pid = point(ident);
    if (!pid)
        return std::nullopt;
    return findOwnerByField(mRepo.airports(), &Airport::pointId, *pid);
}

// -----------------------------------------------------------------------------------------------------------
// Retrouve la piste dont le point associé porte l'ident donné.
std::optional<RunwayId> IdentResolver::runwayViaPointIdent(const QString& ident) const
{
    const auto pid = point(ident);
    if (!pid)
        return std::nullopt;
    return findOwnerByField(mRepo.runways(), &Runway::pointId, *pid);
}

// -----------------------------------------------------------------------------------------------------------
// Retrouve l'aide radio dont le point associé porte l'ident donné.
std::optional<NavaidId> IdentResolver::navaidViaPointIdent(const QString& ident) const
{
    const auto pid = point(ident);
    if (!pid)
        return std::nullopt;
    return findOwnerByField(mRepo.navaids(), &Navaid::pointId, *pid);
}

// -----------------------------------------------------------------------------------------------------------
// Retrouve la procédure du kind donné dont la séquence porte l'ident.
std::optional<ProcedureId> IdentResolver::procedureViaLegSequenceIdent(ProcedureKind kind, const QString& ident) const
{
    const auto lsid = legSequence(ident);
    if (!lsid)
        return std::nullopt;
    return findOwnerByField(mRepo.procedures(kind), &Procedure::legSequenceId, *lsid);
}

// -----------------------------------------------------------------------------------------------------------
// Retrouve l'approche dont la séquence de legs porte l'ident donné.
std::optional<ApproachId> IdentResolver::approachViaLegSequenceIdent(const QString& ident) const
{
    const auto lsid = legSequence(ident);
    if (!lsid)
        return std::nullopt;
    return findOwnerByField(mRepo.approaches(), &Approach::legSequenceId, *lsid);
}

} // namespace navstud::conversion
