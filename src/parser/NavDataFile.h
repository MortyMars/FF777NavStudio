
// NAVDATAFILE.H
// DÉCLARE SOUS FORME DE STRUCTURES LES CHAMPS D'ENREGISTREMENTS CONSTITUANT LES PROCÉDURES


/* AVERTISSEMENT ET ANALYSE PERSONNELLE DU CODE
 * ============================================
 *
 * Les fichiers NavDataFile.h et NavDataFile.cpp ont été extraits de leur contexte initial
 * plus large, pour être opportunément réutilisés ici dans le cadre du présent projet.
 *
 * On notera que les 'ENUMS' déclarées ne sont pas utilisées en tant que telles dans le projet.
 * De fait, elles n'ont ici qu'un rôle documentaire sur la façon dont sont encodés les flags binaires.
 * Elles ont cependant le mérite d'indiquer quelles valeurs sont valides, comment les interpréter,
 * et comment les combiner.
 * Par ailleurs, contrairement à une solution classique d'insertion de commentaires, elles s'avèreraient
 * précieuses dans le cadre d'une extension future des fonctionnalités utilisant l'analyse des données.
 *
 * Les 'STRUCTURES' quant à elles regroupent différentes données liées, et constituent ainsi les items
 * élémentaires décrivant des données de navigation et procédures compatibles avec la norme ARINC-424.
 * Elles assurent ainsi le maintien d'un format qui pourra être sauvegardé/chargé depuis des fichiers binaires.
 * Elles sont construites sur la base de types numériques élémentaires : uint16 et uint32, dont la taille est
 * garantie (2 ou 4 octets), quelle que soit la plateforme, ce qui ne serait pas forcément le cas avec d'autres
 * types de données plus élaborés (enum par exemple).
 * En termes de contenu pur, on peut citer la structure 'index' qui est la structure maîtresse contenant TOUTES
 * les données de navigation dans des vecteurs séparés, un peu comme une base de données en mémoire.
 *
 * Le code charge/sauvegarde des fichiers binaires avec un format et un ordre précis.
 * En utilisant uint16 ou uint32,
*/

#ifndef NAVDATAFILE_H
#define NAVDATAFILE_H

#include <string>
#include <memory>
#include <vector>

#include "Types.h"


namespace ndbl {

    class Logger; // Indique au compilateur que Logger est une classe, dont la déclaration est faite ailleurs

    namespace navdata {

        namespace File {

