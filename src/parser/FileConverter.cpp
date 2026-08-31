
/*  FFB777Parseur fait partie intégrante du projet FFB777Navdata en tant que parseur /encodeur
    Le programme a pour objectif de parser le fichier binaire nav1.db en un texte nav1.txt
    puis de réencoder le fichier texte nav1.txt (après modification) en un binaire nav1-2.db
*/


#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>

#include "FileConverter.h"

using namespace ndbl::navdata;


// DÉFINITION DES MÉTHODES DE LA CLASSE FILECONVERTER


// =====================================================================================================
// CONVERTIR UN FICHIER BINAIRE AU FORMAT TEXTE - MÉTHODE GLOBALE
//
bool FileConverter::convertBinaryToText(const std::string& binaryPath, const std::string& textPath) {
    // Charger les données binaires
    Index index;
    if (!index.load(binaryPath, nullptr)) {
        std::cerr << "Erreur lors du chargement du fichier binaire: " << binaryPath << std::endl;
        return false;
    }

    // Ouvrir le fichier de sortie texte
    std::ofstream outFile(textPath);
    if (!outFile.is_open()) {
        std::cerr << "Impossible d'ouvrir le fichier de sortie: " << textPath << std::endl;
        return false;
    }

    // En-tête du fichier avec métadonnées (CRITIQUE pour la reconstruction)
    outFile << "# NavData Text Format\n";
    /* Modification pour éviter les différences entre binaires décodé et recodé --> NE PAS SAUVEGARDER
     * LES MÉTADONNÉES (qui sont de ttes façons différentes entre les deux fichiers)
     * Les deux lignes suivantes ont été commentées :
    // outFile << "# METADATA_SIZE: " << index.mSize << "\n";
    // outFile << "# METADATA_HASH: 0x" << std::hex << index.mHash << std::dec << "\n\n";
     * La ligne suivante est ajoutée :       */
    outFile << "\n";
    // Fin de Modification

    // Convertir chaque section
    writeConfigSection(
                outFile, index.mConfig);
    writePointsSection(
                outFile, index.mPoints);
    writeWaypointsSection(
                outFile, index.mWaypoints);
    writeNavaidsSection(
                outFile, index.mNavaids);
    writeAirportsSection(
                outFile, index.mAirports);
    writeRunwaysSection(
                outFile, index.mRunways);
    writeLegSequencesSection(
                outFile, index.mLegSequences);
    writeLegsSection(
                outFile, index.mLegs);
    writeProceduresSection(
                outFile, "DEPARTURES", index.mDepartures);
    writeProceduresSection(
                outFile, "ARRIVALS", index.mArrivals);
    writeApproachesSection(
                outFile, index.mApproaches);
    writeProcedureTransitionsSection(
                outFile, "DEPARTURETRANSITIONS", index.mDepartureTransitions);
    writeProcedureTransitionsSection(
                outFile, "ARRIVALTRANSITIONS", index.mArrivalTransitions);
    writeApproachTransitionsSection(
                outFile, index.mApproachTransitions);
    writeRunwayProcedureTransitionsSection(
                outFile, "RUNWAYDEPARTURETRANSITIONS", index.mRunwayDepartureTransitions);
    writeRunwayProcedureTransitionsSection(
                outFile, "RUNWAYARRIVALTRANSITIONS", index.mRunwayArrivalTransitions);
    writeAirwaysSection(
                outFile, index.mAirways);
    writeAirwaySegmentsSection(
                outFile, index.mAirwaySegments);
    writeAirwaySegmentLegsSection(
                outFile, index.mAirwaySegmentLegs);
    writeRoutesSection(
                outFile, index.mRoutes);
    writeRouteSegmentsSection(
                outFile, index.mRouteSegments);

    outFile.close();
    std::cout << "Texte créé : " << textPath << std::endl;
    return true;
}


// =====================================================================================================
// CONVERTIR UN FICHIER TEXTE AU FORMAT BINAIRE - MÉTHODE GLOBALE
//
bool FileConverter::convertTextToBinary(const std::string& textPath, const std::string& binaryPath) {
    std::ifstream inFile(textPath);
    if (!inFile.is_open()) {
        std::cerr << "Impossible d'ouvrir le fichier texte: " << textPath << std::endl;
        return false;
    }

    Index index;
    std::string line;
    std::string currentSection;

    // Parcourir le fichier ligne par ligne
    while (std::getline(inFile, line)) {
        // Ignorer les lignes vides
        if (line.empty()) {
            continue;
        }

        // Traiter les métadonnées AVANT de les ignorer comme commentaires
        if (line[0] == '#') {
            if (!parseMetadataLine(line, index)) {
                // Si ce n'est pas une métadonnée, on ignore (commentaire normal)
            }
            continue;
        }

        // Détecter les sections
        if (line.find("[") == 0 && line.find("]") != std::string::npos) {
            currentSection = line.substr(1, line.find("]") - 1);
            continue;
        }

        // Parser les données selon la section
        if (!parseDataLine(line, currentSection, index)) {
            std::cerr << "Erreur de parsing ligne: " << line << std::endl;
            return false;
        }
    }

    inFile.close();

    // Sauvegarder au format binaire
    if (!index.save(binaryPath)) {
        std::cerr << "Erreur lors de la sauvegarde du fichier binaire: " << binaryPath << std::endl;
        return false;
    }

    std::cout << "Binaire créé : " << binaryPath << std::endl;
    return true;
}


