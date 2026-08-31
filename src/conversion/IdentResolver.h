#pragma once

// ============================================================================
// IdentResolver.h
// Résout un ident texte saisi par l'utilisateur vers l'Id<Tag> du modèle
// cible, contre un model::ProjectRepository déjà (au moins partiellement)
// peuplé — cf. UserToModelConverter.h : la résolution d'une structure donnée
// suppose que ses dépendances sont déjà présentes dans le repository, d'où
// l'ordre topologique fixe piloté par Regenerator.
//
// Construit une fois par étape de régénération (coût négligeable à l'échelle
// d'un projet — au plus quelques centaines d'entités, pas le fichier
// mondial) : ne PAS conserver un IdentResolver au-delà de la portée où le
// repository qu'il indexe reste stable, puisque ses index sont figés à la
// construction et ne suivent pas les ajouts ultérieurs.
//
// DEUX PRIMITIVES, qui composent les quatre schémas de résolution identifiés
// dans UserEntities.h :
//   - résolution DIRECTE : ident -> Id<Tag> d'une table qui porte son propre
//     champ `ident` (Point, LegSequence). Couvre à la fois le schéma
//     [direct] (LegSequence référencée pour elle-même) et [1 saut] (Point
//     référencé pour lui-même : le PointId résolu EST la valeur stockée).
//   - résolution PROPRIÉTAIRE : étant donné un Id<Tag> intermédiaire déjà
//     résolu (un PointId ou un LegSequenceId), retrouve l'entité d'une autre
//     table dont un champ précis vaut cet Id intermédiaire. Couvre
//     [2 sauts pt] et [2 sauts seq], en composition avec la résolution
//     directe ci-dessus.
// ============================================================================

#include "ProjectRepository.h"

#include <QHash>
#include <QString>

#include <optional>

namespace navstud::conversion {

class IdentResolver
{
public:
    explicit IdentResolver(const model::ProjectRepository& repo);

    // --- Résolution directe ---
    std::optional<model::PointId>       point(const QString& ident) const;
    std::optional<model::LegSequenceId> legSequence(const QString& ident) const;

    // --- Résolution propriétaire, composée avec point() ---
    // "GS/DME associé" ET Leg.navaidIdent utilisent la même opération :
    // ident -> Point -> Navaid dont c'est le pointId.
    std::optional<model::AirportId> airportViaPointIdent(const QString& ident) const;
    std::optional<model::RunwayId>  runwayViaPointIdent(const QString& ident) const;
    std::optional<model::NavaidId>  navaidViaPointIdent(const QString& ident) const;

    // --- Résolution propriétaire, composée avec legSequence() ---
    std::optional<model::ProcedureId> procedureViaLegSequenceIdent(model::ProcedureKind kind, const QString& ident) const;
    std::optional<model::ApproachId>  approachViaLegSequenceIdent(const QString& ident) const;

private:
    const model::ProjectRepository& mRepo;
    QHash<QString, model::PointId>       mPointsByIdent;
    QHash<QString, model::LegSequenceId> mLegSequencesByIdent;
};

} // namespace navstud::conversion