            //---------------------------------------------------------------------------
            // Flags to reflect Waypoint Description Code 5.17 and Waypoint Type 5.42.
            // Remarque : l'opérateur << est ici l'opérateur de décalage de bits
            // Les flags sont additionnés pour les points à plusieurs usages                        LFFA
            enum ePointUsage {
                PointUsage_Intersect = (1 << 0),        /// Named Intersection             (1 << 0) --> 2^0 = 1
                PointUsage_Uncharted = (1 << 1),        /// _U__ = Uncharted Intersection  (1 << 1) --> 2^1 = 2
                PointUsage_OffRoute = (1 << 2),         /// Off-Route Intersection         (1 << 2) --> 2^2 = 4
                PointUsage_Unnamed = (1 << 3),          /// Unnamed                        (1 << 3) --> 2^3 = 8
                PointUsage_Phantom = (1 << 4),          /// P___ = Phantom                 (1 << 4) --> 2^4 = 16
                PointUsage_Essential = (1 << 5),        //  E___ = Essential Waypoint               (1 << 5) --> 2^5 = 32
                PointUsage_NonEssential = (1 << 6),     /// R___ = Non-Essential Waypoint           (1 << 6) --> 2^6 = 64
                PointUsage_TransEssential = (1 << 7),   /// T____ = Transition Essential Waypoint   (1 << 7) --> 2^7 = 128
                PointUsage_RNAV = (1 << 8),             /// RNAV Waypoint                  (1 << 8) --> 2^8 = 256
                PointUsage_Airport = (1 << 9),          /// A___ = Airport as Waypoint              (1 << 9) --> 2^9 = 512
                PointUsage_Runway = (1 << 10),          //  G___ = Runway as Waypoint               (1 << 10) --> 2^10 = 1 024
                PointUsage_NDB = (1 << 11),             /// N___ = NDB Navaid as Waypoint           (1 << 11) --> 2^11 = 2 048
                PointUsage_VHF = (1 << 12),             /// V____ = VHF Navaid as Waypoint          (1 << 12) --> 2^12 = 4 096
                PointUsage_OM = (1 << 13),              /// Outer Marker as Waypoint       (1 << 13) --> 2^13 = 8 192
                PointUsage_MM = (1 << 14),              /// Middle Marker as Waypoint      (1 << 14) --> 2^14 = 16 384
                PointUsage_IAF = (1 << 15),             //  ___A = Initial Approach Fix             (1 << 15) --> 2^15 = 32 768
                PointUsage_Approach = (1 << 16),        //  ___B = Intermediate Approach Fix        (1 << 16) --> 2^16 = 65 536
                PointUsage_FAF = (1 << 17),             //  ___F = Final Approach Fix               (1 << 17) --> 2^17 = 131 072
                PointUsage_FACF = (1 << 18),            //  ___I = Final Approach Course Fix        (1 << 18) --> 2^18 = 262 144
                PointUsage_FEP = (1 << 19),             /// Final End Point Fix            (1 << 19) --> 2^19 = 524 288
                PointUsage_MAP = (1 << 20),             //  ___M = Missed Approach Point Fix        (1 << 20) --> 2^20 = 1 048 576
                PointUsage_Oceanic = (1 << 21),         /// Oceanic Gateway Waypoint       (1 << 21) --> 2^21 = 2 097 152
                PointUsage_Airspace = (1 << 22),        /// FIR/UIR or Ctrl. Airsp. Inter. (1 << 22) --> 2^22 = 4 194 304
                PointUsage_ATC = (1 << 23),             //  __C_ = ATC Compulsory Waypoint          (1 << 23) --> 2^23 = 8 388 608
                PointUsage_End = (1 << 24),             //  _E__ = End of Enroute or Terminal Proc  (1 << 24) --> 2^24 = 16 777 216
                PointUsage_Overfly = (1 << 25),         //  _Y__ = Flyover Waypoint                 (1 << 25) --> 2^25 = 33 554 432
                PointUsage_AfterFAF = (1 << 26),        /// After FAF                      (1 << 26) --> 2^26 = 67 108 864
                PointUsage_BeforeFAF = (1 << 27),       /// Before FAF                     (1 << 27) --> 2^27 = 134 217 728
                PointUsage_PathPoint = (1 << 28),       /// Path Point Fix                 (1 << 28) --> 2^28 = 268 435 456
                PointUsage_Stepdown = (1 << 29),        /// Stepdown Fix                   (1 << 29) --> 2^29 = 536 870 912
                PointUsage_Holding = (1 << 30),         //  ___H = Holding Fix                      (1 << 30) --> 2^30 = 1 073 741 824
            };

            //---------------------------------------------------------------------------
            // Flags to reflect NAVAID Class 5.35
            // Les flags sont ajoutés pour les Navaids à type multiple : LOC/GS --> 32+64 = 96
            enum eNavaidType {                          //  LFFA
                // Valeurs unitaires pour chacun des types élémentaires
                NavaidType_DME = (1 << 0),              //  DME (code '12' du earth_nav.dat) --> 2^0= 1
                NavaidType_NDB = (1 << 1),              /// 2^1= 2
                NavaidType_VOR = (1 << 2),              /// 2^2= 4
                NavaidType_TAC = (1 << 3),              /// 2^3= 8
                NavaidType_GLS = (1 << 4),              /// 2^4= 16
                NavaidType_LOC = (1 << 5),              //  LOC (code  '4' du earth_nav.dat) --> 2^5= 32
                NavaidType_GS = (1 << 6),               //  GS  (code  '6' du earth_nav.dat) --> 2^6= 64
                NavaidType_IM = (1 << 7),               /// 2^7= 128
                NavaidType_MM = (1 << 8),               /// 2^8= 256
                NavaidType_OM = (1 << 9),               /// 2^9= 512
                NavaidType_BM = (1 << 10),              /// 2^10= 1024
                NavaidType_Biased = (1 << 11),          /// 2^11= 2048
                NavaidType_Collocated = (1 << 12),      /// 2^12= 4096
                NavaidType_NonCollocated = (1 << 13),   /// 2^13= 8192
                                                        //  GS/DME non coloc. au LOC --> 8192+1+64 = 8257
                // Navaids types résultants
                // Dans les 2 types ci-après l'opérateur | (shift+option+l) est le OU binaire
                // Chacun des bits activés apporte sa valeur unitaire dans la valeur globale
                NavaidType_Approach = NavaidType_GLS | NavaidType_LOC | NavaidType_GS | NavaidType_IM |
                                      NavaidType_MM | NavaidType_OM | NavaidType_BM,

