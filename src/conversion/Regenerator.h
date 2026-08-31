#pragma once

// ============================================================================
// Regenerator.h
// Reconstruit un model::ProjectRepository à partir d'un userdata::UserProject
// — c'est la passe qui permet la saisie "permissive" demandée : une ligne
// dont un ident ne se résout pas encore n'empêche ni la saisie, ni le reste
// du projet d'être régénéré ; elle est simplement absente du repository
// reconstruit, avec son échec consigné pour affichage (niveau Alerte).
//
// ORDRE TOPOLOGIQUE FIXE, sans cycle, un seul passage suffit :
//   Point -> Waypoint/Airport -> Runway -> Navaid -> LegSequence -> Leg
//         -> Procedure(Sid/Star) -> Approach
//         -> ProcedureTransition(Sid/Star) -> ApproachTransition
//         -> RunwayProcedureTransition(Sid/Star)
//
// Aujourd'hui seul Point est câblé (aucune résolution d'ident nécessaire) —
// les structures suivantes s'ajoutent au même endroit, dans runAll(), au
// fur et à mesure que leur convertisseur (et le futur IdentResolver dont
// elles ont besoin) sont écrits. Ne pas anticiper leur câblage avant que
// leur convertisseur existe réellement.
//
// DEUX NIVEAUX (cf. exigence fonctionnelle "Alerte" / "Validation") :
//   - Alerte     = union des échecs de conversion + tous les diagnostics du
//                  Validator (Error et Warning), affichés pour correction.
//   - Validation = AUCUN échec de conversion ET aucun validator::Severity::
//                  Error. Les Warning seuls n'empêchent pas la Validation —
//                  cohérent avec procedure-missing-rpt, déjà en Warning
//                  précisément pour tolérer une saisie en cours.
// ============================================================================

#include "ProjectRepository.h"
#include "UserProject.h"
#include "Validator.h"

#include <QString>
#include <QStringList>
#include <QVector>

namespace navstud::conversion {

// Échec de conversion d'une ligne UserXxx -> Xxx. Réutilise
// validator::EntityKind plutôt que d'en dupliquer un : ce sont les mêmes
// catégories d'entités, la même distinction Sid/Star pour Procedure et
// consorts.
struct ConversionFailure
{
    validator::EntityKind kind;
    qint32                userId; // l'id côté UserProject == l'id qu'aurait porté l'entité cible (cf. UserProject.h)
    QStringList           errors;
};

struct RegenerationResult
{
    model::ProjectRepository        repository;
    QVector<ConversionFailure>      conversionFailures;
    QVector<validator::Diagnostic>  validationDiagnostics;

    // Aucun échec de conversion, aucune Severity::Error du Validator. Les
    // Warning (validator ou futurs) n'empêchent pas la validation.
    bool isValid() const
    {
        if (!conversionFailures.isEmpty())
            return false;
        for (const validator::Diagnostic& d : validationDiagnostics) {
            if (d.severity == validator::Severity::Error)
                return false;
        }
        return true;
    }

    int alertCount() const { return conversionFailures.size() + validationDiagnostics.size(); }
};

class Regenerator
{
public:
    RegenerationResult regenerate(const userdata::UserProject& project) const;
};

} // namespace navstud::conversion