// =====================================================================================================
// PARSER LES MÉTADONNÉES DU FICHIER
//
bool FileConverter::parseMetadataLine(const std::string& line, Index& index) {
    if (line.find("# METADATA_SIZE:") == 0) {
        std::string sizeStr = line.substr(16); // Après "# METADATA_SIZE:"
        // Supprimer les espaces de début
        size_t start = sizeStr.find_first_not_of(" \t");
        if (start != std::string::npos) {
            sizeStr = sizeStr.substr(start);
            try {
                index.mSize = std::stoull(sizeStr);
                return true;
            } catch (const std::exception& e) {
                std::cerr << "Erreur parsing taille: " << e.what() << std::endl;
            }
        }
    } else if (line.find("# METADATA_HASH:") == 0) {
        std::string hashStr = line.substr(16); // Après "# METADATA_HASH:"
        size_t hexPos = hashStr.find("0x");
        if (hexPos != std::string::npos) {
            hashStr = hashStr.substr(hexPos + 2);
            try {
                index.mHash = std::stoull(hashStr, nullptr, 16);
                return true;
            } catch (const std::exception& e) {
                std::cerr << "Erreur parsing hash: " << e.what() << std::endl;
            }
        }
    }
    return false; // Pas une métadonnée reconnue
}


// =====================================================================================================
// COPIE SÉCURISÉE DES CHAINES C
//
void FileConverter::safeStrCopy(char* dest, const std::string& src, size_t destSize) {
    if (destSize == 0) return;

    size_t copyLen = std::min(src.length(), destSize - 1);
    std::memcpy(dest, src.c_str(), copyLen);
    dest[copyLen] = '\0';
}


// =====================================================================================================
// MÉTHODES D'ÉCRITURE POUR CHAQUE TYPE DE SECTION (ORDRE RESPECTANT CELUI DES STRUCTURES)
//
// Conversion TEXTE de la section [CONFIG] =============================================================
void FileConverter::writeConfigSection(std::ofstream& out, const std::vector<File::Config>& configs) {
    out << "[CONFIG]\n";
    out << "# Count: " << configs.size() << "\n";
    for (size_t i = 0; i < configs.size(); ++i) {
        const auto& config = configs[i];
        out << "CONFIG " << i << " \"" << config.mSource << "\" \"" << config.mCycle
            << "\" \"" << config.mSequence << "\" \"" << config.mStart
            << "\" \"" << config.mEnd << "\"\n";
    }
    out << "\n";
}

// Conversion en TEXTE de la section [POINTS] ==========================================================
void FileConverter::writePointsSection(std::ofstream& out, const std::vector<File::Point>& points) {
    out << "[POINTS]\n";
    out << "# Count: " << points.size() << "\n";
    for (size_t i = 0; i < points.size(); ++i) {
        const auto& point = points[i];
        out << "POINT " << i << " \"" << point.mIdent << "\" "
            << std::fixed << std::setprecision(12) << point.mLat << " " << point.mLon << " "
            << std::setprecision(6) << point.mMagVar << " " << point.mHoldCourse << " "
            << std::setprecision(3) << point.mHoldDistInMeters << " " << point.mHoldTime << " "
            << (int)point.mHoldSide << "\n";
    }
    out << "\n";
}

// Conversion en TEXTE de la section [WAYPOINTS] =======================================================
void FileConverter::writeWaypointsSection(std::ofstream& out, const std::vector<File::Waypoint>& waypoints) {
    out << "[WAYPOINTS]\n";
    out << "# Count: " << waypoints.size() << "\n";
    for (size_t i = 0; i < waypoints.size(); ++i) {
        const auto& wp = waypoints[i];
        out << "WAYPOINT " << i << " " << wp.mPointId << "\n";
    }
    out << "\n";
}

// Conversion en TEXTE de la section [NAVAIDS] =========================================================
void FileConverter::writeNavaidsSection(std::ofstream& out, const std::vector<File::Navaid>& navaids) {
    out << "[NAVAIDS]\n";
    out << "# Count: " << navaids.size() << "\n";
    for (size_t i = 0; i < navaids.size(); ++i) {
        const auto& nav = navaids[i];
        // ORDRE CORRIGÉ selon la structure Navaid
        out << "NAVAID " << i << " " << nav.mType << " " << nav.mPointId << " "
            << nav.mDistanceNavaidId << " " << std::fixed << std::setprecision(6)
            << nav.mElevationInMeters << " " << nav.mDeclination << " "
            << nav.mFigureOfMerit << " " << nav.mFreq << " " << nav.mCategory << " "
            << nav.mCourse << " " << nav.mAngle << " " << nav.mRunwayId << "\n";
    }
    out << "\n";
}

// Conversion en TEXTE de la section [AIRPORTS] ========================================================
void FileConverter::writeAirportsSection(std::ofstream& out, const std::vector<File::Airport>& airports) {
    out << "[AIRPORTS]\n";
    out << "# Count: " << airports.size() << "\n";
    for (size_t i = 0; i < airports.size(); ++i) {
        const auto& airport = airports[i];
        out << "AIRPORT " << i << " " << airport.mPointId << " "
            << std::fixed << std::setprecision(6) << airport.mElevationInMeters << " "
            << airport.mLimitSpeedInMetersPerSec << " " << airport.mLimitAltitudeInMeters << " "
            << airport.mTransitionAltitudeInMeters << " " << airport.mTransitionLevelInMeters << "\n";
    }
    out << "\n";
}

// Conversion en TEXTE de la section [RUNWAYS] =========================================================
void FileConverter::writeRunwaysSection(std::ofstream& out, const std::vector<File::Runway>& runways) {
    out << "[RUNWAYS]\n";
    out << "# Count: " << runways.size() << "\n";
    for (size_t i = 0; i < runways.size(); ++i) {
        const auto& runway = runways[i];
        out << "RUNWAY " << i << " " << runway.mAirportId << " " << runway.mPointId << " "
            << std::fixed << std::setprecision(6) << runway.mElevationInMeters << " "
            << runway.mGradient << " " << runway.mCourse << " " << runway.mLengthInMeters << " "
            << runway.mDisplacedInMeters << " " << runway.mStopwayInMeters << " "
            << runway.mCrossInMeters << "\n";
    }
    out << "\n";
}

