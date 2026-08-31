/*
 * Le binôme 'FileConverter.h' et 'FileConverter.cpp' constitue le coeur des fonctionnalités
 * recherchées dans cette l'application
 * C'est ce binôme qui assure le parsage binaire --> texte et le réencodage texte --> binaire
 * En utilisant la structure des données et les outils pour y accéder, définis par le binôme
 * 'NavDataFile.h /cpp', la compatibilité de format 'nav1.db' est assurée
 */

// FILECONVERTER.H
// DÉCLARE LA CLASSE FILECONVERTER ASSURANT LA CONVERTION DES FICHIERS DE BINAIRES À TEXTE ET INVERSEMENT

#ifndef FILECONVERTER_H
#define FILECONVERTER_H

#include <fstream>
#include <sstream>
#include <cstring>  // nécessaire pour std::memcpy dans safeStrCopy

#include "NavDataFile.h"

using namespace ndbl::navdata;

// DÉCLARATION DE LA CLASSE FILECONVERTER ============================================================================*
class FileConverter {

    private:
        // Noms des sections dans l'ordre de sauvegarde
        const std::vector<std::string> sectionNames = {
            "CONFIG", "POINTS", "WAYPOINTS", "NAVAIDS", "AIRPORTS", "RUNWAYS",
            "LEGSEQUENCES", "LEGS", "DEPARTURES", "ARRIVALS", "APPROACHES",
            "DEPARTURETRANSITIONS", "ARRIVALTRANSITIONS", "APPROACHTRANSITIONS",
            "RUNWAYDEPARTURETRANSITIONS", "RUNWAYARRIVALTRANSITIONS",
            "AIRWAYS", "AIRWAYSEGMENTS", "AIRWAYSEGMENTLEGS", "ROUTES", "ROUTESEGMENTS"
        };

    public:
        // Convertit un fichier binaire en format texte lisible
        bool convertBinaryToText(const std::string& binaryPath, const std::string& textPath);

        // Convertit un fichier texte en format binaire
        bool convertTextToBinary(const std::string& textPath, const std::string& binaryPath);

    private:

        // Méthodes de conversion BINAIRE vers TEXTE pour chaque type de section
        void writeConfigSection(                    std::ofstream& out, const std::vector<File::Config>& configs);
        void writePointsSection(                    std::ofstream& out, const std::vector<File::Point>& points);
        void writeWaypointsSection(                 std::ofstream& out, const std::vector<File::Waypoint>& waypoints);
        void writeNavaidsSection(                   std::ofstream& out, const std::vector<File::Navaid>& navaids);
        void writeAirportsSection(                  std::ofstream& out, const std::vector<File::Airport>& airports);
        void writeRunwaysSection(                   std::ofstream& out, const std::vector<File::Runway>& runways);
        void writeLegSequencesSection(              std::ofstream& out, const std::vector<File::LegSequence>& legSeqs);
        void writeLegsSection(                      std::ofstream& out, const std::vector<File::Leg>& legs);
        void writeProceduresSection(                std::ofstream& out, const std::string& sectionName,
                                                    const std::vector<File::Procedure>& procedures);
        void writeApproachesSection(                std::ofstream& out, const std::vector<File::Approach>& approaches);
        void writeProcedureTransitionsSection(      std::ofstream& out, const std::string& sectionName,
                                                    const std::vector<File::ProcedureTransition>& transitions);
        void writeApproachTransitionsSection(       std::ofstream& out, const std::vector<File::ApproachTransition>& transitions);
        void writeRunwayProcedureTransitionsSection(std::ofstream& out, const std::string& sectionName,
                                                    const std::vector<File::RunwayProcedureTransition>& transitions);
        void writeAirwaysSection(                   std::ofstream& out, const std::vector<File::Airway>& airways);
        void writeAirwaySegmentsSection(            std::ofstream& out, const std::vector<File::AirwaySegment>& segments);
        void writeAirwaySegmentLegsSection(         std::ofstream& out, const std::vector<File::AirwaySegmentLeg>& segmentLegs);
        void writeRoutesSection(                    std::ofstream& out, const std::vector<File::Route>& routes);
        void writeRouteSegmentsSection(             std::ofstream& out, const std::vector<File::RouteSegment>& routeSegments);




        // Méthodes de conversion (parsing) de lignes de données TEXTE vers BINAIRE
        bool parseDataLine(     const std::string& line, const std::string& section, Index& index);
        bool parseMetadataLine( const std::string& line, Index& index);

        // Utilitaires pour la gestion des chaînes
        void safeStrCopy(char* dest, const std::string& src, size_t destSize);
        bool parseQuotedString(                 std::istringstream& iss, std::string& result);

        // Méthodes de conversion (parsing) TEXTE vers BINAIRE pour chaque type de structure
        bool parseConfig(                       std::istringstream& iss, std::vector<File::Config>& configs);
        bool parsePoint(                        std::istringstream& iss, std::vector<File::Point>& points);
        bool parseWaypoint(                     std::istringstream& iss, std::vector<File::Waypoint>& waypoints);
        bool parseNavaid(                       std::istringstream& iss, std::vector<File::Navaid>& navaids);
        bool parseAirport(                      std::istringstream& iss, std::vector<File::Airport>& airports);
        bool parseRunway(                       std::istringstream& iss, std::vector<File::Runway>& runways);
        bool parseLegSequence(                  std::istringstream& iss, std::vector<File::LegSequence>& legSequences);
        bool parseLeg(                          std::istringstream& iss, std::vector<File::Leg>& legs);
        bool parseProcedure(                    std::istringstream& iss, std::vector<File::Procedure>& procedures);
        bool parseApproach(                     std::istringstream& iss, std::vector<File::Approach>& approaches);
        bool parseProcedureTransition(          std::istringstream& iss, std::vector<File::ProcedureTransition>& transitions);
        bool parseApproachTransition(           std::istringstream& iss, std::vector<File::ApproachTransition>& transitions);
        bool parseRunwayProcedureTransition(    std::istringstream& iss, std::vector<File::RunwayProcedureTransition>& transitions);
        bool parseAirway(                       std::istringstream& iss, std::vector<File::Airway>& airways);
        bool parseAirwaySegment(                std::istringstream& iss, std::vector<File::AirwaySegment>& segments);
        bool parseAirwaySegmentLeg(             std::istringstream& iss, std::vector<File::AirwaySegmentLeg>& segmentLegs);
        bool parseRoute(                        std::istringstream& iss, std::vector<File::Route>& routes);
        bool parseRouteSegment(                 std::istringstream& iss, std::vector<File::RouteSegment>& routeSegments);

};
// FIN DE DÉCLARATION DE LA CLASSE FILECONVERTER =====================================================================*


#endif // FILE_CONVERTER_H
