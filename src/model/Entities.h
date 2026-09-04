#pragma once

/* -----------------------------------------------------------------------------------------------------
Entities.h

Modèle de données interne (modèle cible) du générateur de procédures SID/STAR/Approach
pour le FMC FF B777v2 (X-Plane 12).

Traduction typée de la spec de référence NavDataFile.h (namespace ndbl::navdata::File).
Les structures ci-dessous reprennent les mêmes champs, dans le même ordre, car cet ordre est important :
il correspond exactement à l'ordre des colonnes dans les fichiers texte finaux, où chaque ligne a la forme :

    NOM_STRUCTURE index champ1 champ2 champ3 ...

    Exemple :   POINT 12654 mIdent mLat mLong mMagVar mHoldCourse mHoldDistInMeters mHoldTime mHoldSide

Ce fichier ne reproduit PAS le format binaire packé de la spec d'origine
(#pragma pack(4), char[N+1], uint16/sint32 bruts) : c'est un modèle éditable en mémoire, pas un blob binaire.

Les identifiants inter-entités sont donc remplacés par des types forts (Id<Tag>) plutôt que des sint32 nus,
précisément pour rendre impossible à la compilation la confusion découverte en debug entre
Procedure::mLegSequenceId et RunwayProcedureTransition::mLegSequenceId (cause du doublon de séquence de legs
sur le ND de l'appareil).

Pour la même raison, tous les champs flottants sont en 'double', PAS en 'float' comme la spec d'origine
qui les voulait 'float' pour des contraintes de mémoire binaire packée qui ne nous concernent pas.
Un float (~7 chiffres significatifs) ne suffit pas à représenter fidèlement une valeur convertie pieds->mètres
à 6 décimales (ex. 1219.199961, 10 chiffres significatifs) —> bug constaté en pratique :
un LegSequence::transitionInMeters stocké en 'float' était réécrit 1219.199951 au lieu de 1219.199961.

Corollaire pour tout code appelant : NE JAMAIS suffixer un littéral flottant par 'f' lors de la
construction d'une entité (2.3f perd déjà la précision AVANT même la conversion vers double), mais toujours
écrire les littéraux sans suffixe (2.3, 1219.199961).
----------------------------------------------------------------------------------------------------- */

#include <QString>
#include <QFlags>
#include <QtGlobal>
#include <functional>

namespace navstud::model {

// =====================================================================================================
// Identifiants forts
// Id<Tag> encapsule un qint32, comme les mXxxId de la spec d'origine, mais chaque famille d'entité a un
// type distinct : impossible d'assigner un LegSequenceId là où un ProcedureId est attendu, et impossible
// de recopier tel quel un Procedure::legSequenceId dans un 'RunwayProcedureTransition::legSequenceId'
// sans cast explicite, ce qui doit attirer l'œil en revue de code.
//
// IMPORTANT : Id::invalid() (-1) reste une valeur assignable. Le type ne garantit PAS à lui seul "jamais -1"
// pour RunwayProcedureTransition::legSequenceId (cf. mDataStructures.txt ligne ~330), cette règle reste de
// la responsabilité du validateur (règle bloquante dédiée), au même titre que
// "legSequenceId != parentProcedure.legSequenceId".
template <typename Tag>

class Id
{
    public:
        static constexpr qint32 kInvalidValue = -1;

        constexpr Id() noexcept : mValue(kInvalidValue) {}
        explicit constexpr Id(qint32 value) noexcept : mValue(value) {}

        static constexpr Id invalid() noexcept { return Id(); }

        constexpr qint32 value() const noexcept { return mValue; }
        constexpr bool isValid() const noexcept { return mValue >= 0; }

        constexpr bool operator==(const Id& other) const noexcept { return mValue == other.mValue; }
        constexpr bool operator!=(const Id& other) const noexcept { return mValue != other.mValue; }
        constexpr bool operator<(const Id& other) const noexcept { return mValue < other.mValue; }