                NavaidType_Transmit = NavaidType_NDB | NavaidType_VOR | NavaidType_LOC | NavaidType_GS
            };

            //---------------------------------------------------------------------------
            // Route Type for SID (PD) 5.7                                                       LFFA
            enum eDepartureType {
                DepartureType_EngineOut = 0,              /// 0 - Engine Out SID
                DepartureType_RunwayTransition = 1,       /// 1 - SID Runway Transition
                DepartureType_CommonRoute = 2,            //  2 - SID or SID Common Route        SID = 2
                DepartureType_EnrouteTransition = 3,      /// 3 - SID Enroute Transition
                DepartureType_RNAV_RunwayTransition = 4,  /// 4 - RNAV SID Runway Transition
                DepartureType_RNAV_CommonRoute = 5,       //  5 - RNAV SID or SID Common Route   RNAV SID = 5
                DepartureType_RNAV_EnrouteTransition = 6, /// 6 - RNAV SID Enroute Transition
            };

            //---------------------------------------------------------------------------
            // Route Type for STAR (PE) 5.7                                                      LFFA
            enum eArrivalType {
                ArrivalType_EnrouteTransition = 1,      /// 1 - STAR Enroute Transition
                ArrivalType_CommonRoute = 2,            //  2 - STAR or STAR Common Route        STAR = 2
                ArrivalType_RunwayTransition = 3,       /// 3 - STAR Runway Transition
                ArrivalType_RNAV_EnrouteTransition = 4, /// 4 - RNAV STAR Enroute Transition
                ArrivalType_RNAV_CommonRoute = 5,       //  5 - RNAV STAR or STAR Common Route   RNAV STAR = 5
                ArrivalType_RNAV_RunwayTransition = 6,  /// 6 - RNAV STAR Runway Transition
            };

            //---------------------------------------------------------------------------
            // Route Type for Airport Approach (PF) 5.7                                          LFFA
            enum eApproachType {
                ApproachType_Transition = 0, //  A - Approach Transition                         APP TRANSITION = 0
                ApproachType_LOC_BC = 1,     /// B - Localizer/Backcourse
                ApproachType_RNAV_GPS = 2,   //  E - RNAV, GPS Required                          APP RNAV = 2 ????
                ApproachType_FMS = 3,        /// F - Flight Management System
                ApproachType_IGS = 4,        /// G - Instrument Guidance
                ApproachType_ILS = 5,        //  I - Instrument Landing System                   APP ILS = 5
                ApproachType_LAAS_GLS = 6,   /// J - LAAS-GPS/GLS
                ApproachType_WAAS = 7,       /// K - WAAS-GPS
                ApproachType_LOC = 8,        /// L - Localizer Only
                ApproachType_MLS = 9,        /// M - Microwave Landing System
                ApproachType_NDB = 10,       /// N - Non-Directional Beacon
                ApproachType_GPS = 11,       /// P - Global Positioning System
                ApproachType_RNAV = 12,      //  R - Area Navigation                             APP RNAV = 12 ????
                ApproachType_TACAN = 13,     /// T - TACAN
                ApproachType_SDF = 14,       /// U - Simplified Directional Facility
                ApproachType_VOR = 15,       /// V - VOR Approach
                ApproachType_MLSA = 16,      /// W - Microwave Landing System Type A
                ApproachType_LDA = 17,       /// X - Localizer Directional Aid
                ApproachType_MLSC = 18,      /// Y - Microwave Landing System Type B, C
                ApproachType_Missed = 19,    //  Z - Missed Approach                             APP MISS = 19
            };