// Conversion en TEXTE de la section [LEGSEQUENCES] ====================================================
void FileConverter::writeLegSequencesSection(std::ofstream& out, const std::vector<File::LegSequence>& legSeqs) {
    out << "[LEGSEQUENCES]\n";
    out << "# Count: " << legSeqs.size() << "\n";
    for (size_t i = 0; i < legSeqs.size(); ++i) {
        const auto& legSeq = legSeqs[i];
        out << "LEGSEQUENCE " << i << " \"" << legSeq.mIdent << "\" "
            << (int)legSeq.mSequenceType << " " << std::fixed << std::setprecision(6)
            << legSeq.mTransitionInMeters << "\n";
    }
    out << "\n";
}

// Conversion en TEXTE de la section [LEGS] ============================================================
void FileConverter::writeLegsSection(std::ofstream& out, const std::vector<File::Leg>& legs) {
    out << "[LEGS]\n";
    out << "# Count: " << legs.size() << "\n";
    for (size_t i = 0; i < legs.size(); ++i) {
        const auto& leg = legs[i];
        // ORDRE EXACT selon la structure Leg :
        // mCode, mLegSequenceId, mPointId, mPointUsage, mCourse, mDistanceInMeters,
        // mNavaidId, mNavaidCourse, mNavaidDistanceInMeters, mAltitudeLimitMinInMeters,
        // mAltitudeLimitMaxInMeters, mAirSpeedLimit, mPath, mTurnDir, mRnpInMeters
        out << "LEG " << i << " " << (int)leg.mCode << " " << leg.mLegSequenceId << " "
            << leg.mPointId << " " << leg.mPointUsage << " "
            << std::fixed << std::setprecision(6) << leg.mCourse << " "
            << leg.mDistanceInMeters << " " << leg.mNavaidId << " " << leg.mNavaidCourse << " "
            << leg.mNavaidDistanceInMeters << " " << leg.mAltitudeLimitMinInMeters << " "
            << leg.mAltitudeLimitMaxInMeters << " " << leg.mAirSpeedLimit << " "
            << leg.mPath << " " << (int)leg.mTurnDir << " " << leg.mRnpInMeters << "\n";
    }
    out << "\n";
}

// Conversion en TEXTE des sections [DEPARTURES] et [ARRIVALS] =========================================
void FileConverter::writeProceduresSection(std::ofstream& out, const std::string& sectionName,
                                           const std::vector<File::Procedure>& procedures) {
    out << "[" << sectionName << "]\n";
    out << "# Count: " << procedures.size() << "\n";
    for (size_t i = 0; i < procedures.size(); ++i) {
        const auto& proc = procedures[i];
        out << "PROCEDURE " << i << " " << proc.mAirportId << " " << proc.mLegSequenceId << "\n";
    }
    out << "\n";
}

// Conversion en TEXTE de la section [APPROACHES] ======================================================
void FileConverter::writeApproachesSection(std::ofstream& out, const std::vector<File::Approach>& approaches) {
    out << "[APPROACHES]\n";
    out << "# Count: " << approaches.size() << "\n";
    for (size_t i = 0; i < approaches.size(); ++i) {
        const auto& approach = approaches[i];
        out << "APPROACH " << i << " " << approach.mRunwayId << " " << approach.mLegSequenceId << " "
            << std::fixed << std::setprecision(6) << approach.mDecisionHeightInMeters << " "
            << approach.mMinimumDescentInMeters << "\n";
    }
    out << "\n";
}

// Conversion en TEXTE des sections [DEPARTURETRANSITIONS] et [ARRIVALTRANSITIONS] =====================
void FileConverter::writeProcedureTransitionsSection(std::ofstream& out, const std::string& sectionName,
                                                     const std::vector<File::ProcedureTransition>& transitions) {
    out << "[" << sectionName << "]\n";
    out << "# Count: " << transitions.size() << "\n";
    for (size_t i = 0; i < transitions.size(); ++i) {
        const auto& trans = transitions[i];
        out << "PROCEDURETRANSITION " << i << " " << trans.mProcedureId << " " << trans.mLegSequenceId << "\n";
    }
    out << "\n";
}

// Conversion en TEXTE de la section [APPROACHTRANSITIONS] =============================================
void FileConverter::writeApproachTransitionsSection(std::ofstream& out, const std::vector<File::ApproachTransition>& transitions) {
    out << "[APPROACHTRANSITIONS]\n";
    out << "# Count: " << transitions.size() << "\n";
    for (size_t i = 0; i < transitions.size(); ++i) {
        const auto& trans = transitions[i];
        out << "APPROACHTRANSITION " << i << " " << trans.mApproachId << " " << trans.mLegSequenceId << "\n";
    }
    out << "\n";
}

// Conversion en TEXTE des sections [RUNWAYDEPARTURETRANSITIONS] ET [RUNWAYARRIVALTRANSITIONS] =========
void FileConverter::writeRunwayProcedureTransitionsSection(std::ofstream& out, const std::string& sectionName,
                                                           const std::vector<File::RunwayProcedureTransition>& transitions) {
    out << "[" << sectionName << "]\n";
    out << "# Count: " << transitions.size() << "\n";
    for (size_t i = 0; i < transitions.size(); ++i) {
        const auto& trans = transitions[i];
        out << "RUNWAYPROCEDURETRANSITION " << i << " " << trans.mRunwayId << " "
            << trans.mProcedureId << " " << trans.mEngineOutProcedureId << " "
            << trans.mLegSequenceId << "\n";
    }
    out << "\n";
}

// Conversion en TEXTE de la section [AIRWAYS] =========================================================
void FileConverter::writeAirwaysSection(std::ofstream& out, const std::vector<File::Airway>& airways) {
    out << "[AIRWAYS]\n";
    out << "# Count: " << airways.size() << "\n";
    for (size_t i = 0; i < airways.size(); ++i) {
        const auto& airway = airways[i];
        out << "AIRWAY " << i << " \"" << airway.mIdent << "\"\n";
    }
    out << "\n";
}

