#pragma once

/* -----------------------------------------------------------------------------------------------------
UserEntities.h

MODÈLE DE SAISIE :
C'est ce que l'utilisateur tape, avant toute résolution d'ident ou conversion d'unité.
Ce modèle est distinct du modèle cible (model/Entities.h).
Ici les références inter-entités sont du texte (QString), les altitudes sont en pieds, les listes de
choix conservent leur libellé humain plutôt que leur valeur numérique cible.

Ce fichier est stocké tel quel en base (dernier état de saisie, retrouvé d'une session à l'autre)
La conversion vers model::ProjectRepository se fait à la demande, ligne par ligne, par le moteur de
génération.
Une conversion peut échouer (ident non encore résolvable) sans que la saisie elle-même soit invalide.
Ce header ne porte aucune règle de validité, seulement la forme des données saisies.

RÉSOLUTION DES INDEX :
Il y a quatre familles, redondantes à travers toutes les structures ci-dessous ; chaque champ texte
référençant une autre entité est annoté avec l'un de ces quatre schémas :

    [direct]        texte == ident propre de la table cible
                    (LegSequence.ident, Point.ident)

    [1 saut]        texte -> Point ; c'est CE PointId qui est stocké tel quel
                    (Waypoint.pointId, Airport.pointId, Runway.pointId,
                    Leg.pointId)

    [2 sauts pt]    texte -> Point -> entité dont c'est le pointId
                    (Runway.airportId, Navaid.runwayId, Approach.runwayId,
                    Navaid.distanceNavaidId "GS/DME associé", Leg.navaidId)

    [2 sauts seq]   texte -> LegSequence -> entité dont c'est le legSequenceId
                    (ProcedureTransition.procedureId,
                    RunwayProcedureTransition.procedureId/engineOut,
                    ApproachTransition.approachId)

Le registre de résolution n'a besoin que de CES QUATRE opérations composées, jamais d'une résolution
ad hoc par structure.
----------------------------------------------------------------------------------------------------- */

#include <QString>