            /* Sauvegarde (push) du format actuel d'alignement des données en mémoire (8 octets par défaut sur système 64 bits)
             * et forcement du nouvel alignement sur 4 octets (pack 4), sachant que les données de longueur supérieure seront
             * alignées sur un multiple de 4 octets.
             * L'objectif est de se conformer à un format d'alignement des données attendu (souvent dans le cas de fichiers binaires)
             * Au passage on gagne de la place en mémoire en diminuant le 'padding' (emplacements perdus)
             * À titre d'exemple en alignement standard sur système 64 bits, un 'char' occupe 1 octet utile et 7 octets de padding
             * En mode 'pack 4' le gâchis est réduit à 3 octets (1 octet utile + 3 de padding pour une longueur totale de 4 octets) */
            #pragma pack(push, 4)



            //---------------------------------------------------------------------------
            // STRUCTURE POINT
            // Used for Waypoints, Navaids, Airports, Runways and Leg points. All points must be unique, duplicated
            // Ident-Lat-Lon NOT ALLOWED IN ANY CASE! Grid points must not be included except it's a part of airway
            // or procedure. Get hold parameters of first hold linked to waypoint.
            struct Point {
                char mIdent[7 + 1];             // Fix Identifier from 5.13 (7 car + le car de fin de chaîne \0 null terminator)
                double mLat;                    // Latitude in degrees from 5.36
                double mLon;                    // Longitude in degrees from 5.37
                float mMagVar;                  // Magnetic Variation in degrees from 5.39, ABS==360 - not defined
                float mHoldCourse;              // Inbound Holding Course (magnetic) in degrees from 5.62, <0 - not specified
                float mHoldDistInMeters;        // Leg Length in meters from 5.64, <0 - not specified
                float mHoldTime;                // Leg Time in seconds from 5.65, <0 - not specified
                sint8 mHoldSide;                // Turn Direction from 5.63, -1 - L, +1 - R, 0 - not specified
            };

            //---------------------------------------------------------------------------
            // Reflects Waypoint Record (EA) 4.1.4 for airways.
            struct Waypoint {
                sint32 mPointId; // Point index
            };

            //---------------------------------------------------------------------------
            // STRUCTURE NAVAID
            // Reflects VHF NAVAID 4.1.2, NDB NAVAID 4.1.3, Airport and Heliport Localizer and Glide Slope Records (PI) 4.1.11
            // Type and Usage are derived from NAVAID Class, LOC point for ILS used.
            // In case of non collocated/collocated DME/TAC (Tacan) for ILS, or non collocated DME/TAC for VOR/NDB,
            // new Navaid must be created with NavaidType_Collocated/NavaidType_NonCollocated
            // Type flag and it's index must be set in DistanceNavaidId for primary navaid.
            /*
             * Traduction de 'In case of' : En cas de DME/TAC non colocalisé/colocalisé pour une ILS (ou de DME/TAC non colocalisé
             * pour un VOR/NDB), un nouveau Navaid doit être créé avec l'indicateur NavaidType_Collocated/NavaidType_NonCollocated
             * (NDMCN : càd 4096 ou 8192) et son index doit être défini dans DistanceNavaidId pour le navaid primaire (NDMCN : LOC). */
            struct Navaid {
                uint16 mType;                   // Mask of NavaidType_ from 5.35
                sint32 mPointId;                // Point index
                sint32 mDistanceNavaidId;       // Non collocated/collocated DME/TAC navaid index, <0 - not specified
                float mElevationInMeters;       // Elevation in meters from 5.40 or 5.74, 0 if not used
                float mDeclination;             // Station Declination in degrees from 5.66, >=+360 - TRUE NORTH, <=-360 GRID, 0 if not used
                                                // Déclinaison magnétique à l'emplacement du LOC
                uint32 mFigureOfMerit;          // Figure of Merit from 5.149, 0 if not used
                uint32 mFreq;                   // Frequency in Hz/10 from 5.34
                uint32 mCategory;               // LS Category from 5.80, 0 - LOC only/not used, 4 - IGS, 5 - LDA
                float mCourse;                  // LS Course from 5.47, 0 - not specified
                float mAngle;                   // LS Glide Slope Angle from 5.52, 0 - not specified
                sint32 mRunwayId;               // Runway index
            };