// Conversion en TEXTE de la section [AIRWAYSEGMENTS] ==================================================
void FileConverter::writeAirwaySegmentsSection(std::ofstream& out, const std::vector<File::AirwaySegment>& segments) {
    out << "[AIRWAYSEGMENTS]\n";
    out << "# Count: " << segments.size() << "\n";
    for (size_t i = 0; i < segments.size(); ++i) {
        const auto& segment = segments[i];
        out << "AIRWAYSEGMENT " << i << " " << segment.mAirwayId << "\n";
    }
    out << "\n";
}

// Conversion en TEXTE de la section [AIRWAYSEGMENTLEGS] ===============================================
void FileConverter::writeAirwaySegmentLegsSection(std::ofstream& out, const std::vector<File::AirwaySegmentLeg>& segmentLegs) {
    out << "[AIRWAYSEGMENTLEGS]\n";
    out << "# Count: " << segmentLegs.size() << "\n";
    for (size_t i = 0; i < segmentLegs.size(); ++i) {
        const auto& segmentLeg = segmentLegs[i];
        out << "AIRWAYSEGMENTLEG " << i << " " << segmentLeg.mAirwaySegmentId << " " << segmentLeg.mPointId << "\n";
    }
    out << "\n";
}

// Conversion en TEXTE de la section [ROUTES] ==========================================================
void FileConverter::writeRoutesSection(std::ofstream& out, const std::vector<File::Route>& routes) {
    out << "[ROUTES]\n";
    out << "# Count: " << routes.size() << "\n";
    for (size_t i = 0; i < routes.size(); ++i) {
        const auto& route = routes[i];
        // ORDRE CORRIGÉ selon la structure Route
        out << "ROUTE " << i << " \"" << route.mIdent << "\" \"" << route.mCompany << "\" "
            << route.mDepartureAirportId << " " << route.mDepartureRunwayId << " "
            << route.mDepartureId << " " << route.mDepartureTransitionId << " "
            << route.mArrivalId << " " << route.mArrivalTransitionId << " "
            << route.mApproachId << " " << route.mApproachTransitionId << " "
            << route.mArrivalAirportId << " " << route.mAlternateRouteId << " "
            << route.mCostIndex << " " << route.mCruiseLevel << "\n";
    }
    out << "\n";
}

// Conversion en TEXTE de la section [ROUTESEGMENTS] ===================================================
void FileConverter::writeRouteSegmentsSection(std::ofstream& out, const std::vector<File::RouteSegment>& routeSegments) {
    out << "[ROUTESEGMENTS]\n";
    out << "# Count: " << routeSegments.size() << "\n";
    for (size_t i = 0; i < routeSegments.size(); ++i) {
        const auto& routeSegment = routeSegments[i];
        out << "ROUTESEGMENT " << i << " " << routeSegment.mRouteId << " "
            << routeSegment.mAirwayId << " " << routeSegment.mPointId << "\n";
    }
    out << "\n";
}



// =====================================================================================================
// PARSING D'UNE LIGNE DE DONNÉES TEXTE VERS BINAIRE SELON LA SECTION - MÉTHODE GLOBALE
//
bool FileConverter::parseDataLine(const std::string& line, const std::string& section, Index& index) {
    std::istringstream iss(line);
    std::string keyword;
    iss >> keyword;

    if (section == "CONFIG" && keyword == "CONFIG") {
        return parseConfig(iss, index.mConfig);
    } else if (section == "POINTS" && keyword == "POINT") {
        return parsePoint(iss, index.mPoints);
    } else if (section == "WAYPOINTS" && keyword == "WAYPOINT") {
        return parseWaypoint(iss, index.mWaypoints);
    } else if (section == "NAVAIDS" && keyword == "NAVAID") {
        return parseNavaid(iss, index.mNavaids);
    } else if (section == "AIRPORTS" && keyword == "AIRPORT") {
        return parseAirport(iss, index.mAirports);
    } else if (section == "RUNWAYS" && keyword == "RUNWAY") {
        return parseRunway(iss, index.mRunways);
    } else if (section == "LEGSEQUENCES" && keyword == "LEGSEQUENCE") {
        return parseLegSequence(iss, index.mLegSequences);
    } else if (section == "LEGS" && keyword == "LEG") {
        return parseLeg(iss, index.mLegs);
    } else if (section == "DEPARTURES" && keyword == "PROCEDURE") {
        return parseProcedure(iss, index.mDepartures);
    } else if (section == "ARRIVALS" && keyword == "PROCEDURE") {
        return parseProcedure(iss, index.mArrivals);
    } else if (section == "APPROACHES" && keyword == "APPROACH") {
        return parseApproach(iss, index.mApproaches);
    } else if (section == "DEPARTURETRANSITIONS" && keyword == "PROCEDURETRANSITION") {
        return parseProcedureTransition(iss, index.mDepartureTransitions);
    } else if (section == "ARRIVALTRANSITIONS" && keyword == "PROCEDURETRANSITION") {
        return parseProcedureTransition(iss, index.mArrivalTransitions);
    } else if (section == "APPROACHTRANSITIONS" && keyword == "APPROACHTRANSITION") {
        return parseApproachTransition(iss, index.mApproachTransitions);
    } else if (section == "RUNWAYDEPARTURETRANSITIONS" && keyword == "RUNWAYPROCEDURETRANSITION") {
        return parseRunwayProcedureTransition(iss, index.mRunwayDepartureTransitions);
    } else if (section == "RUNWAYARRIVALTRANSITIONS" && keyword == "RUNWAYPROCEDURETRANSITION") {
        return parseRunwayProcedureTransition(iss, index.mRunwayArrivalTransitions);
    } else if (section == "AIRWAYS" && keyword == "AIRWAY") {
        return parseAirway(iss, index.mAirways);
    } else if (section == "AIRWAYSEGMENTS" && keyword == "AIRWAYSEGMENT") {
        return parseAirwaySegment(iss, index.mAirwaySegments);
    } else if (section == "AIRWAYSEGMENTLEGS" && keyword == "AIRWAYSEGMENTLEG") {
        return parseAirwaySegmentLeg(iss, index.mAirwaySegmentLegs);
    } else if (section == "ROUTES" && keyword == "ROUTE") {
        return parseRoute(iss, index.mRoutes);
    } else if (section == "ROUTESEGMENTS" && keyword == "ROUTESEGMENT") {
        return parseRouteSegment(iss, index.mRouteSegments);
    }

    return false; // Section ou mot-clé non reconnu
}