    private:
        qint32 mValue;
};

struct PointTag {};
struct WaypointTag {};
struct NavaidTag {};
struct AirportTag {};
struct RunwayTag {};
struct LegSequenceTag {};
struct LegTag {};
struct ProcedureTag {};
struct ApproachTag {};
struct ProcedureTransitionTag {};
struct ApproachTransitionTag {};
struct RunwayProcedureTransitionTag {};

using PointId                      = Id<PointTag>;
using WaypointId                   = Id<WaypointTag>;
using NavaidId                     = Id<NavaidTag>;
using AirportId                    = Id<AirportTag>;
using RunwayId                     = Id<RunwayTag>;
using LegSequenceId                = Id<LegSequenceTag>;
using LegId                        = Id<LegTag>;
using ProcedureId                  = Id<ProcedureTag>;
using ApproachId                   = Id<ApproachTag>;
using ProcedureTransitionId        = Id<ProcedureTransitionTag>;
using ApproachTransitionId         = Id<ApproachTransitionTag>;
using RunwayProcedureTransitionId  = Id<RunwayProcedureTransitionTag>;

} // namespace navstud::model

// std::hash pour permettre l'usage de Id<Tag> comme clé de QHash/std::unordered_map
namespace std {
template <typename Tag>
struct hash<navstud::model::Id<Tag>>
{
    size_t operator()(const navstud::model::Id<Tag>& id) const noexcept
    {
        return std::hash<qint32>()(id.value());
    }
};
} // namespace std