            //---------------------------------------------------------------------------
            // STRUCTURE AIRPORT
            // Reflects Airport Records (PA) 4.1.7. Airports with longest runway less than 1000m or not hard surface runways
            // must be dropped out (with all linked procedures).
            struct Airport {
                sint32 mPointId;                   // Point index
                float mElevationInMeters;          // Airport Elevation in meters
                float mLimitSpeedInMetersPerSec;   // Speed Limit in meters per second, <0 - not specified
                float mLimitAltitudeInMeters;      // Speed Limit Altitude in meters, <0 - not specified
                float mTransitionAltitudeInMeters; // Transition Altitude in meters, <0 - not specified
                float mTransitionLevelInMeters;    // Transition Level in meters, <0 - not specified
            };

            //---------------------------------------------------------------------------
            // STRUCTURE RUNWAY
            // Reflects Runway Records (PG) 4.1.10. Runways with length less than 1000m or not hard surface runways must be
            // dropped out (with all linked procedures).
            struct Runway {
                sint32 mAirportId;              // Airport index
                sint32 mPointId;                // Point index, coords of actual threshold used
                float mElevationInMeters;       // Landing Threshold Elevation in meters from 5.68
                float mGradient;                // Runway Gradient in percent from 5.212
                float mCourse;                  // Runway Magnetic Bearing from 5.58
                float mLengthInMeters;          // Runway Length from 5.57
                float mDisplacedInMeters;       // Runway Displaced Threshold Distance from 5.69
                float mStopwayInMeters;         // Runway Stopway length from 5.79
                float mCrossInMeters;           // Threshold Crossing Height from 5.67 with respect to displaced length (actual threshold crossing height)
            };

            //---------------------------------------------------------------------------
            // STRUCTURE LEGSEQUENCE
            // Intermediate linkage of SID/STAR/Approach legs
            struct LegSequence {
                char mIdent[7 + 1];             // Identifier (7 car + le car de fin de chaîne \0 null terminator)
                uint8 mSequenceType;            // DepartureType_, ArrivalType_ or ApproachType_
                float mTransitionInMeters;      // Transition Altitude/Level in meters
            };

            //---------------------------------------------------------------------------
            // STRUCTURE LEG
            // Reflects 4.1.9 Airport SID/STAR/Approach (PD, PE and PF)
            // For RF legs, ARC Radius, computed from arc center and point, must be coded to RHO. Time value coded in
            // Distance as negative value. NavaidDistance must be computed if required by leg code.
            struct Leg {
                uint16 mCode;                    // Path and Termination from 5.21 coded as 2 CHAR's like 'CF'
                sint32 mLegSequenceId;           // Leg sequence index
                sint32 mPointId;                 // Point index (Fix Identifier) from 5.13, <0 - not specified
                uint32 mPointUsage;              // Mask of PointUsage_ (Waypoint Description Code) from 5.17
                float mCourse;                   // Magnetic Course (OB MAG CRS) from 5.26, <0 - not specified
                float mDistanceInMeters;         // Route Distance/Holding Distance or Time (RTE DIST FROM, HOLD DIST/TIME) in meters/seconds, <0 - time, =0 - unused
                sint32 mNavaidId;                // Navaid index, <0 - not specified
                float mNavaidCourse;             // THETA in degrees from 5.24, <0 - not specified
                float mNavaidDistanceInMeters;   // RHO in meters from 5.24, <0 - not specified
                float mAltitudeLimitMinInMeters; // Minimum altitude limit in meters from 5.29, <0 - none, equals to AltitudeLimitMax in case of AT
                float mAltitudeLimitMaxInMeters; // Maximum altitude limit in meters from 5.29, <0 - none, 0 in case of Stepdown Fix in Final Approach
                float mAirSpeedLimit;            // Speed Limit, <0 - none
                float mPath;                     // Vertical Angle, 0 - not specified
                sint8 mTurnDir;                  // Turn Direction from 5.20, -1 - L, +1 - R, 0 - E
                float mRnpInMeters;              // rnp in meters from 5.211, <=0 - not specified
            };