// =====================================================================================================
// PARSING EN BINAIRE DE LA SECTION [CONFIG] - MÉTHODE ÉLÉMENTAIRE
//
bool FileConverter::parseConfig(std::istringstream& iss, std::vector<File::Config>& configs) {
    size_t index;
    std::string source, cycle, sequence, start, end;

    if (!(iss >> index)) return false;

    // Parser les chaînes entre guillemets
    if (!parseQuotedString(iss, source) ||
        !parseQuotedString(iss, cycle) ||
        !parseQuotedString(iss, sequence) ||
        !parseQuotedString(iss, start) ||
        !parseQuotedString(iss, end)) {
        return false;
    }

    // Assurer que le vecteur a la bonne taille
    if (index >= configs.size()) {
        configs.resize(index + 1);

        // Modification visant à diminuer les différences entre
        // binaires décodé /recodé : Initialiser explicitement le nouvel élément
        memset(&configs[index], 0, sizeof(File::Config));
        // Fin de modification
    }

    File::Config& config = configs[index];
    safeStrCopy(config.mSource, source, sizeof(config.mSource));
    safeStrCopy(config.mCycle, cycle, sizeof(config.mCycle));
    safeStrCopy(config.mSequence, sequence, sizeof(config.mSequence));
    safeStrCopy(config.mStart, start, sizeof(config.mStart));
    safeStrCopy(config.mEnd, end, sizeof(config.mEnd));

    return true;
}


// =====================================================================================================
// PARSING EN BINAIRE DE LA SECTION [POINT] - MÉTHODE ÉLÉMENTAIRE
//
bool FileConverter::parsePoint(std::istringstream& iss, std::vector<File::Point>& points) {
    size_t index;
    std::string ident;
    double lat, lon;
    float magVar, holdCourse, holdDistInMeters, holdTime;
    int holdSide;

    if (!(iss >> index)) return false;

    if (!parseQuotedString(iss, ident)) return false;

    if (!(iss >> lat >> lon >> magVar >> holdCourse >> holdDistInMeters >> holdTime >> holdSide)) {
        return false;
    }

    if (index >= points.size()) {
        points.resize(index + 1);

        // Modification visant à diminuer les différences entre
        // binaires décodé /recodé : Initialiser explicitement le nouvel élément
        memset(&points[index], 0, sizeof(File::Point));
        // Fin de modification
    }

    File::Point& point = points[index];
    safeStrCopy(point.mIdent, ident, sizeof(point.mIdent));
    point.mLat = lat;
    point.mLon = lon;
    point.mMagVar = magVar;
    point.mHoldCourse = holdCourse;
    point.mHoldDistInMeters = holdDistInMeters;
    point.mHoldTime = holdTime;
    point.mHoldSide = static_cast<uint8>(holdSide); // ex uint8_t

    return true;
}


// =====================================================================================================
// PARSING EN BINAIRE DE LA SECTION [WAYPOINT] - MÉTHODE ÉLÉMENTAIRE
//
bool FileConverter::parseWaypoint(std::istringstream& iss, std::vector<File::Waypoint>& waypoints) {
    size_t index;
    uint32 pointId;

    if (!(iss >> index >> pointId)) return false;

    if (index >= waypoints.size()) {
        waypoints.resize(index + 1);

        // Modification visant à diminuer les différences entre
        // binaires décodé /recodé : Initialiser explicitement le nouvel élément
        memset(&waypoints[index], 0, sizeof(File::Waypoint));
        // Fin de modification
    }

    waypoints[index].mPointId = pointId;
    return true;
}


// =====================================================================================================
// PARSING EN BINAIRE DE LA SECTION [NAVAID] - MÉTHODE ÉLÉMENTAIRE
//
bool FileConverter::parseNavaid(std::istringstream& iss, std::vector<File::Navaid>& navaids) {
    size_t index;
    uint32 type;          // Modif pour éviter débordement
    uint32 pointId, distanceNavaidId, figureOfMerit, freq, category, runwayId;
    float elevationInMeters, declination, course, angle;

    // ORDRE EXACT selon la structure Navaid :
    // mType, mPointId, mDistanceNavaidId, mElevationInMeters, mDeclination,
    // mFigureOfMerit, mFreq, mCategory, mCourse, mAngle, mRunwayId
    if (!(iss >> index >> type >> pointId >> distanceNavaidId >> elevationInMeters
          >> declination >> figureOfMerit >> freq >> category >> course >> angle >> runwayId)) {
        return false;
    }

    if (index >= navaids.size()) {
        navaids.resize(index + 1);

        // Modification visant à diminuer les différences entre
        // binaires décodé /recodé : Initialiser explicitement le nouvel élément
        memset(&navaids[index], 0, sizeof(File::Navaid));
        // Fin de modification
    }

    File::Navaid& navaid = navaids[index];
    navaid.mType = static_cast<uint16>(type); // Modif pour éviter débordement (ex uint16_t)
    navaid.mPointId = pointId;
    navaid.mDistanceNavaidId = distanceNavaidId;
    navaid.mElevationInMeters = elevationInMeters;
    navaid.mDeclination = declination;
    navaid.mFigureOfMerit = figureOfMerit;
    navaid.mFreq = freq;
    navaid.mCategory = category;
    navaid.mCourse = course;
    navaid.mAngle = angle;
    navaid.mRunwayId = runwayId;

    return true;
}


