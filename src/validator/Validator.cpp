#include "Validator.h"

#include <QHash>

namespace navstud::validator {

using namespace navstud::model;

namespace {

// -----------------------------------------------------------------------------------------------------------
// Vérifie qu'une clé étrangère obligatoire est spécifiée (non invalid()) et
// qu'elle existe dans la table cible, en consignant un diagnostic sinon.
// Clé étrangère obligatoire : doit être valide, et exister dans la table cible.
template <typename Tag, typename Entity>
void requireExists(const EntityTable<Tag, Entity>& table, Id<Tag> id,
                    EntityKind ownerKind, qint32 ownerId, const QString& fieldName,
                    QVector<Diagnostic>& out)
{
    if (!id.isValid()) {
        out.push_back(Diagnostic{
            Severity::Error, QStringLiteral("ref-required"),
            QStringLiteral("%1 requis mais non spécifié (-1)").arg(fieldName),
            EntityRef{ ownerKind, ownerId } });
        return;
    }
    if (!table.contains(id)) {
        out.push_back(Diagnostic{
            Severity::Error, QStringLiteral("ref-dangling"),
            QStringLiteral("%1 = %2 : référence introuvable").arg(fieldName).arg(id.value()),
            EntityRef{ ownerKind, ownerId } });
    }
}

// -----------------------------------------------------------------------------------------------------------
// Vérifie une clé étrangère optionnelle : invalid() est légitime, sinon la
// clé doit exister dans la table cible, sinon consigne un diagnostic.
// Clé étrangère optionnelle : invalid() = "non spécifié", légitime. Si
// renseignée, doit exister.
template <typename Tag, typename Entity>
void checkExistsIfValid(const EntityTable<Tag, Entity>& table, Id<Tag> id,
                         EntityKind ownerKind, qint32 ownerId, const QString& fieldName,
                         QVector<Diagnostic>& out)
{
    if (!id.isValid())
        return;
    if (!table.contains(id)) {
        out.push_back(Diagnostic{
            Severity::Error, QStringLiteral("ref-dangling"),
            QStringLiteral("%1 = %2 : référence introuvable").arg(fieldName).arg(id.value()),
            EntityRef{ ownerKind, ownerId } });
    }
}

// Les deux kinds à parcourir partout où Procedure/ProcedureTransition/
// RunwayProcedureTransition sont concernés — cf. Entities.h (ProcedureKind).
constexpr ProcedureKind kAllKinds[] = { ProcedureKind::Sid, ProcedureKind::Star };

// -----------------------------------------------------------------------------------------------------------
// Retourne l'EntityKind de la procédure selon son kind (SID ou STAR).
EntityKind procedureEntityKind(ProcedureKind kind)
{
    return kind == ProcedureKind::Sid ? EntityKind::SidProcedure : EntityKind::StarProcedure;
}

// -----------------------------------------------------------------------------------------------------------
// Retourne l'EntityKind de la transition de procédure selon son kind.
EntityKind procedureTransitionEntityKind(ProcedureKind kind)
{
    return kind == ProcedureKind::Sid ? EntityKind::SidProcedureTransition : EntityKind::StarProcedureTransition;
}

// -----------------------------------------------------------------------------------------------------------
// Retourne l'EntityKind de la transition piste/procédure selon son kind.
EntityKind runwayProcedureTransitionEntityKind(ProcedureKind kind)
{
    return kind == ProcedureKind::Sid ? EntityKind::SidRunwayProcedureTransition : EntityKind::StarRunwayProcedureTransition;
}

// -----------------------------------------------------------------------------------------------------------
// Retourne le libellé textuel ("SID" ou "STAR") du kind de procédure.
QString kindLabel(ProcedureKind kind)
{
    return kind == ProcedureKind::Sid ? QStringLiteral("SID") : QStringLiteral("STAR");
}

} // namespace

// -----------------------------------------------------------------------------------------------------------
// Contrôle l'intégrité référentielle de toutes les entités du dépôt : chaque
// clé étrangère doit pointer vers une entrée existante de la table cible.
void Validator::checkReferentialIntegrity(const ProjectRepository& repo, QVector<Diagnostic>& out) const
{
    for (const WaypointId id : repo.waypoints().order()) {
        const Waypoint* w = repo.waypoints().find(id);
        if (!w) continue;
        requireExists(repo.points(), w->pointId, EntityKind::Waypoint, id.value(), QStringLiteral("pointId"), out);
    }

    for (const NavaidId id : repo.navaids().order()) {
        const Navaid* n = repo.navaids().find(id);
        if (!n) continue;
        requireExists(repo.points(), n->pointId, EntityKind::Navaid, id.value(), QStringLiteral("pointId"), out);
        checkExistsIfValid(repo.navaids(), n->distanceNavaidId, EntityKind::Navaid, id.value(), QStringLiteral("distanceNavaidId"), out);
        // Hypothèse : un Navaid est toujours rattaché à une Runway dans ce projet
        // (uniquement des navaids d'approche à ce stade). À assouplir en
        // checkExistsIfValid le jour où un NDB/VOR enroute sans piste apparaît.
        requireExists(repo.runways(), n->runwayId, EntityKind::Navaid, id.value(), QStringLiteral("runwayId"), out);
    }

    for (const AirportId id : repo.airports().order()) {
        const Airport* a = repo.airports().find(id);
        if (!a) continue;
        requireExists(repo.points(), a->pointId, EntityKind::Airport, id.value(), QStringLiteral("pointId"), out);
    }

    for (const RunwayId id : repo.runways().order()) {
        const Runway* r = repo.runways().find(id);
        if (!r) continue;
        requireExists(repo.airports(), r->airportId, EntityKind::Runway, id.value(), QStringLiteral("airportId"), out);
        requireExists(repo.points(), r->pointId, EntityKind::Runway, id.value(), QStringLiteral("pointId"), out);
    }

    for (const LegId id : repo.legs().order()) {
        const Leg* l = repo.legs().find(id);
        if (!l) continue;
        requireExists(repo.legSequences(), l->legSequenceId, EntityKind::Leg, id.value(), QStringLiteral("legSequenceId"), out);
        checkExistsIfValid(repo.points(), l->pointId, EntityKind::Leg, id.value(), QStringLiteral("pointId"), out);
        checkExistsIfValid(repo.navaids(), l->navaidId, EntityKind::Leg, id.value(), QStringLiteral("navaidId"), out);
    }

    for (const ApproachId id : repo.approaches().order()) {
        const Approach* a = repo.approaches().find(id);
        if (!a) continue;
        requireExists(repo.runways(), a->runwayId, EntityKind::Approach, id.value(), QStringLiteral("runwayId"), out);
        requireExists(repo.legSequences(), a->legSequenceId, EntityKind::Approach, id.value(), QStringLiteral("legSequenceId"), out);
    }

    for (const ApproachTransitionId id : repo.approachTransitions().order()) {
        const ApproachTransition* t = repo.approachTransitions().find(id);
        if (!t) continue;
        requireExists(repo.approaches(), t->approachId, EntityKind::ApproachTransition, id.value(), QStringLiteral("approachId"), out);
        requireExists(repo.legSequences(), t->legSequenceId, EntityKind::ApproachTransition, id.value(), QStringLiteral("legSequenceId"), out);
    }

    // Procedure/ProcedureTransition/RunwayProcedureTransition : deux tables
    // indépendantes chacune (Sid/Star). procedureId doit toujours être
    // résolu contre la table du MÊME kind — jamais l'autre, puisque les deux
    // compteurs sont indépendants et qu'un même id peut exister des deux
    // côtés sans rapport l'un avec l'autre.
    for (const ProcedureKind kind : kAllKinds) {
        const EntityKind procKind = procedureEntityKind(kind);
        for (const ProcedureId id : repo.procedures(kind).order()) {
            const Procedure* p = repo.procedures(kind).find(id);
            if (!p) continue;
            requireExists(repo.airports(), p->airportId, procKind, id.value(), QStringLiteral("airportId"), out);
            requireExists(repo.legSequences(), p->legSequenceId, procKind, id.value(), QStringLiteral("legSequenceId"), out);
        }

        const EntityKind ptKind = procedureTransitionEntityKind(kind);
        for (const ProcedureTransitionId id : repo.procedureTransitions(kind).order()) {
            const ProcedureTransition* t = repo.procedureTransitions(kind).find(id);
            if (!t) continue;
            requireExists(repo.procedures(kind), t->procedureId, ptKind, id.value(), QStringLiteral("procedureId"), out);
            requireExists(repo.legSequences(), t->legSequenceId, ptKind, id.value(), QStringLiteral("legSequenceId"), out);
        }

        const EntityKind rptKind = runwayProcedureTransitionEntityKind(kind);
        for (const RunwayProcedureTransitionId id : repo.runwayProcedureTransitions(kind).order()) {
            const RunwayProcedureTransition* t = repo.runwayProcedureTransitions(kind).find(id);
            if (!t) continue;
            requireExists(repo.runways(), t->runwayId, rptKind, id.value(), QStringLiteral("runwayId"), out);
            requireExists(repo.procedures(kind), t->procedureId, rptKind, id.value(), QStringLiteral("procedureId"), out);
            checkExistsIfValid(repo.procedures(kind), t->engineOutProcedureId, rptKind, id.value(), QStringLiteral("engineOutProcedureId"), out);
            // legSequenceId volontairement absent d'ici : la règle "jamais invalid()"
            // (checkRunwayProcedureTransitionLegSequence) est plus stricte et plus
            // parlante que le générique "requis" — pas de doublon de diagnostic.
            checkExistsIfValid(repo.legSequences(), t->legSequenceId, rptKind, id.value(), QStringLiteral("legSequenceId"), out);
        }
    }
}

// ============================================================================
// Règle bloquante n°1 et n°2 (cf. debug LFFA) :
//   1. RunwayProcedureTransition::legSequenceId ne doit jamais être invalid()
//      -> fait planter X-Plane.
//   2. RunwayProcedureTransition::legSequenceId ne doit jamais être égal au
//      legSequenceId de sa Procedure parente -> double insertion des legs,
//      doublons de points de cheminement sur le ND.
// ============================================================================
// -----------------------------------------------------------------------------------------------------------
// Vérifie que la legSequence d'une RunwayProcedureTransition est valide et
// ne réutilise pas la séquence cœur de sa procédure parente (doublon ND).
void Validator::checkRunwayProcedureTransitionLegSequence(const ProjectRepository& repo, QVector<Diagnostic>& out) const
{
    for (const ProcedureKind kind : kAllKinds) {
        const EntityKind rptKind = runwayProcedureTransitionEntityKind(kind);
        for (const RunwayProcedureTransitionId id : repo.runwayProcedureTransitions(kind).order()) {
            const RunwayProcedureTransition* t = repo.runwayProcedureTransitions(kind).find(id);
            if (!t)
                continue;

            if (!t->legSequenceId.isValid()) {
                out.push_back(Diagnostic{
                    Severity::Error, QStringLiteral("rpt-legseq-invalid"),
                    QStringLiteral("legSequenceId ne doit jamais être invalid() (-1) — fait planter X-Plane"),
                    EntityRef{ rptKind, id.value() } });
                continue; // rien de plus à vérifier sur une référence déjà invalide
            }

            if (const Procedure* parent = repo.procedures(kind).find(t->procedureId)) {
                if (t->legSequenceId == parent->legSequenceId) {
                    out.push_back(Diagnostic{
                        Severity::Error, QStringLiteral("rpt-legseq-reuses-parent"),
                        QStringLiteral("legSequenceId=%1 réutilise la séquence \"cœur\" de la Procedure %2 (%3) — "
                                       "provoque un doublon de legs sur le ND. Créer une LegSequence "
                                       "d'ancrage dédiée.")
                            .arg(t->legSequenceId.value()).arg(t->procedureId.value()).arg(kindLabel(kind)),
                        EntityRef{ rptKind, id.value() } });
                }
            }
            // Si la Procedure parente est elle-même introuvable, c'est déjà
            // signalé par checkReferentialIntegrity — pas la peine de dupliquer.
        }
    }
}

// ============================================================================
// Règle n°3 (cf. debug LFFA — cas des 3 STAR sur 7 qui en manquaient) :
// chaque Procedure doit avoir au moins une RunwayProcedureTransition (du
// MÊME kind) pour apparaître correctement au MCDU.
//
// Sévérité Warning et non Error : en cours d'édition, une Procedure peut
// légitimement exister sans sa RunwayProcedureTransition le temps de la
// saisir. À faire remonter en Error dans une passe de validation "pré-export"
// dédiée si besoin, mais pas ici pour ne pas polluer l'édition interactive.
// ============================================================================
// -----------------------------------------------------------------------------------------------------------
// Signale (en avenir Warning) toute procédure sans RunwayProcedureTransition
// du même kind, qui n'apparaîtrait pas correctement au MCDU.
void Validator::checkProcedureHasRunwayProcedureTransition(const ProjectRepository& repo, QVector<Diagnostic>& out) const
{
    for (const ProcedureKind kind : kAllKinds) {
        const EntityKind procKind = procedureEntityKind(kind);
        for (const ProcedureId id : repo.procedures(kind).order()) {
            const Procedure* p = repo.procedures(kind).find(id);
            if (!p)
                continue;
            if (repo.runwayProcedureTransitionsFor(kind, id).isEmpty()) {
                out.push_back(Diagnostic{
                    Severity::Warning, QStringLiteral("procedure-missing-rpt"),
                    QStringLiteral("%1 sans aucune RunwayProcedureTransition — n'apparaîtra pas correctement au MCDU").arg(kindLabel(kind)),
                    EntityRef{ procKind, id.value() } });
            }
        }
    }
}

// ============================================================================
// Règle n°4, heuristique non bloquante : le premier point d'un SID doit
// correspondre au numéro de la piste associée (ex. DPR07 sur "07", pas sur
// son réciproque "25").
// ============================================================================
// -----------------------------------------------------------------------------------------------------------
// Heuristique non bloquante : le premier point d'un SID doit correspondre au
// numéro de piste associé (alerte sinon, ex. réciproque utilisé).
void Validator::checkSidInitialPointMatchesRunway(const ProjectRepository& repo, QVector<Diagnostic>& out) const
{
    for (const ProcedureId id : repo.proceduresByKind(ProcedureKind::Sid)) {
        const Procedure* sid = repo.sidProcedures().find(id);
        if (!sid)
            continue;

        // Premier Leg de la LegSequence "cœur" du SID, dans l'ordre d'insertion
        // (cf. EntityTable::order() — c'est aussi l'ordre de vol).
        const Leg* firstLeg = nullptr;
        for (const LegId legId : repo.legs().order()) {
            const Leg* l = repo.legs().find(legId);
            if (l && l->legSequenceId == sid->legSequenceId) {
                firstLeg = l;
                break;
            }
        }
        if (!firstLeg || !firstLeg->pointId.isValid())
            continue;

        const Point* firstPoint = repo.points().find(firstLeg->pointId);
        if (!firstPoint)
            continue; // référence déjà signalée par checkReferentialIntegrity

        for (const RunwayProcedureTransitionId rptId : repo.runwayProcedureTransitionsFor(ProcedureKind::Sid, id)) {
            const RunwayProcedureTransition* rpt = repo.sidRunwayProcedureTransitions().find(rptId);
            if (!rpt)
                continue;
            const Runway* runway = repo.runways().find(rpt->runwayId);
            if (!runway)
                continue;
            const Point* runwayPoint = repo.points().find(runway->pointId);
            if (!runwayPoint)
                continue;

            if (firstPoint->ident != runwayPoint->ident) {
                out.push_back(Diagnostic{
                    Severity::Warning, QStringLiteral("sid-initial-point-mismatch"),
                    QStringLiteral("Le premier point du SID (\"%1\") ne correspond pas au numéro de piste "
                                   "attendu (\"%2\") — vérifier qu'il ne s'agit pas du réciproque")
                        .arg(firstPoint->ident, runwayPoint->ident),
                    EntityRef{ EntityKind::SidProcedure, id.value() } });
            }
        }
    }
}

// -----------------------------------------------------------------------------------------------------------
// Exécute toutes les règles de validation sur le dépôt et retourne la liste
// des diagnostics, règles bloquantes d'abord puis heuristiques.
QVector<Diagnostic> Validator::validate(const ProjectRepository& repo) const
{
    QVector<Diagnostic> diagnostics;
    checkReferentialIntegrity(repo, diagnostics);
    checkRunwayProcedureTransitionLegSequence(repo, diagnostics);
    checkProcedureHasRunwayProcedureTransition(repo, diagnostics);
    checkSidInitialPointMatchesRunway(repo, diagnostics);
    checkUniquePointIdent(repo, diagnostics);
    return diagnostics;
}

// ============================================================================
// Un Point.ident dupliqué rend la résolution par ident ambiguë : deux Point
// différents portant le même texte, et rien ne dit lequel sera retrouvé par
// IdentResolver (dernier inséré dans son QHash). Signale CHAQUE point
// impliqué dans le doublon, pas seulement le second — l'utilisateur doit
// pouvoir corriger n'importe lequel des deux.
// ============================================================================
// -----------------------------------------------------------------------------------------------------------
// Signale (en Warning) chaque point dont l'ident est dupliqué, car la
// résolution par ident ne peut alors retenir qu'un seul des candidats.
void Validator::checkUniquePointIdent(const ProjectRepository& repo, QVector<Diagnostic>& out) const
{
    QHash<QString, QVector<PointId>> idsByIdent;
    for (const PointId id : repo.points().order()) {
        if (const Point* p = repo.points().find(id))
            idsByIdent[p->ident].push_back(id);
    }

    for (auto it = idsByIdent.constBegin(); it != idsByIdent.constEnd(); ++it) {
        if (it.value().size() <= 1)
            continue;
        for (const PointId id : it.value()) {
            out.push_back(Diagnostic{
                Severity::Warning, QStringLiteral("point-duplicate-ident"),
                QStringLiteral("Ident \"%1\" utilisé par %2 points — la résolution par ident ne peut "
                               "en retenir qu'un seul, silencieusement").arg(it.key()).arg(it.value().size()),
                EntityRef{ EntityKind::Point, id.value() } });
        }
    }
}

} // namespace navstud::validator