            //---------------------------------------------------------------------------
            // STRUCTURE PROCEDURE (SECTIONS [DEPARTURES] ET [ARRIVALS])
            // Intermediate linkage of SID/STAR, Runway transition without Procedure or Procedure without Runway transition not
            // allowed. In case of procedure for airport, or only Runway transition defined, empty Procedure or Runway transition
            // must be added.
            struct Procedure {
                sint32 mAirportId;              // Airport index
                sint32 mLegSequenceId;          // Leg sequence index
            };

            //---------------------------------------------------------------------------
            //STRUCTURE APPROACH
            // Intermediate linkage of Approach 5.10. First missed approach procedure must be added to approach leg sequence.
            struct Approach {
                sint32 mRunwayId;               // Runway index
                sint32 mLegSequenceId;          // Leg sequence index
                float mDecisionHeightInMeters;  // Decision Height in meters from 5.170 for aircraft type, <0 - not specified
                float mMinimumDescentInMeters;  // Minimum Descent Altitude in meters from 5.171, <0 - not specified
            };

            //---------------------------------------------------------------------------
            // STRUCTURE PROCEDURETRANSITION (SECTIONS [DEPARTURETRANSITIONS] ET [ARRIVAL TRANSITIONS])
            // Intermediate linkage of Enroute transition 5.11
            struct ProcedureTransition {
                sint32 mProcedureId;            // Procedure index
                sint32 mLegSequenceId;          // Leg sequence index
            };

            //---------------------------------------------------------------------------
            // STRUCTURE APPROACHTRANSITION
            // Intermediate linkage of STAR to APPROACH transition 5.11
            struct ApproachTransition {
                sint32 mApproachId;             // Approach index
                sint32 mLegSequenceId;          // Leg sequence index
            };

            //---------------------------------------------------------------------------
            // STRUCTURE RUNWAYPROCEDURETRANSITION (SECTIONS [RUNWAYDEPARTURETRANSTIONS] ET [RUNWAYARRIVALTRANSITIONS]
            // Intermediate linkage of SID/STAR from/to RUNWAY transition 5.11. In case of no SID runway transition
            // but EOSID avail, SID runway transition must be created with empty procedure and empty leg sequence.
            struct RunwayProcedureTransition {
                sint32 mRunwayId;               // Runway index
                sint32 mProcedureId;            // Procedure index
                sint32 mEngineOutProcedureId;   // Engine out SID index
                sint32 mLegSequenceId;          // Leg sequence index
            };

            //---------------------------------------------------------------------------
            // STRUCTURE AIRWAY
            // Intermediate linkage for airway
            struct Airway {
                char mIdent[6 + 1];             // Identifier (6 car + le car de fin de chaîne \0 null terminator)
            };

            //---------------------------------------------------------------------------
            // STRUCTURE AIRSEGMENT
            // Intermediate linkage for airway segments. Segment must be created for each allowed airway direction.
            struct AirwaySegment {
                sint32 mAirwayId;               // Airway index
            };

            //---------------------------------------------------------------------------
            // STRUCTURE AIRWAYSEGMENTLEG
            // Enroute Airways Records (ER)
            struct AirwaySegmentLeg {
                sint32 mAirwaySegmentId;        // Airway segment id
                sint32 mPointId;                // Point id
            };