// =====================================================================================================
// PARSING EN BINAIRE DE LA SECTION [AIRPORT] - MÉTHODE ÉLÉMENTAIRE
//
bool FileConverter::parseAirport(std::istringstream& iss, std::vector<File::Airport>& airports) {
    size_t index;
    uint32 pointId;
    float elevationInMeters, limitSpeedInMetersPerSec, limitAltitudeInMeters;
    float transitionAltitudeInMeters, transitionLevelInMeters;

    if (!(iss >> index >> pointId >> elevationInMeters >> limitSpeedInMetersPerSec
          >> limitAltitudeInMeters >> transitionAltitudeInMeters >> transitionLevelInMeters)) {
        return false;
    }

    if (index >= airports.size()) {
        airports.resize(index + 1);

        // Modification visant à diminuer les différences entre
        // binaires décodé /recodé : Initialiser explicitement le nouvel élément
        memset(&airports[index], 0, sizeof(File::Airport));
        // Fin de modification
    }

    File::Airport& airport = airports[index];
    airport.mPointId = pointId;
    airport.mElevationInMeters = elevationInMeters;
    airport.mLimitSpeedInMetersPerSec = limitSpeedInMetersPerSec;
    airport.mLimitAltitudeInMeters = limitAltitudeInMeters;
    airport.mTransitionAltitudeInMeters = transitionAltitudeInMeters;
    airport.mTransitionLevelInMeters = transitionLevelInMeters;

    return true;
}


// =====================================================================================================
// PARSING EN BINAIRE DE LA SECTION [RUNWAY] - MÉTHODE ÉLÉMENTAIRE
//
bool FileConverter::parseRunway(std::istringstream& iss, std::vector<File::Runway>& runways) {
    size_t index;
    uint32 airportId, pointId;
    float elevationInMeters, gradient, course, lengthInMeters;
    float displacedInMeters, stopwayInMeters, crossInMeters;

    if (!(iss >> index >> airportId >> pointId >> elevationInMeters >> gradient
          >> course >> lengthInMeters >> displacedInMeters >> stopwayInMeters >> crossInMeters)) {
        return false;
    }

    if (index >= runways.size()) {
        runways.resize(index + 1);

        // Modification visant à diminuer les différences entre
        // binaires décodé /recodé : Initialiser explicitement le nouvel élément
        memset(&runways[index], 0, sizeof(File::Runway));
        // Fin de modification
    }

    File::Runway& runway = runways[index];
    runway.mAirportId = airportId;
    runway.mPointId = pointId;
    runway.mElevationInMeters = elevationInMeters;
    runway.mGradient = gradient;
    runway.mCourse = course;
    runway.mLengthInMeters = lengthInMeters;
    runway.mDisplacedInMeters = displacedInMeters;
    runway.mStopwayInMeters = stopwayInMeters;
    runway.mCrossInMeters = crossInMeters;

    return true;
}


// =====================================================================================================
// PARSING EN BINAIRE DE LA SECTION [LEGSEQUENCE] - MÉTHODE ÉLÉMENTAIRE
//
bool FileConverter::parseLegSequence(std::istringstream& iss, std::vector<File::LegSequence>& legSequences) {
    size_t index;
    std::string ident;
    int sequenceType;
    float transitionInMeters;

    if (!(iss >> index)) return false;

    if (!parseQuotedString(iss, ident)) return false;

    if (!(iss >> sequenceType >> transitionInMeters)) return false;

    if (index >= legSequences.size()) {
        legSequences.resize(index + 1);

        // Modification visant à diminuer les différences entre
        // binaires décodé /recodé : Initialiser explicitement le nouvel élément
        memset(&legSequences[index], 0, sizeof(File::LegSequence));
        // Fin de modification
    }

    File::LegSequence& legSeq = legSequences[index];
    safeStrCopy(legSeq.mIdent, ident, sizeof(legSeq.mIdent));
    legSeq.mSequenceType = static_cast<uint8>(sequenceType);  //ex uint8_t
    legSeq.mTransitionInMeters = transitionInMeters;

    return true;
}



// =====================================================================================================
// PARSING EN BINAIRE DE LA SECTION [LEG] - MÉTHODE ÉLÉMENTAIRE
//
bool FileConverter::parseLeg(std::istringstream& iss, std::vector<File::Leg>& legs) {
    size_t index;
    uint32 code; // Modif pour éviter débordement
    int turnDir;
    uint32 legSequenceId, pointId, pointUsage, navaidId;
    float course, distanceInMeters, navaidCourse, navaidDistanceInMeters;
    float altitudeLimitMinInMeters, altitudeLimitMaxInMeters, airSpeedLimit;
    float path, rnpInMeters;

    // ORDRE EXACT selon la structure Leg
    if (!(iss >> index >> code >> legSequenceId >> pointId >> pointUsage >> course
          >> distanceInMeters >> navaidId >> navaidCourse >> navaidDistanceInMeters
          >> altitudeLimitMinInMeters >> altitudeLimitMaxInMeters >> airSpeedLimit
          >> path >> turnDir >> rnpInMeters)) {
        return false;
    }

    if (index >= legs.size()) {
        legs.resize(index + 1);

        // Modification visant à diminuer les différences entre
        // binaires décodé /recodé : Initialiser explicitement le nouvel élément
        memset(&legs[index], 0, sizeof(File::Leg));
        // Fin de modification
    }

    File::Leg& leg = legs[index];
    leg.mCode = static_cast<uint16>(code); // Modif pour éviter débordement (ex uint16_t)
    leg.mLegSequenceId = legSequenceId;
    leg.mPointId = pointId;
    leg.mPointUsage = pointUsage;
    leg.mCourse = course;
    leg.mDistanceInMeters = distanceInMeters;
    leg.mNavaidId = navaidId;
    leg.mNavaidCourse = navaidCourse;
    leg.mNavaidDistanceInMeters = navaidDistanceInMeters;
    leg.mAltitudeLimitMinInMeters = altitudeLimitMinInMeters;
    leg.mAltitudeLimitMaxInMeters = altitudeLimitMaxInMeters;
    leg.mAirSpeedLimit = airSpeedLimit;
    leg.mPath = path;
    leg.mTurnDir = static_cast<uint8>(turnDir); // ex uint8_t
    leg.mRnpInMeters = rnpInMeters;

    return true;
}


