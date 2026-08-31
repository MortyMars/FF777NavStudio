#pragma once

// ============================================================================
// Validator.h
// Valide un ProjectRepository en lecture seule, produit une liste de
// Diagnostic. Ne modifie jamais le repository. Chaque règle est une méthode
// privée indépendante — en ajouter ou en retirer une ne doit jamais affecter
// les autres.
// ============================================================================

#include "ProjectRepository.h"

#include <QString>
#include <QVector>

namespace navstud::validator {

enum class Severity {
    Error,   // bloquant : plantage X-Plane, ou donnée invalide/introuvable
    Warning, // non bloquant : heuristique, ou incomplétude tolérable en cours d'édition
};

// Identifie la table/l'entité concernée par un diagnostic, pour permettre à
// l'UI (QTableView) de surligner la ligne fautive sans que le Validator ait
// à connaître quoi que ce soit de l'UI elle-même.
//
// SidProcedure/StarProcedure (et les paires ProcedureTransition/
// RunwayProcedureTransition équivalentes) sont volontairement distincts :
// depuis que Sid et Star ont des compteurs d'ID indépendants (cf.
// ProcedureKind dans Entities.h), "Procedure#5" seul ne suffit plus à
// identifier une entité de façon unique — il faut préciser de laquelle des
// deux tables il s'agit.
enum class EntityKind {
    Point,
    Waypoint,
    Navaid,
    Airport,
    Runway,
    LegSequence,
    Leg,
    Approach,
    ApproachTransition,
    SidProcedure,
    StarProcedure,
    SidProcedureTransition,
    StarProcedureTransition,
    SidRunwayProcedureTransition,
    StarRunwayProcedureTransition,
};

struct EntityRef
{
    EntityKind kind;
    qint32     id;
};

struct Diagnostic
{
    Severity  severity;
    QString   ruleId;  // identifiant court et stable, ex. "rpt-legseq-invalid" — pour filtrage/désactivation future
    QString   message;
    EntityRef ref;
};

class Validator
{
public:
    QVector<Diagnostic> validate(const model::ProjectRepository& repo) const;

private:
    // Intégrité référentielle générique : toute clé étrangère non optionnelle
    // doit être valide et exister dans la table cible ; toute clé étrangère
    // optionnelle (invalid() = "non spécifié"), si elle est renseignée, doit
    // exister.
    void checkReferentialIntegrity(const model::ProjectRepository& repo, QVector<Diagnostic>& out) const;

    // Règles métier issues du debug LFFA.
    void checkRunwayProcedureTransitionLegSequence(const model::ProjectRepository& repo, QVector<Diagnostic>& out) const;
    void checkProcedureHasRunwayProcedureTransition(const model::ProjectRepository& repo, QVector<Diagnostic>& out) const;
    void checkSidInitialPointMatchesRunway(const model::ProjectRepository& repo, QVector<Diagnostic>& out) const;

    // Un Point.ident dupliqué est une ambiguïté silencieuse pour
    // IdentResolver (son QHash ident->PointId ne retient que le dernier
    // inséré) — Warning et non Error : un doublon transitoire peut
    // légitimement exister le temps de renommer l'un des deux points,
    // cohérent avec la tolérance déjà admise ailleurs (ex.
    // procedure-missing-rpt) pendant l'édition.
    void checkUniquePointIdent(const model::ProjectRepository& repo, QVector<Diagnostic>& out) const;
};

} // namespace navstud::validator