            //---------------------------------------------------------------------------
            // STRUCTURE ROUTE
            struct Route {
                char mIdent[10 + 1];            // Identifier   (10 car + le car de fin de chaîne \0 null terminator)
                char mCompany[20 + 1];          // Company name (20 car + le car de fin de chaîne \0 null terminator)
                sint32 mDepartureAirportId;     // Departure airport index
                sint32 mDepartureRunwayId;      // Departure runway index
                sint32 mDepartureId;            // Departure procedure index
                sint32 mDepartureTransitionId;  // Departure transition index
                sint32 mArrivalId;              // Arrival procedure index
                sint32 mArrivalTransitionId;    // Arrival transition index
                sint32 mApproachId;             // Approach index
                sint32 mApproachTransitionId;   // Approach transition index
                sint32 mArrivalAirportId;       // Arrival airport index
                sint32 mAlternateRouteId;       // Alternate route index
                sint16 mCostIndex;              // Cost index
                sint16 mCruiseLevel;            // Cruise level
            };

            //---------------------------------------------------------------------------
            // STRUCTURE ROUTESEGMENT
            struct RouteSegment {
                sint32 mRouteId;                // route index
                sint32 mAirwayId;               // airway index
                sint32 mPointId;                // point index
            };

            //---------------------------------------------------------------------------
            // STRUCTURE CONFIG
            struct Config {
                char mSource[3 + 1];            // ARINC cycle source   (3 car + le car de fin de chaîne \0 null terminator)
                char mCycle[4 + 1];             // ARINC cycle          (4 car + le car de fin de chaîne \0 null terminator)
                char mSequence[2 + 1];          // ARINC cycle sequence (2 car + le car de fin de chaîne \0 null terminator)
                char mStart[7 + 1];             // cycle start date     (7 car + le car de fin de chaîne \0 null terminator)
                char mEnd[7 + 1];               // cycle end date       (7 car + le car de fin de chaîne \0 null terminator)
            };

        } // End namespace File


        typedef std::shared_ptr<struct Index> IndexPtr;

        // Retour à l'alignement des données en mémoire, sauvegardé précédemment par 'pragma pack(push)'
        // (sans doute retour à 8 bits sur système 64 bits)
        #pragma pack(pop)

        //-------------------------------------------------------------------------------
        // STRUCTURE INDEX
        // NB : Étant sorti à ce stade du code, du 'namespace File' (cf. ligne ~374) dans lequel les structures
        // élémentaires ont été déclarées, il est désormais nécessaire pour accéder à celles-ci de préfixer leur
        // nom par 'File::'
        struct Index {
            std::vector<File::Config> mConfig;
            std::vector<File::Point> mPoints;
            std::vector<File::Waypoint> mWaypoints;
            std::vector<File::Navaid> mNavaids;
            std::vector<File::Airport> mAirports;
            std::vector<File::Runway> mRunways;
            std::vector<File::LegSequence> mLegSequences;
            std::vector<File::Leg> mLegs;
            std::vector<File::Procedure> mDepartures;
            std::vector<File::Procedure> mArrivals;
            std::vector<File::Approach> mApproaches;
            std::vector<File::ProcedureTransition> mDepartureTransitions;
            std::vector<File::ProcedureTransition> mArrivalTransitions;
            std::vector<File::ApproachTransition> mApproachTransitions;
            std::vector<File::RunwayProcedureTransition> mRunwayDepartureTransitions;
            std::vector<File::RunwayProcedureTransition> mRunwayArrivalTransitions;
            std::vector<File::Airway> mAirways;
            std::vector<File::AirwaySegment> mAirwaySegments;
            std::vector<File::AirwaySegmentLeg> mAirwaySegmentLegs;
            std::vector<File::Route> mRoutes;
            std::vector<File::RouteSegment> mRouteSegments;
            uint32 mSize;
            uint32 mHash;

            virtual ~Index() = default;

            virtual bool load(const std::string & inPath, Logger * inLogger);

            // load procedures from database.  returns non-zero in case of errors.
            virtual int loadProcedures(sint64) { return 0; }

            // returns true if all procedures was loaded during database initialization
            virtual bool isProceduresPreloaded() { return true; }

            virtual bool save(const std::string & inPath);

        }; // Fin de structure 'Index'

    } // End namespace navdata

} // End namespace ndbl

#endif // NAVDATA_FILE_H