namespace navstud::model {


// =====================================================================================================
// PointUsage — Waypoint Description Code 5.17 / Waypoint Type 5.42
// PointUsage est rendu actif : Q_DECLARE_FLAGS permet au validateur de vérifier des règles sémantiques,
// par ex. le point posé comme seuil de piste dans une 'RunwayProcedureTransition' doit porter le flag
// Runway ; un point IAF d'approche doit porter le flag Iaf, etc.
enum class PointUsage : quint32 {
    None            = 0,
    Intersect       = 1u << 0,   // Named Intersection
    Uncharted       = 1u << 1,   // _U__ = Uncharted Intersection
    OffRoute        = 1u << 2,   // Off-Route Intersection
    Unnamed         = 1u << 3,   // Unnamed
    Phantom         = 1u << 4,   // P___ = Phantom
    Essential       = 1u << 5,   // E___ = Essential Waypoint
    NonEssential    = 1u << 6,   // R___ = Non-Essential Waypoint
    TransEssential  = 1u << 7,   // T____ = Transition Essential Waypoint
    Rnav            = 1u << 8,   // RNAV Waypoint
    Airport         = 1u << 9,   // A___ = Airport as Waypoint
    Runway          = 1u << 10,  // G___ = Runway as Waypoint
    Ndb             = 1u << 11,  // N___ = NDB Navaid as Waypoint
    Vhf             = 1u << 12,  // V____ = VHF Navaid as Waypoint
    OuterMarker     = 1u << 13,  // Outer Marker as Waypoint
    MiddleMarker    = 1u << 14,  // Middle Marker as Waypoint
    Iaf             = 1u << 15,  // ___A = Initial Approach Fix
    Approach        = 1u << 16,  // ___B = Intermediate Approach Fix
    Faf             = 1u << 17,  // ___F = Final Approach Fix
    Facf            = 1u << 18,  // ___I = Final Approach Course Fix
    Fep             = 1u << 19,  // Final End Point Fix
    Map             = 1u << 20,  // ___M = Missed Approach Point Fix
    Oceanic         = 1u << 21,  // Oceanic Gateway Waypoint
    Airspace        = 1u << 22,  // FIR/UIR or Ctrl. Airsp. Inter.
    Atc             = 1u << 23,  // __C_ = ATC Compulsory Waypoint
    End             = 1u << 24,  // _E__ = End of Enroute or Terminal Proc
    Overfly         = 1u << 25,  // _Y__ = Flyover Waypoint
    AfterFaf        = 1u << 26,  // After FAF
    BeforeFaf       = 1u << 27,  // Before FAF
    PathPoint       = 1u << 28,  // Path Point Fix
    Stepdown        = 1u << 29,  // Stepdown Fix
    Holding         = 1u << 30,  // ___H = Holding Fix
};
Q_DECLARE_FLAGS(PointUsageFlags, PointUsage)
Q_DECLARE_OPERATORS_FOR_FLAGS(PointUsageFlags)


// =====================================================================================================
// NavaidType — NAVAID Class 5.35
enum class NavaidType : quint16 {
    None          = 0,
    Dme           = 1u << 0,
    Ndb           = 1u << 1,
    Vor           = 1u << 2,
    Tac           = 1u << 3,
    Gls           = 1u << 4,
    Loc           = 1u << 5,
    Gs            = 1u << 6,
    Im            = 1u << 7,
    Mm            = 1u << 8,
    Om            = 1u << 9,
    Bm            = 1u << 10,
    Biased        = 1u << 11,
    Collocated    = 1u << 12,
    NonCollocated = 1u << 13,
};
Q_DECLARE_FLAGS(NavaidTypeFlags, NavaidType)
Q_DECLARE_OPERATORS_FOR_FLAGS(NavaidTypeFlags)


// =====================================================================================================
// Ces combinaisons sont des combinaisons de flags, pas des valeurs isolées : elles ne peuvent donc pas
// être des enumerators — elles sont reconstituées ici comme constantes de type NavaidTypeFlags.
inline constexpr NavaidTypeFlags kNavaidTypeApproach =
    NavaidTypeFlags(NavaidType::Gls) | NavaidType::Loc | NavaidType::Gs |
    NavaidType::Im | NavaidType::Mm | NavaidType::Om | NavaidType::Bm;

inline constexpr NavaidTypeFlags kNavaidTypeTransmit =
    NavaidTypeFlags(NavaidType::Ndb) | NavaidType::Vor | NavaidType::Loc | NavaidType::Gs;


// =====================================================================================================
// NavaidCategory — LS Category 5.80, valeurs confirmées sur les données LFFA.
// Catégories numériques (0-3) et catégories à code lettre (IGS/LDA/SDF) cohabitent dans le même champ
// quint32 d'origine, stockées ici comme un enum unique dont les enumerators lettre prennent la valeur du
// code ASCII correspondant (ex. Igs='I'=73), ce qui ne collisionne jamais avec les valeurs numériques 0-3.
enum class NavaidCategory : quint32 {
    LocalizerOnly       = 0,    // ILS Localizer Only, No Glideslope
    CategoryI           = 1,    // ILS Localizer/MLS/GLS Category I
    CategoryII          = 2,    // ILS Localizer/MLS/GLS Category II
    CategoryIII         = 3,    // ILS Localizer/MLS/GLS Category III
    Igs                 = 'I',  // IGS Facility
    LdaWithGlideslope   = 'L',  // LDA Facility with Glideslope
    LdaNoGlideslope     = 'A',  // LDA Facility, no Glideslope
    SdfWithGlideslope   = 'S',  // SDF Facility with Glideslope
    SdfNoGlideslope     = 'F',  // SDF Facility, no Glideslope
};


// =====================================================================================================
// Familles de procédure et types de séquence
// 'LegSequence::sequenceTypeRaw' est un uint8 brut dont le sens dépend de LA STRUCTURE QUI RÉFÉRENCE
// la LegSequence (cf. commentaire détaillé sur LegSequence plus bas), jamais d'une propriété stockée sur
// la LegSequence elle-même, une même séquence pouvant être référencée dans des contextes différents
// (ex. ancrage de seuil de piste partagé SID + STAR). Ce enum sert donc de paramètre contextuel côté
// validateur (ex. checkSequenceType (ProcedureFamily::Sid, leg.sequenceTypeRaw)), pas de champ de struct.
enum class ProcedureFamily : quint8 {
    Sid,
    Star,
    Approach,
};


// =====================================================================================================
// ProcedureKind — discriminant Sid/Star.
// CORRECTION (confirmée par un extrait du fichier mondial de production) : contrairement à une première
// hypothèse construite sur les seules données LFFA, SID et STAR n'ont PAS un espace d'ID partagé — ce sont
// deux compteurs INDÉPENDANTS.
// Preuve dans l'extrait mondial : [DEPARTURES] contient "PROCEDURE 0 111 1" et [ARRIVALS] contient séparément
// "PROCEDURE 0 56 89185" — le même id 0 existe légitimement dans les deux sections simultanément. L'absence
// de chevauchement observée sur LFFA (35945-35948 côté SID, 27597-27603 côté STAR) n'était qu'une coïncidence
// liée à l'état des deux compteurs à ce moment-là, pas une preuve de partage.
//
// Conséquence structurelle : Procedure, ProcedureTransition et 'RunwayProcedureTransition' ne portent PLUS
// de champ kind. Chaque kind vit dans sa PROPRE EntityTable, avec son propre IdAllocator indépendant (cf.
// ProjectRepository) — c'est l'appartenance à la table qui porte kind, exactement le même principe que
// "un Id<Tag> n'a de sens que dans sa table" déjà vrai pour tous les autres identifiants de ce fichier.
// Cet enum 'ProcedureKind' reste utile comme PARAMÈTRE (sélectionner la bonne table, le bon fichier _Xxx.txt),
// jamais comme champ stocké sur une entité.
enum class ProcedureKind : quint8 {
    Sid,
    Star,
};

// Route Type for SID (PD) 5.7
enum class DepartureType : quint8 {
    EngineOut              = 0,
    RunwayTransition       = 1,
    CommonRoute            = 2,
    EnrouteTransition      = 3,
    RnavRunwayTransition   = 4,
    RnavCommonRoute        = 5,
    RnavEnrouteTransition  = 6,
};

// Route Type for STAR (PE) 5.7
enum class ArrivalType : quint8 {
    EnrouteTransition      = 1,
    CommonRoute            = 2,
    RunwayTransition       = 3,
    RnavEnrouteTransition  = 4,
    RnavCommonRoute        = 5,
    RnavRunwayTransition   = 6,
};

// Route Type for Airport Approach (PF) 5.7
enum class ApproachType : quint8 {
    Transition = 0,
    LocBc      = 1,
    RnavGps    = 2,
    Fms        = 3,
    Igs        = 4,
    Ils        = 5,
    LaasGls    = 6,
    Waas       = 7,
    Loc        = 8,
    Mls        = 9,
    Ndb        = 10,
    Gps        = 11,
    Rnav       = 12,
    Tacan      = 13,
    Sdf        = 14,
    Vor        = 15,
    Mlsa       = 16,
    Lda        = 17,
    Mlsc       = 18,
    Missed     = 19,
};


// =====================================================================================================
// STRUCTURE POINT — fichier _Point.txt
// Utilisé pour Waypoints, Navaids, Airports, Runways et points de Leg.
// Tous les points doivent être uniques (couple Ident-Lat-Lon).
struct Point {
    QString  ident;                     // mIdent — Fix Identifier 5.13 (7 car. max)
    double   latitude          = 0.0;   // mLat, degrés
    double   longitude         = 0.0;   // mLon, degrés
    double   magVar            = 0.0;   // mMagVar, degrés. ABS==360 -> non défini
    double   holdCourse        = -1.0;  // mHoldCourse, degrés magnétiques. <0 -> non spécifié
    double   holdDistInMeters  = -1.0;  // mHoldDistInMeters. <0 -> non spécifié
    double   holdTime          = -1.0;  // mHoldTime, secondes. <0 -> non spécifié
    qint8    holdSide          = 0;     // mHoldSide. -1 = L, +1 = R, 0 = non spécifié
};


// =====================================================================================================
// STRUCTURE WAYPOINT — fichier _Waypoint.txt
// Reflète Waypoint Record (EA) 4.1.4 pour les airways.
struct Waypoint {
    PointId pointId;                   // mPointId
};


// =====================================================================================================
// STRUCTURE NAVAID — fichier _Navaid.txt
// Reflète VHF NAVAID 4.1.2, NDB NAVAID 4.1.3, Localizer/Glide Slope (PI) 4.1.11
struct Navaid {
    NavaidTypeFlags type;                   // mType, cf. NavaidType
    PointId  pointId;                       // mPointId
    NavaidId distanceNavaidId;              // mDistanceNavaidId. invalid() -> non spécifié
    double    elevationInMeters   = 0.0;    // mElevationInMeters. 0 si non utilisé
    double    declination         = 0.0;    // mDeclination. >=+360 TRUE NORTH, <=-360 GRID, 0 si non utilisé
    quint32  figureOfMerit        = 0;      // mFigureOfMerit. 0 si non utilisé
    quint32  frequencyMHzTimes100 = 0;      // mFreq : fréquence en MHz x100 (ex. 10895 -> 108.95 MHz)
    NavaidCategory category = NavaidCategory::LocalizerOnly; // mCategory — cf. NavaidCategory (table ARINC 5.80)
    double    course              = 0.0;    // mCourse. 0 -> non spécifié
    double    angle               = 0.0;    // mAngle (LS Glide Slope). 0 -> non spécifié
    RunwayId runwayId;                      // mRunwayId
};


// =====================================================================================================
// STRUCTURE AIRPORT — fichier _Airport.txt
struct Airport {
    PointId pointId;                                // mPointId
    double   elevationInMeters            = 0.0;    // mElevationInMeters
    double   limitSpeedInMetersPerSec     = -1.0;   // mLimitSpeedInMetersPerSec. <0 -> non spécifié
    double   limitAltitudeInMeters        = -1.0;   // mLimitAltitudeInMeters. <0 -> non spécifié
    double   transitionAltitudeInMeters   = -1.0;   // mTransitionAltitudeInMeters. <0 -> non spécifié
    double   transitionLevelInMeters      = -1.0;   // mTransitionLevelInMeters. <0 -> non spécifié
};


// =====================================================================================================
// STRUCTURE RUNWAY — fichier _Runway.txt
struct Runway {
    AirportId airportId;                      // mAirportId
    PointId   pointId;                        // mPointId — coordonnées du seuil réel
    double     elevationInMeters   = 0.0;     // mElevationInMeters — 5.68
    double     gradient            = 0.0;     // mGradient, % — 5.212
    double     course              = 0.0;     // mCourse, magnétique — 5.58
    double     lengthInMeters      = 0.0;     // mLengthInMeters — 5.57
    double     displacedInMeters   = 0.0;     // mDisplacedInMeters — 5.69
    double     stopwayInMeters     = 0.0;     // mStopwayInMeters — 5.79
    double     crossInMeters       = 0.0;     // mCrossInMeters — 5.67
};


// =====================================================================================================
// STRUCTURE LEGSEQUENCE — fichier _LegSequence.txt
// Liaison intermédiaire des legs de SID/STAR/Approach.
// RAPPEL (piège identifié en debug) : une RunwayProcedureTransition ne doit JAMAIS réutiliser le
// 'LegSequenceId' de son Procedure parent. Elle doit référencer une LegSequence minimale dédiée
// (un seul Leg de type IF posé sur le point de seuil de piste).
//
// CORRECTION (confirmée sur données réelles LFFA) : sequenceTypeRaw N'A PAS de "famille" intrinsèque.
// Une même LegSequence-ancrage de seuil de piste est légitimement partagée entre un SID et un(des) STAR,
// ex. LFFA : LegSequence "RW07" référencée à la fois par une 'RunwayProcedureTransition' de
// '_RunProcTransSID.txt' et une de '_RunProcTransSTAR.txt'. Le sens de sequenceTypeRaw dépend uniquement
// de LA STRUCTURE QUI RÉFÉRENCE cette LegSequence à un instant donné, jamais de la LegSequence elle-même :
//   - référencée par Procedure (liste SID)           -> DepartureType_
//   - référencée par Procedure (liste STAR)          -> ArrivalType_
//   - référencée par Approach / ApproachTransition   -> ApproachType_
//   - référencée par RunwayProcedureTransition (ancrage dédié) -> valeur neutre (0 observé dans les données
//     LFFA), sans signification DepartureType_/ArrivalType_ propre — c'est un simple marqueur.
// Le validateur doit donc vérifier 'sequenceTypeRaw' au niveau de chaque référence (avec le contexte de qui
// référence), jamais au niveau de 'LegSequence' seule, d'où l'absence ici de tout champ "family" ou
// d'accesseur typé sur cette struct.
struct LegSequence {
    QString ident;                      // mIdent (7 car. max)
    quint8  sequenceTypeRaw = 0;        // mSequenceType — brut, interprétation contextuelle (cf. ci-dessus)
    double   transitionInMeters = -1.0; // mTransitionInMeters
};


// =====================================================================================================
// STRUCTURE LEG — fichier _Leg.txt
// Reflète 4.1.9 Airport SID/STAR/Approach (PD, PE, PF).
struct Leg {
    QString         code;                   // mCode — Path & Termination 5.21 ('IF', 'CF', ...)
    LegSequenceId   legSequenceId;          // mLegSequenceId — jamais invalid()
    PointId         pointId;                // mPointId. invalid() -> non spécifié
    PointUsageFlags pointUsage;             // mPointUsage — 5.17
    double          course                    = -1.0;  // mCourse (OB MAG CRS) — 5.26. <0 = non spécifié
    double          distanceInMeters          =  0.0;  // mDistanceInMeters. <0 = temps, =0 = inutilisé
    NavaidId        navaidId;                          // mNavaidId. invalid() -> non spécifié
    double          navaidCourse              = -1.0;  // mNavaidCourse (THETA) — 5.24. <0 = non spécifié
    double          navaidDistanceInMeters    = -1.0;  // mNavaidDistanceInMeters (RHO) — 5.24. <0 = non spécif
    double          altitudeLimitMinInMeters  = -1.0;  // mAltitudeLimitMinInMeters — 5.29. <0 = aucune
    double          altitudeLimitMaxInMeters  = -1.0;  // mAltitudeLimitMaxInMeters — 5.29. <0 = aucune
    double          airSpeedLimit             = -1.0;  // mAirSpeedLimit. <0 = aucune
    double          path                      = 0.0;   // mPath — angle vertical. 0 = non spécifié
    qint8           turnDir                   = 0;     // mTurnDir — 5.20. -1 = L, +1 = R, 0 = E
    double          rnpInMeters               = 0.0;   // mRnpInMeters — 5.211. <=0 = non spécifié
};


// =====================================================================================================
// STRUCTURE PROCEDURE — fichiers _Procedure_SID.txt / _Procedure_STAR.txt
// Liaison intermédiaire de SID/STAR. Une Procedure sans 'RunwayProcedureTransition' correspondante
// n'apparaît pas correctement au MCDU (cf. cas des 3 STAR sur 7 identifié en debug), règle de validateur.
struct Procedure {
    AirportId     airportId;       // mAirportId
    LegSequenceId legSequenceId;   // mLegSequenceId — séquence "cœur" du SID/STAR
};


// =====================================================================================================
// STRUCTURE APPROACH — fichier _Approach.txt
// Le premier missed approach doit être ajouté à la séquence de legs de l'approche (5.10), règle métier à
// porter par le validateur, pas par ce header : rien ici ne peut garantir l'ordre des Leg d'une LegSequence.
struct Approach {
    RunwayId       runwayId;                       // mRunwayId
    LegSequenceId  legSequenceId;                  // mLegSequenceId
    double         decisionHeightInMeters = -1.0;  // mDecisionHeightInMeters — 5.170. <0 -> non spécifié
    double         minimumDescentInMeters = -1.0;  // mMinimumDescentInMeters — 5.171. <0 -> non spécifié
};


// =====================================================================================================
// STRUCTURE PROCEDURETRANSITION
// Fichiers '_ProcedureTransition_SID.txt' / '_ProcedureTransition_STAR.txt' (enroute transition, 5.11)
struct ProcedureTransition {
    ProcedureId   procedureId;     // mProcedureId — dans la table du MÊME kind que cette ProcedureTransition
    LegSequenceId legSequenceId;   // mLegSequenceId
};


// =====================================================================================================
// STRUCTURE APPROACHTRANSITION — fichier _ApproachTransition.txt
// Liaison STAR -> Approach (5.11)
struct ApproachTransition {
    ApproachId    approachId;      // mApproachId
    LegSequenceId legSequenceId;   // mLegSequenceId
};


// =====================================================================================================
// STRUCTURE RUNWAYPROCEDURETRANSITION
// Fichier '_RunwayProcedureTransition_SID.txt' / '_RunwayProcedureTransition_STAR.txt'
// Liaison SID/STAR depuis/vers une RUNWAY (5.11).
// DEUX RÈGLES BLOQUANTES issues du debug, à porter par le validateur (pas par le type, cf. remarque sur
// Id::invalid() en tête de fichier) :
//   1. legSequenceId != invalid()                          -> fait planter X-Plane
//   2. legSequenceId != parentProcedure.legSequenceId      -> doublon de legs sur le ND
// Cas SID sans transition de piste mais EOSID disponible : pas d'enregistrement sentinelle nécessaire, le
// writer omet simplement la ligne correspondante (fichier de sortie correctement nommé mais vide pour ce cas précis).
struct RunwayProcedureTransition {
    RunwayId      runwayId;             // mRunwayId
    ProcedureId   procedureId;          // mProcedureId — dans la table du MÊME kind que cette RunwayProcedureTransition
    ProcedureId   engineOutProcedureId; // mEngineOutProcedureId — SID engine-out, table Sid par construction
    LegSequenceId legSequenceId;        // mLegSequenceId — jamais invalid(), jamais == procedureId associé
};

} // namespace navstud::model