// =====================================================================================================
// PARSING EN BINAIRE DES SECTIONS [SID] ET [STAR] - MÉTHODE ÉLÉMENTAIRE
//
bool FileConverter::parseProcedure(std::istringstream& iss, std::vector<File::Procedure>& procedures) {
    size_t index;
    uint32 airportId, legSequenceId;

    if (!(iss >> index >> airportId >> legSequenceId)) return false;

    if (index >= procedures.size()) {
        procedures.resize(index + 1);

        // Modification visant à diminuer les différences entre
        // binaires décodé /recodé : Initialiser explicitement le nouvel élément
        memset(&procedures[index], 0, sizeof(File::Procedure));
        // Fin de modification
    }

    File::Procedure& procedure = procedures[index];
    procedure.mAirportId = airportId;
    procedure.mLegSequenceId = legSequenceId;

    return true;
}


// =====================================================================================================
// PARSING EN BINAIRE DE LA SECTION [APPROACHES] - MÉTHODE ÉLÉMENTAIRE
//
bool FileConverter::parseApproach(std::istringstream& iss, std::vector<File::Approach>& approaches) {
    size_t index;
    uint32 runwayId, legSequenceId;
    float decisionHeightInMeters, minimumDescentInMeters;

    if (!(iss >> index >> runwayId >> legSequenceId >> decisionHeightInMeters >> minimumDescentInMeters)) {
        return false;
    }

    if (index >= approaches.size()) {
        approaches.resize(index + 1);

        // Modification visant à diminuer les différences entre
        // binaires décodé /recodé : Initialiser explicitement le nouvel élément
        memset(&approaches[index], 0, sizeof(File::Approach));
        // Fin de modification
    }

    File::Approach& approach = approaches[index];
    approach.mRunwayId = runwayId;
    approach.mLegSequenceId = legSequenceId;
    approach.mDecisionHeightInMeters = decisionHeightInMeters;
    approach.mMinimumDescentInMeters = minimumDescentInMeters;

    return true;
}


// =====================================================================================================
// PARSING EN BINAIRE DES SECTIONS [SID] ET [STAR TRANSITION] - MÉTHODE ÉLÉMENTAIRE
//
bool FileConverter::parseProcedureTransition(std::istringstream& iss,
                                             std::vector<File::ProcedureTransition>& transitions) {
    size_t index;
    uint32 procedureId, legSequenceId;

    if (!(iss >> index >> procedureId >> legSequenceId)) return false;

    if (index >= transitions.size()) {
        transitions.resize(index + 1);

        // Modification visant à diminuer les différences entre
        // binaires décodé /recodé : Initialiser explicitement le nouvel élément
        memset(&transitions[index], 0, sizeof(File::ProcedureTransition));
        // Fin de modification
    }

    File::ProcedureTransition& transition = transitions[index];
    transition.mProcedureId = procedureId;
    transition.mLegSequenceId = legSequenceId;

    return true;
}


// =====================================================================================================
// PARSING EN BINAIRE DE LA SECTION [APPROACHTRANSITIONS] - MÉTHODE ÉLÉMENTAIRE
//
bool FileConverter::parseApproachTransition(std::istringstream& iss,
                                            std::vector<File::ApproachTransition>& transitions) {
    size_t index;
    uint32 approachId, legSequenceId;

    if (!(iss >> index >> approachId >> legSequenceId)) return false;

    if (index >= transitions.size()) {
        transitions.resize(index + 1);

        // Modification visant à diminuer les différences entre
        // binaires décodé /recodé : Initialiser explicitement le nouvel élément
        memset(&transitions[index], 0, sizeof(File::ApproachTransition));
        // Fin de modification
    }

    File::ApproachTransition& transition = transitions[index];
    transition.mApproachId = approachId;
    transition.mLegSequenceId = legSequenceId;

    return true;
}


// =====================================================================================================
// PARSING EN BINAIRE DE LA SECTION [RW SID et STAR TRANSITIONS] - MÉTHODE ÉLÉMENTAIRE
//
bool FileConverter::parseRunwayProcedureTransition(std::istringstream& iss,
                                                   std::vector<File::RunwayProcedureTransition>& transitions) {
    size_t index;
    uint32 runwayId, procedureId, engineOutProcedureId, legSequenceId;

    if (!(iss >> index >> runwayId >> procedureId >> engineOutProcedureId >> legSequenceId)) {
        return false;
    }

    if (index >= transitions.size()) {
        transitions.resize(index + 1);

        // Modification visant à diminuer les différences entre
        // binaires décodé /recodé : Initialiser explicitement le nouvel élément
        memset(&transitions[index], 0, sizeof(File::RunwayProcedureTransition));
        // Fin de modification
    }

    File::RunwayProcedureTransition& transition = transitions[index];
    transition.mRunwayId = runwayId;
    transition.mProcedureId = procedureId;
    transition.mEngineOutProcedureId = engineOutProcedureId;
    transition.mLegSequenceId = legSequenceId;

    return true;
}


