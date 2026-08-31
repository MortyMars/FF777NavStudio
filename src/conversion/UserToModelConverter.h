#pragma once

// ============================================================================
// UserToModelConverter.h
// Convertit une entité de saisie (userdata::UserXxx) vers son équivalent du
// modèle cible (model::Xxx). Une fonction par structure, indépendante des
// autres — Point n'a besoin d'aucune résolution d'ident ni d'aucune
// conversion d'unité, c'est pourquoi elle est la première écrite : elle
// valide le tuyau (UI -> conversion -> ProjectRepository) avant d'introduire
// la complexité de la résolution.
//
// Les futures fonctions (Navaid, Runway, ...) auront besoin de résoudre des
// idents contre un ProjectRepository déjà partiellement peuplé — leur
// signature prendra donc un paramètre de résolution supplémentaire. Point
// n'en a pas besoin, sa signature reste volontairement minimale plutôt que
// de prendre un paramètre inutilisé.
// ============================================================================

#include "ConversionResult.h"
#include "Entities.h"
#include "IdentResolver.h"
#include "UserEntities.h"

namespace navstud::conversion {

ConversionResult<model::Point> convertPoint(const userdata::UserPoint& input);

// Navaid : première structure à vraiment exercer IdentResolver — pointIdent
// en 1 saut, associatedNavaidIdent et runwayIdent en 2 sauts via Point.
// Suppose que les Point et Runway déjà référencés existent dans le
// repository que resolver indexe (cf. ordre topologique dans Regenerator).
ConversionResult<model::Navaid> convertNavaid(const userdata::UserNavaid& input, const IdentResolver& resolver);

// Airport : pointIdent en 1 saut — le Point résolu devient directement
// Airport.pointId (pas de recherche d'un Airport existant, cf. remarque
// dans UserEntities.h sur la distinction "on crée" / "on retrouve").
ConversionResult<model::Airport> convertAirport(const userdata::UserAirport& input, const IdentResolver& resolver);

// Runway : airportIdent en 2 sauts pt (-> Airport), thresholdIdent en 1 saut
// (-> devient Runway.pointId directement).
ConversionResult<model::Runway> convertRunway(const userdata::UserRunway& input, const IdentResolver& resolver);

// LegSequence : ident direct (aucune résolution), sequenceTypeRaw via la
// double liste ILS/RNAV x APP/APP TRANS/SID/STAR, transitionInMeters
// converti depuis des PIEDS (/3.28084).
ConversionResult<model::LegSequence> convertLegSequence(const userdata::UserLegSequence& input);

// Leg : la structure la plus riche — legSequenceIdent direct, pointIdent en
// 1 saut (optionnel), navaidIdent en 2 sauts pt (optionnel), codePath en
// liste blanche (déjà le texte cible), wpDescription via table de
// conversion (table WP_descript_code). altitudeLimitMinInFeet/MaxInFeet :
// PIEDS (cohérence instrumentation B777), convertis /3.28084 vers les
// champs cible en mètres, SAUF la sentinelle -1.0 (non spécifié) laissée
// telle quelle.
ConversionResult<model::Leg> convertLeg(const userdata::UserLeg& input, const IdentResolver& resolver);

// Procedure (Sid ou Star, selon kind) : airportIdent en 2 sauts pt,
// legSequenceIdent direct.
ConversionResult<model::Procedure> convertProcedure(const userdata::UserProcedure& input, const IdentResolver& resolver);

// Approach : runwayIdent en 2 sauts pt, legSequenceIdent direct,
// decisionHeightInFeet/minimumDescentInFeet convertis depuis des PIEDS.
ConversionResult<model::Approach> convertApproach(const userdata::UserApproach& input, const IdentResolver& resolver);

// ProcedureTransition (Sid ou Star, selon kind) : procedureIdent en 2 sauts
// seq (-> Procedure DU MÊME kind), legSequenceIdent direct.
ConversionResult<model::ProcedureTransition> convertProcedureTransition(
    const userdata::UserProcedureTransition& input, model::ProcedureKind kind, const IdentResolver& resolver);

// ApproachTransition : approachIdent en 2 sauts seq, legSequenceIdent direct.
ConversionResult<model::ApproachTransition> convertApproachTransition(
    const userdata::UserApproachTransition& input, const IdentResolver& resolver);

// RunwayProcedureTransition (Sid ou Star, selon kind) : runwayIdent en
// 2 sauts pt, procedureIdent en 2 sauts seq (-> Procedure DU MÊME kind),
// engineOutProcedureId toujours invalid() (pas de champ de saisie pour
// l'instant, cf. UserEntities.h), legSequenceIdent direct.
ConversionResult<model::RunwayProcedureTransition> convertRunwayProcedureTransition(
    const userdata::UserRunwayProcedureTransition& input, model::ProcedureKind kind, const IdentResolver& resolver);

} // namespace navstud::conversion
