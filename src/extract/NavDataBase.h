#ifndef NAVDATABASE_H
#define NAVDATABASE_H

#include <QString>
#include <QStringList>
#include <QByteArray>
#include <vector>
//#include <cstdint>

namespace navstud::extract {

// ---------------------------------------------------------------------------
// Cœur d'analyse et d'extraction des données du fichier nav1.txt
// (format "NavData Text Format" utilisé par le FlightFactor B777).
//
// Les structures AIRWAY / AIRWAYSEGMENT / AIRWAYSEGMENTLEG / ROUTE /
// ROUTESEGMENT sont ignorées.
// ---------------------------------------------------------------------------
class NavDataBase
{
public:
    struct AirportInfo
    {
        QString ident;            // Code ICAO en clair (p.ex. "KJFK")
        qint32  airportIdx = -1;  // Index dans la structure AIRPORT
        qint32  pointIdx = -1;    // Index dans la structure POINT
        double  lat = 0.0;
        double  lon = 0.0;
        double  elevationM = 0.0;
    };

    struct ExtractStats
    {
        int config = 0;
        int points = 0;
        int waypoints = 0;
        int navaids = 0;
        int airports = 0;
        int runways = 0;
        int legSequences = 0;
        int legs = 0;
        int departures = 0;
        int arrivals = 0;
        int approaches = 0;
        int depTransitions = 0;
        int arrTransitions = 0;
        int appTransitions = 0;
        int rwyDepTransitions = 0;
        int rwyArrTransitions = 0;
    };

    // -----------------------------------------------------------------------
    // Sélection des enregistrements rattachés à un aéroport, produite par la
    // traversée de dépendances de selectAirport(). Chaque vecteur est indexé
    // par l'index D'ORIGINE de l'enregistrement dans sa section ; une valeur
    // non nulle signifie « enregistrement rattaché à l'aéroport ».
    // -----------------------------------------------------------------------
    struct AirportSelection
    {
        std::vector<char> points;
        std::vector<char> waypoints;
        std::vector<char> navaids;
        std::vector<char> airports;
        std::vector<char> runways;
        std::vector<char> legSequences;
        std::vector<char> legs;
        std::vector<char> departures;
        std::vector<char> arrivals;
        std::vector<char> approaches;
        std::vector<char> departureTransitions;
        std::vector<char> arrivalTransitions;
        std::vector<char> approachTransitions;
        std::vector<char> runwayDepartureTransitions;
        std::vector<char> runwayArrivalTransitions;
    };

    NavDataBase() = default;

    // Charge et analyse le fichier nav1.txt complet.
    bool load(const QString &path, QString *error = nullptr);

    // Calcule l'ensemble des enregistrements rattachés directement ou
    // indirectement à l'aéroport "icao" (même traversée que extractAirport()).
    // Sert aussi bien à l'extraction qu'à la suppression/réindexation.
    bool selectAirport(const QString &icao, AirportSelection *out,
                       QString *error = nullptr) const;

    bool isLoaded() const { return mLoaded; }
    QString source() const { return mSourcePath; }

    // Liste des aéroports (une entrée par aéroport du fichier).
    const std::vector<AirportInfo> &airports() const { return mAirports; }

    // Les identifiants triés (pour liste déroulante / complétion).
    QStringList airportIdents() const;

    // Extrait toutes les données (directes ou indirectes) rattachées à
    // l'aéroport de code "icao" vers le fichier "outPath". Si selection n'est
    // pas nul, y recopie la sélection calculée (utile pour la suppression
    // ultérieure des mêmes enregistrements dans le fichier mondial).
    bool extractAirport(const QString &icao, const QString &outPath,
                        QString *error = nullptr,
                        ExtractStats *stats = nullptr,
                        AirportSelection *selection = nullptr) const;

private:
    enum Section {
        SConfig, SPoints, SWaypoints, SNavaids, SAirports, SRunways,
        SLegSequences, SLegs, SDepartures, SArrivals, SApproaches,
        SDepartureTransitions, SArrivalTransitions, SApproachTransitions,
        SRunwayDepartureTransitions, SRunwayArrivalTransitions,
        SAirways, SAirwaySegments, SAirwaySegmentLegs, SRoutes, SRoutesegments,
        SUnknown
    };

    void clear();
    void processRecord(int section, qint64 lineOffset, int lineLen);
    void buildAirportList();
    QByteArray lineAt(qint64 offset) const;

    QString mSourcePath;
    QByteArray mData;                 // contenu complet du fichier source
    bool mLoaded = false;

    // Tableaux indexés par l'index de l'enregistrement
    std::vector<qint64> mConfigOff;

    std::vector<QByteArray> mPointIdent;
    std::vector<double> mPointLat;
    std::vector<double> mPointLon;
    std::vector<qint64> mPointOff;

    std::vector<qint32> mWaypointPoint;
    std::vector<qint64> mWaypointOff;

    std::vector<qint32> mNavaidPoint;
    std::vector<qint32> mNavaidRunway;
    std::vector<qint64> mNavaidOff;

    std::vector<qint32> mAirportPoint;
    std::vector<double> mAirportElev;
    std::vector<qint64> mAirportOff;

    std::vector<qint32> mRunwayAirport;
    std::vector<qint32> mRunwayPoint;
    std::vector<qint64> mRunwayOff;

    std::vector<qint64> mLegSeqOff;

    std::vector<qint32> mLegLegSeq;
    std::vector<qint32> mLegPoint;
    std::vector<qint32> mLegNavaid;
    std::vector<qint64> mLegOff;

    std::vector<qint32> mDepAirport;
    std::vector<qint32> mDepLegSeq;
    std::vector<qint64> mDepOff;

    std::vector<qint32> mArrAirport;
    std::vector<qint32> mArrLegSeq;
    std::vector<qint64> mArrOff;

    std::vector<qint32> mAppRunway;
    std::vector<qint32> mAppLegSeq;
    std::vector<qint64> mAppOff;

    std::vector<qint32> mDepTProc;
    std::vector<qint32> mDepTLegSeq;
    std::vector<qint64> mDepTOff;

    std::vector<qint32> mArrTProc;
    std::vector<qint32> mArrTLegSeq;
    std::vector<qint64> mArrTOff;

    std::vector<qint32> mAppTApproach;
    std::vector<qint32> mAppTLegSeq;
    std::vector<qint64> mAppTOff;

    std::vector<qint32> mRwyDepRunway;
    std::vector<qint32> mRwyDepProc;
    std::vector<qint32> mRwyDepEngineOut;
    std::vector<qint32> mRwyDepLegSeq;
    std::vector<qint64> mRwyDepOff;

    std::vector<qint32> mRwyArrRunway;
    std::vector<qint32> mRwyArrProc;
    std::vector<qint32> mRwyArrEngineOut;
    std::vector<qint32> mRwyArrLegSeq;
    std::vector<qint64> mRwyArrOff;

    std::vector<AirportInfo> mAirports;

};

} // namespace navstud::extract

#endif // NAVDATABASE_H