namespace navstud::userdata {

// ============================================================================
// UserPoint — saisie directe, aucune résolution d'ident (première tranche
// verticale à construire : c'est la plus simple, elle valide le tuyau de
// bout en bout sans la complexité de la résolution).
struct UserPoint {
    QString ident;           // [direct, identité de CETTE table] 6 car. max
    double  latitude         = 0.0;
    double  longitude        = 0.0;
    double  magVar           = 0.0;
    double  holdCourse       = -1.0;
    double  holdDistInMeters = -1.0;
    double  holdTime         = -1.0;
    qint8   holdSide         = 0;
};


// =====================================================================================================
// UserWaypoint
struct UserWaypoint {
    QString pointIdent;     // [1 saut] -> Point
};


// =====================================================================================================
// UserNavaid
// type : liste de choix (9 libellés, cf. Flags_Navaid.xlsx) -> somme de
// flags NavaidType. category : liste de choix à 9 codes (0/1/2/3/I/L/A/S/F)
// dont les valeurs cible SONT déjà les codes attendus par NavaidCategory —
// pas de table de conversion à part, réutilise directement
// reader::parse::navaidCategory.
struct UserNavaid {
    QString type;                   // liste de choix -> NavaidTypeFlags (table Flags_Navaid)
    QString pointIdent;             // [1 saut] -> Point (emplacement de CE navaid)
    QString associatedNavaidIdent;  // [2 sauts pt] -> Point -> Navaid dont c'est le pointId ("GS/DME associé"), ou "-1"
    double  elevationInMeters = 0.0;
    double  declination       = 0.0;
    // figureOfMerit : PAS un champ de saisie, toujours 0 (cf. tableau, colonne fixe)
    quint32 frequencyMHzTimes100 = 0; // saisie directe déjà au format cible (ex. 10895)
    QString category;                 // liste de choix "0/1/2/3/I/L/A/S/F" -> NavaidCategory (valeurs déjà cible)
    double  course = 0.0;
    double  angle  = 0.0;
    QString runwayIdent; // [2 sauts pt] -> Point (seuil) -> Runway dont c'est le pointId
};


// =====================================================================================================
// UserAirport
// pointIdent ici est un cas particulier de [1 saut] : contrairement à
// Runway/Navaid, Airport N'EST PAS recherché dans sa propre table à partir
// du Point résolu — c'est CE Point qui devient directement Airport.pointId,
// exactement comme UserWaypoint. La distinction "1 saut" vs "2 sauts pt" ne
// dépend donc pas du nom du champ ("Ident Airport" ailleurs veut dire
// l'inverse, cf. UserRunway/UserProcedure) mais de si on CRÉE l'Airport
// (1 saut) ou qu'on le RETROUVE (2 sauts).
struct UserAirport {
    QString pointIdent; // [1 saut] -> Point ; devient Airport.pointId directement
    double  elevationInMeters          = 0.0;
    double  limitSpeedInMetersPerSec   = -1.0;
    double  limitAltitudeInMeters      = -1.0;
    double  transitionAltitudeInMeters = 0.0;
    double  transitionLevelInMeters    = 0.0;
};


// =====================================================================================================
// UserRunway
struct UserRunway {
    QString airportIdent;   // [2 sauts pt] -> Point -> Airport dont c'est le pointId
    QString thresholdIdent; // [1 saut] -> Point ; devient Runway.pointId directement
    double  elevationInMeters  = 0.0;
    double  gradient           = 0.0;
    double  course             = 0.0;
    double  lengthInMeters     = 0.0;
    double  displacedInMeters  = 0.0;
    double  stopwayInMeters    = 0.0;
    double  crossInMeters      = 0.0;
};


// =====================================================================================================
// UserLegSequence
// ilsOrRnav x procedureKind : double liste -> sequenceTypeRaw via la table
// dérivée des données LFFA (0=APP TRANS, 2=SID/STAR-ILS, 5=SID/STAR-RNAV ou
// APP-ILS, 12=APP-RNAV). altitudeLevelTransInFeet : PIEDS, converti /3.28084
// vers LegSequence.transitionInMeters — confirmé sur données réelles, ne
// PAS traiter comme une simple recopie de texte.
struct UserLegSequence {
    QString ident;          // [direct, identité de CETTE table] 6 car. max, sans guillemets (ajoutés au Writer)
    QString ilsOrRnav;      // liste de choix : "ILS" / "RNAV"
    QString procedureKind;  // liste de choix : "APP" / "APP TRANS" / "SID" / "STAR"
    double  altitudeLevelTransInFeet = 0.0; // PIEDS — converti /3.28084 à la génération
};


// =====================================================================================================
// UserLeg
// codePath : liste de choix (CF/DF/HM/IF/RF/TF) — déjà le texte cible
// attendu par Leg.code, aucune conversion supplémentaire nécessaire au-delà
// de l'encodage 2-car.->uint16 déjà géré par writer::format::packLegCode.
// wpDescription : liste de choix (25 codes 4-car., cf. WP_descript_code.xlsx)
// -> PointUsageFlags.
struct UserLeg {
    QString codePath;         // liste de choix -> Leg.code (déjà le texte cible)
    QString legSequenceIdent; // [direct] -> LegSequence
    QString pointIdent;       // [1 saut] -> Point ("FIX")
    QString wpDescription;    // liste de choix -> PointUsageFlags (table WP_descript_code)
    double  course            = -1.0;
    double  distanceInMeters  = 0.0;
    QString navaidIdent;      // [2 sauts pt] -> Point -> Navaid dont c'est le pointId, ou vide/"-1"
    double  navaidCourse             = -1.0;
    double  navaidDistanceInMeters   = -1.0;
    double  altitudeLimitMinInFeet   = -1.0; // PIEDS — converti /3.28084 à la génération, SAUF -1 (non spécifié)
    double  altitudeLimitMaxInFeet   = -1.0; // PIEDS — converti /3.28084 à la génération, SAUF -1 (non spécifié)
    double  airSpeedLimit            = -1.0;
    double  path                     = 0.0;
    qint8   turnDir                  = 0;    // -1 = L, 0 = E, +1 = R (les 3 valeurs, cf. confirmation)
    double  rnpInMeters              = 0.0;
};


// =====================================================================================================
// UserProcedure — commune à SID et STAR ; le kind (Sid/Star) est porté par
// la table qui la contient côté UserProject, pas par un champ ici, comme
// pour model::Procedure.
struct UserProcedure {
    QString airportIdent;     // [2 sauts pt] -> Point -> Airport dont c'est le pointId
    QString legSequenceIdent; // [direct] -> LegSequence (séquence "cœur")
};


// =====================================================================================================
// UserApproach
// decisionHeightInFeet et minimumDescentInFeet : PIEDS, tous deux convertis
// /3.28084 — les deux sont de VRAIES saisies (cf. confirmation : la DH n'est
// pas figée en dur malgré sa quasi-constance habituelle par aéroport).
struct UserApproach {
    QString runwayIdent;        // [2 sauts pt] -> Point (seuil) -> Runway dont c'est le pointId
    QString legSequenceIdent;   // [direct] -> LegSequence
    double  decisionHeightInFeet = 0.0;     // PIEDS — converti /3.28084
    double  minimumDescentInFeet = 0.0;     // PIEDS — converti /3.28084
};


// =====================================================================================================
// UserApproachTransition
struct UserApproachTransition {
    QString approachIdent;    // [2 sauts seq] -> LegSequence -> Approach dont c'est le legSequenceId
    QString legSequenceIdent; // [direct] -> LegSequence
};


// =====================================================================================================
// UserProcedureTransition — commune à SID et STAR (même remarque que
// UserProcedure : le kind vit dans la table conteneur).
struct UserProcedureTransition {
    QString procedureIdent;   // [2 sauts seq] -> LegSequence -> Procedure (du bon kind) dont c'est le legSequenceId
    QString legSequenceIdent; // [direct] -> LegSequence (séquence de la transition)
};


// =====================================================================================================
// UserRunwayProcedureTransition — commune à SID et STAR.
struct UserRunwayProcedureTransition {
    QString runwayIdent;      // [2 sauts pt] -> Point (seuil) -> Runway dont c'est le pointId
    QString procedureIdent;   // [2 sauts seq] -> LegSequence -> Procedure (du bon kind) dont c'est le legSequenceId
    // engineOutProcedureId : PAS un champ de saisie pour l'instant, toujours -1 (EOS)
    QString legSequenceIdent; // [direct] -> LegSequence (ancrage dédié à CETTE transition)
};

} // namespace navstud::userdata