// =====================================================================================================
// PARSING EN BINAIRE DE LA SECTION [AIRWAY] - MÉTHODE ÉLÉMENTAIRE
//
bool FileConverter::parseAirway(std::istringstream& iss, std::vector<File::Airway>& airways) {
    size_t index;
    std::string ident;

    if (!(iss >> index)) return false;

    if (!parseQuotedString(iss, ident)) return false;

    if (index >= airways.size()) {
        airways.resize(index + 1);

        // Modification visant à diminuer les différences entre
        // binaires décodé /recodé : Initialiser explicitement le nouvel élément
        memset(&airways[index], 0, sizeof(File::Airway));
        // Fin de modification
    }

    File::Airway& airway = airways[index];
    safeStrCopy(airway.mIdent, ident, sizeof(airway.mIdent));

    return true;
}


// =====================================================================================================
// PARSING EN BINAIRE DE LA SECTION [AIRWAYSEGMENTS] - MÉTHODE ÉLÉMENTAIRE
//
bool FileConverter::parseAirwaySegment(std::istringstream& iss, std::vector<File::AirwaySegment>& segments) {
    size_t index;
    uint32 airwayId;

    if (!(iss >> index >> airwayId)) return false;

    if (index >= segments.size()) {
        segments.resize(index + 1);

        // Modification visant à diminuer les différences entre
        // binaires décodé /recodé : Initialiser explicitement le nouvel élément
        memset(&segments[index], 0, sizeof(File::AirwaySegment));
        // Fin de modification
    }

    segments[index].mAirwayId = airwayId;
    return true;
}


// =====================================================================================================
// PARSING EN BINAIRE DE LA SECTION [AIRWAYSEGMENTLEG] - MÉTHODE ÉLÉMENTAIRE
//
bool FileConverter::parseAirwaySegmentLeg(std::istringstream& iss, std::vector<File::AirwaySegmentLeg>& segmentLegs) {
    size_t index;
    uint32 airwaySegmentId, pointId;

    if (!(iss >> index >> airwaySegmentId >> pointId)) return false;

    if (index >= segmentLegs.size()) {
        segmentLegs.resize(index + 1);

        // Modification visant à diminuer les différences entre
        // binaires décodé /recodé : Initialiser explicitement le nouvel élément
        memset(&segmentLegs[index], 0, sizeof(File::AirwaySegmentLeg));
        // Fin de modification
    }

    File::AirwaySegmentLeg& segmentLeg = segmentLegs[index];
    segmentLeg.mAirwaySegmentId = airwaySegmentId;
    segmentLeg.mPointId = pointId;

    return true;
}


// =====================================================================================================
// PARSING EN BINAIRE DE LA SECTION [ROUTE] - MÉTHODE ÉLÉMENTAIRE
//
bool FileConverter::parseRoute(std::istringstream& iss, std::vector<File::Route>& routes) {
    size_t index;
    std::string ident, company;
    uint32 departureAirportId, departureRunwayId, departureId, departureTransitionId;
    uint32 arrivalId, arrivalTransitionId, approachId, approachTransitionId;
    uint32 arrivalAirportId, alternateRouteId, costIndex, cruiseLevel;

    if (!(iss >> index)) return false;

    if (!parseQuotedString(iss, ident) || !parseQuotedString(iss, company)) return false;

    if (!(iss >> departureAirportId >> departureRunwayId >> departureId >> departureTransitionId
          >> arrivalId >> arrivalTransitionId >> approachId >> approachTransitionId
          >> arrivalAirportId >> alternateRouteId >> costIndex >> cruiseLevel)) {
        return false;
    }

    if (index >= routes.size()) {
        routes.resize(index + 1);

        // Modification visant à diminuer les différences entre
        // binaires décodé /recodé : Initialiser explicitement le nouvel élément
        memset(&routes[index], 0, sizeof(File::Route));
        // Fin de modification
    }

    File::Route& route = routes[index];
    safeStrCopy(route.mIdent, ident, sizeof(route.mIdent));
    safeStrCopy(route.mCompany, company, sizeof(route.mCompany));
    route.mDepartureAirportId = departureAirportId;
    route.mDepartureRunwayId = departureRunwayId;
    route.mDepartureId = departureId;
    route.mDepartureTransitionId = departureTransitionId;
    route.mArrivalId = arrivalId;
    route.mArrivalTransitionId = arrivalTransitionId;
    route.mApproachId = approachId;
    route.mApproachTransitionId = approachTransitionId;
    route.mArrivalAirportId = arrivalAirportId;
    route.mAlternateRouteId = alternateRouteId;
    route.mCostIndex = costIndex;
    route.mCruiseLevel = cruiseLevel;

    return true;
}


// =====================================================================================================
// PARSING EN BINAIRE DE LA SECTION [ROUTESEGMENTS] - MÉTHODE ÉLÉMENTAIRE
//
bool FileConverter::parseRouteSegment(std::istringstream& iss, std::vector<File::RouteSegment>& routeSegments) {
    size_t index;
    uint32 routeId, airwayId, pointId;

    if (!(iss >> index >> routeId >> airwayId >> pointId)) return false;

    if (index >= routeSegments.size()) {
        routeSegments.resize(index + 1);

        // Modification visant à diminuer les différences entre
        // binaires décodé /recodé : Initialiser explicitement le nouvel élément
        memset(&routeSegments[index], 0, sizeof(File::RouteSegment));
        // Fin de modification
    }

    File::RouteSegment& routeSegment = routeSegments[index];
    routeSegment.mRouteId = routeId;
    routeSegment.mAirwayId = airwayId;
    routeSegment.mPointId = pointId;

    return true;
}


// =====================================================================================================
// MÉTHODE UTILITAIRE POUR PARSER EN BINAIRE LES CHAINES ENTRE GUILLEMETS
//
bool FileConverter::parseQuotedString(std::istringstream& iss, std::string& result) {
    std::string token;
    if (!(iss >> token)) return false;

    if (token.size() < 2 || token.front() != '"' || token.back() != '"') {
        return false;
    }

    result = token.substr(1, token.size() - 2);
    return true;
}


