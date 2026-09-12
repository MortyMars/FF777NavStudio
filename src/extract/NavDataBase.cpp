#include "NavDataBase.h"

#include <QFile>
#include <QFileInfo>
#include <QTextStream>
//#include <algorithm>
#include <cctype>

namespace navstud::extract {

namespace {

struct Token { const char *p; int len; };

enum { MAX_TOKENS = 32 };

// Découpe une ligne en champs (séparés par des espaces), en retirant les
// guillemets qui entourent les identifiants ("KJFK" -> KJFK).
inline int splitLine(const char *line, int len, Token *out, int max)
{
    int n = 0;
    const char *end = line + len;
    const char *p = line;
    while (p < end && n < max) {
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\r'))
            ++p;
        if (p >= end)
            break;
        if (*p == '"') {
            ++p;
            const char *s = p;
            while (p < end && *p != '"')
                ++p;
            out[n++] = Token{s, static_cast<int>(p - s)};
            if (p < end)
                ++p;
        } else {
            const char *s = p;
            while (p < end && *p != ' ' && *p != '\t' && *p != '\r')
                ++p;
            out[n++] = Token{s, static_cast<int>(p - s)};
        }
    }
    return n;
}

inline int tokenToInt(const Token &t)
{
    return QByteArray(t.p, t.len).toInt();
}

inline double tokenToDouble(const Token &t)
{
    return QByteArray(t.p, t.len).toDouble();
}

inline void setError(QString *error, const QString &msg)
{
    if (error)
        *error = msg;
}

template <typename V>
inline void ensureVec(V &v, int index, typename V::value_type def)
{
    if (index >= static_cast<int>(v.size()))
        v.resize(index + 1, def);
}

} // namespace

// ---------------------------------------------------------------------------

void NavDataBase::clear()
{
    mData.clear();
    mSourcePath.clear();
    mLoaded = false;

    mConfigOff.clear();
    mPointIdent.clear(); mPointLat.clear(); mPointLon.clear(); mPointOff.clear();
    mWaypointPoint.clear(); mWaypointOff.clear();
    mNavaidPoint.clear(); mNavaidRunway.clear(); mNavaidOff.clear();
    mAirportPoint.clear(); mAirportElev.clear(); mAirportOff.clear();
    mRunwayAirport.clear(); mRunwayPoint.clear(); mRunwayOff.clear();
    mLegSeqOff.clear();
    mLegLegSeq.clear(); mLegPoint.clear(); mLegNavaid.clear(); mLegOff.clear();
    mDepAirport.clear(); mDepLegSeq.clear(); mDepOff.clear();
    mArrAirport.clear(); mArrLegSeq.clear(); mArrOff.clear();
    mAppRunway.clear(); mAppLegSeq.clear(); mAppOff.clear();
    mDepTProc.clear(); mDepTLegSeq.clear(); mDepTOff.clear();
    mArrTProc.clear(); mArrTLegSeq.clear(); mArrTOff.clear();
    mAppTApproach.clear(); mAppTLegSeq.clear(); mAppTOff.clear();
    mRwyDepRunway.clear(); mRwyDepProc.clear(); mRwyDepEngineOut.clear();
    mRwyDepLegSeq.clear(); mRwyDepOff.clear();
    mRwyArrRunway.clear(); mRwyArrProc.clear(); mRwyArrEngineOut.clear();
    mRwyArrLegSeq.clear(); mRwyArrOff.clear();
    mAirports.clear();
}

QByteArray NavDataBase::lineAt(qint64 offset) const
{
    const char *base = mData.constData();
    const qint64 size = mData.size();
    if (offset < 0 || offset > size)
        return QByteArray();
    const char *s = base + offset;
    const char *e = s;
    while (e < base + size && *e != '\n')
        ++e;
    return QByteArray(s, static_cast<int>(e - s));
}

void NavDataBase::processRecord(int section, qint64 lineOffset, int lineLen)
{
    Token tok[MAX_TOKENS];
    const int n = splitLine(mData.constData() + lineOffset, lineLen, tok, MAX_TOKENS);
    if (n < 2)
        return;

    const int idx = tokenToInt(tok[1]);

    switch (section) {
    case SConfig:
        mConfigOff.push_back(lineOffset);
        break;

    case SPoints:
        if (n >= 5) {
            ensureVec(mPointIdent, idx, QByteArray());
            mPointIdent[idx] = QByteArray(tok[2].p, tok[2].len);
            ensureVec(mPointLat, idx, 0.0);
            mPointLat[idx] = tokenToDouble(tok[3]);
            ensureVec(mPointLon, idx, 0.0);
            mPointLon[idx] = tokenToDouble(tok[4]);
            ensureVec(mPointOff, idx, static_cast<qint64>(-1));
            mPointOff[idx] = lineOffset;
        }
        break;

    case SWaypoints:
        if (n >= 3) {
            ensureVec(mWaypointPoint, idx, static_cast<qint32>(-1));
            mWaypointPoint[idx] = tokenToInt(tok[2]);
            ensureVec(mWaypointOff, idx, static_cast<qint64>(-1));
            mWaypointOff[idx] = lineOffset;
        }
        break;

    case SNavaids:
        if (n >= 13) {
            ensureVec(mNavaidPoint, idx, static_cast<qint32>(-1));
            mNavaidPoint[idx] = tokenToInt(tok[3]);
            ensureVec(mNavaidRunway, idx, static_cast<qint32>(-1));
            mNavaidRunway[idx] = tokenToInt(tok[12]);
            ensureVec(mNavaidOff, idx, static_cast<qint64>(-1));
            mNavaidOff[idx] = lineOffset;
        }
        break;

    case SAirports:
        if (n >= 4) {
            ensureVec(mAirportPoint, idx, static_cast<qint32>(-1));
            mAirportPoint[idx] = tokenToInt(tok[2]);
            ensureVec(mAirportElev, idx, 0.0);
            mAirportElev[idx] = tokenToDouble(tok[3]);
            ensureVec(mAirportOff, idx, static_cast<qint64>(-1));
            mAirportOff[idx] = lineOffset;
        }
        break;

    case SRunways:
        if (n >= 4) {
            ensureVec(mRunwayAirport, idx, static_cast<qint32>(-1));
            mRunwayAirport[idx] = tokenToInt(tok[2]);
            ensureVec(mRunwayPoint, idx, static_cast<qint32>(-1));
            mRunwayPoint[idx] = tokenToInt(tok[3]);
            ensureVec(mRunwayOff, idx, static_cast<qint64>(-1));
            mRunwayOff[idx] = lineOffset;
        }
        break;

    case SLegSequences:
        ensureVec(mLegSeqOff, idx, static_cast<qint64>(-1));
        mLegSeqOff[idx] = lineOffset;
        break;

    case SLegs:
        if (n >= 17) {
            ensureVec(mLegLegSeq, idx, static_cast<qint32>(-1));
            mLegLegSeq[idx] = tokenToInt(tok[3]);
            ensureVec(mLegPoint, idx, static_cast<qint32>(-1));
            mLegPoint[idx] = tokenToInt(tok[4]);
            ensureVec(mLegNavaid, idx, static_cast<qint32>(-1));
            mLegNavaid[idx] = tokenToInt(tok[8]);
            ensureVec(mLegOff, idx, static_cast<qint64>(-1));
            mLegOff[idx] = lineOffset;
        }
        break;

    case SDepartures:
        if (n >= 4) {
            ensureVec(mDepAirport, idx, static_cast<qint32>(-1));
            mDepAirport[idx] = tokenToInt(tok[2]);
            ensureVec(mDepLegSeq, idx, static_cast<qint32>(-1));
            mDepLegSeq[idx] = tokenToInt(tok[3]);
            ensureVec(mDepOff, idx, static_cast<qint64>(-1));
            mDepOff[idx] = lineOffset;
        }
        break;

    case SArrivals:
        if (n >= 4) {
            ensureVec(mArrAirport, idx, static_cast<qint32>(-1));
            mArrAirport[idx] = tokenToInt(tok[2]);
            ensureVec(mArrLegSeq, idx, static_cast<qint32>(-1));
            mArrLegSeq[idx] = tokenToInt(tok[3]);
            ensureVec(mArrOff, idx, static_cast<qint64>(-1));
            mArrOff[idx] = lineOffset;
        }
        break;

    case SApproaches:
        if (n >= 4) {
            ensureVec(mAppRunway, idx, static_cast<qint32>(-1));
            mAppRunway[idx] = tokenToInt(tok[2]);
            ensureVec(mAppLegSeq, idx, static_cast<qint32>(-1));
            mAppLegSeq[idx] = tokenToInt(tok[3]);
            ensureVec(mAppOff, idx, static_cast<qint64>(-1));
            mAppOff[idx] = lineOffset;
        }
        break;

    case SDepartureTransitions:
        if (n >= 4) {
            ensureVec(mDepTProc, idx, static_cast<qint32>(-1));
            mDepTProc[idx] = tokenToInt(tok[2]);
            ensureVec(mDepTLegSeq, idx, static_cast<qint32>(-1));
            mDepTLegSeq[idx] = tokenToInt(tok[3]);
            ensureVec(mDepTOff, idx, static_cast<qint64>(-1));
            mDepTOff[idx] = lineOffset;
        }
        break;

    case SArrivalTransitions:
        if (n >= 4) {
            ensureVec(mArrTProc, idx, static_cast<qint32>(-1));
            mArrTProc[idx] = tokenToInt(tok[2]);
            ensureVec(mArrTLegSeq, idx, static_cast<qint32>(-1));
            mArrTLegSeq[idx] = tokenToInt(tok[3]);
            ensureVec(mArrTOff, idx, static_cast<qint64>(-1));
            mArrTOff[idx] = lineOffset;
        }
        break;

    case SApproachTransitions:
        if (n >= 4) {
            ensureVec(mAppTApproach, idx, static_cast<qint32>(-1));
            mAppTApproach[idx] = tokenToInt(tok[2]);
            ensureVec(mAppTLegSeq, idx, static_cast<qint32>(-1));
            mAppTLegSeq[idx] = tokenToInt(tok[3]);
            ensureVec(mAppTOff, idx, static_cast<qint64>(-1));
            mAppTOff[idx] = lineOffset;
        }
        break;

    case SRunwayDepartureTransitions:
        if (n >= 6) {
            ensureVec(mRwyDepRunway, idx, static_cast<qint32>(-1));
            mRwyDepRunway[idx] = tokenToInt(tok[2]);
            ensureVec(mRwyDepProc, idx, static_cast<qint32>(-1));
            mRwyDepProc[idx] = tokenToInt(tok[3]);
            ensureVec(mRwyDepEngineOut, idx, static_cast<qint32>(-1));
            mRwyDepEngineOut[idx] = tokenToInt(tok[4]);
            ensureVec(mRwyDepLegSeq, idx, static_cast<qint32>(-1));
            mRwyDepLegSeq[idx] = tokenToInt(tok[5]);
            ensureVec(mRwyDepOff, idx, static_cast<qint64>(-1));
            mRwyDepOff[idx] = lineOffset;
        }
        break;

    case SRunwayArrivalTransitions:
        if (n >= 6) {
            ensureVec(mRwyArrRunway, idx, static_cast<qint32>(-1));
            mRwyArrRunway[idx] = tokenToInt(tok[2]);
            ensureVec(mRwyArrProc, idx, static_cast<qint32>(-1));
            mRwyArrProc[idx] = tokenToInt(tok[3]);
            ensureVec(mRwyArrEngineOut, idx, static_cast<qint32>(-1));
            mRwyArrEngineOut[idx] = tokenToInt(tok[4]);
            ensureVec(mRwyArrLegSeq, idx, static_cast<qint32>(-1));
            mRwyArrLegSeq[idx] = tokenToInt(tok[5]);
            ensureVec(mRwyArrOff, idx, static_cast<qint64>(-1));
            mRwyArrOff[idx] = lineOffset;
        }
        break;

    default:
        break;
    }
}

void NavDataBase::buildAirportList()
{
    mAirports.clear();
    const int n = static_cast<int>(mAirportPoint.size());
    mAirports.reserve(n);
    for (int ai = 0; ai < n; ++ai) {
        AirportInfo info;
        info.airportIdx = ai;
        const int pi = mAirportPoint[ai];
        info.pointIdx = pi;
        if (pi >= 0 && pi < static_cast<int>(mPointIdent.size())) {
            info.ident = QString::fromLatin1(mPointIdent[pi]);
            info.lat = mPointLat[pi];
            info.lon = mPointLon[pi];
        }
        if (ai < static_cast<int>(mAirportElev.size()))
            info.elevationM = mAirportElev[ai];
        mAirports.push_back(info);
    }
}

QStringList NavDataBase::airportIdents() const
{
    QStringList ids;
    ids.reserve(mAirports.size());
    for (const AirportInfo &a : mAirports)
        if (!a.ident.isEmpty())
            ids.append(a.ident);
    ids.removeDuplicates();
    ids.sort(Qt::CaseInsensitive);
    return ids;
}

bool NavDataBase::load(const QString &path, QString *error)
{
    clear();

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(error, QStringLiteral("Impossible d'ouvrir le fichier : %1").arg(file.errorString()));
        return false;
    }
    mSourcePath = path;
    mData = file.readAll();
    file.close();

    if (mData.isEmpty()) {
        setError(error, QStringLiteral("Le fichier est vide."));
        return false;
    }

    const char *base = mData.constData();
    const qint64 size = mData.size();
    qint64 off = 0;

    // Parcours séquentiel du fichier : sections [NAME], "# Count: N", records.
    while (off < size) {
        qint64 e = off;
        while (e < size && base[e] != '\n')
            ++e;
        const char *s = base + off;
        int len = static_cast<int>(e - off);
        while (len > 0 && (s[0] == ' ' || s[0] == '\t' || s[0] == '\r')) { ++s; --len; }
        while (len > 0 && (s[len - 1] == '\r')) --len;

        if (len == 0 || s[0] == '#') {
            off = e + 1;
            continue;
        }

        if (s[0] == '[' && len >= 3) {
            const QByteArray name(s + 1, len - 2);
            int section = SUnknown;
            if (name == "CONFIG") section = SConfig;
            else if (name == "POINTS") section = SPoints;
            else if (name == "WAYPOINTS") section = SWaypoints;
            else if (name == "NAVAIDS") section = SNavaids;
            else if (name == "AIRPORTS") section = SAirports;
            else if (name == "RUNWAYS") section = SRunways;
            else if (name == "LEGSEQUENCES") section = SLegSequences;
            else if (name == "LEGS") section = SLegs;
            else if (name == "DEPARTURES") section = SDepartures;
            else if (name == "ARRIVALS") section = SArrivals;
            else if (name == "APPROACHES") section = SApproaches;
            else if (name == "DEPARTURETRANSITIONS") section = SDepartureTransitions;
            else if (name == "ARRIVALTRANSITIONS") section = SArrivalTransitions;
            else if (name == "APPROACHTRANSITIONS") section = SApproachTransitions;
            else if (name == "RUNWAYDEPARTURETRANSITIONS") section = SRunwayDepartureTransitions;
            else if (name == "RUNWAYARRIVALTRANSITIONS") section = SRunwayArrivalTransitions;
            else if (name == "AIRWAYS") section = SAirways;
            else if (name == "AIRWAYSEGMENTS") section = SAirwaySegments;
            else if (name == "AIRWAYSEGMENTLEGS") section = SAirwaySegmentLegs;
            else if (name == "ROUTES") section = SRoutes;
            else if (name == "ROUTESEGMENTS") section = SRoutesegments;

            // Ligne suivante : "# Count: N"
            off = e + 1;
            qint64 ce = off;
            while (ce < size && base[ce] != '\n')
                ++ce;
            int count = 0;
            {
                const QByteArray countLine(base + off, static_cast<int>(ce - off));
                const int colon = countLine.indexOf(':');
                if (colon >= 0)
                    count = countLine.mid(colon + 1).trimmed().toInt();
            }
            off = ce + 1;

            for (int i = 0; i < count; ++i) {
                if (off >= size)
                    break;
                const qint64 recOff = off;
                qint64 re = off;
                while (re < size && base[re] != '\n')
                    ++re;
                processRecord(section, recOff, static_cast<int>(re - recOff));
                off = re + 1;
            }
        } else {
            // Ligne inattendue : on l'ignore.
            off = e + 1;
        }
    }

    buildAirportList();
    mLoaded = true;
    return true;
}

bool NavDataBase::selectAirport(const QString &icao, AirportSelection *out,
                                QString *error) const
{
    if (!mLoaded) {
        setError(error, QStringLiteral("Aucun fichier n'a été chargé."));
        return false;
    }

    const QString query = icao.trimmed().toUpper();
    if (query.isEmpty()) {
        setError(error, QStringLiteral("Code aéroport vide."));
        return false;
    }

    // 1) Recherche de l'aéroport par son identifiant en clair.
    std::vector<int> airportMatches;
    for (const AirportInfo &a : mAirports)
        if (a.ident.compare(query, Qt::CaseInsensitive) == 0)
            airportMatches.push_back(a.airportIdx);
    if (airportMatches.empty()) {
        setError(error, QStringLiteral("Aucun aéroport « %1 » n'a été trouvé dans le fichier.").arg(query));
        return false;
    }

    AirportSelection sel;

    const int nPoints = static_cast<int>(mPointIdent.size());
    const int nWp = static_cast<int>(mWaypointPoint.size());
    const int nNav = static_cast<int>(mNavaidPoint.size());
    const int nAp = static_cast<int>(mAirportPoint.size());
    const int nRw = static_cast<int>(mRunwayAirport.size());
    const int nLsq = static_cast<int>(mLegSeqOff.size());
    const int nLeg = static_cast<int>(mLegOff.size());
    const int nDep = static_cast<int>(mDepOff.size());
    const int nArr = static_cast<int>(mArrOff.size());
    const int nApp = static_cast<int>(mAppOff.size());
    const int nDepT = static_cast<int>(mDepTOff.size());
    const int nArrT = static_cast<int>(mArrTOff.size());
    const int nAppT = static_cast<int>(mAppTOff.size());
    const int nRwDepT = static_cast<int>(mRwyDepOff.size());
    const int nRwArrT = static_cast<int>(mRwyArrOff.size());

    sel.points.assign(nPoints, 0);
    sel.waypoints.assign(nWp, 0);
    sel.navaids.assign(nNav, 0);
    sel.airports.assign(nAp, 0);
    sel.runways.assign(nRw, 0);
    sel.legSequences.assign(nLsq, 0);
    sel.legs.assign(nLeg, 0);
    sel.departures.assign(nDep, 0);
    sel.arrivals.assign(nArr, 0);
    sel.approaches.assign(nApp, 0);
    sel.departureTransitions.assign(nDepT, 0);
    sel.arrivalTransitions.assign(nArrT, 0);
    sel.approachTransitions.assign(nAppT, 0);
    sel.runwayDepartureTransitions.assign(nRwDepT, 0);
    sel.runwayArrivalTransitions.assign(nRwArrT, 0);

    std::vector<char> &hasPoint = sel.points;
    std::vector<char> &hasWp = sel.waypoints;
    std::vector<char> &hasNav = sel.navaids;
    std::vector<char> &hasAp = sel.airports;
    std::vector<char> &hasRw = sel.runways;
    std::vector<char> &hasLsq = sel.legSequences;
    std::vector<char> &hasLeg = sel.legs;
    std::vector<char> &hasDep = sel.departures;
    std::vector<char> &hasArr = sel.arrivals;
    std::vector<char> &hasApp = sel.approaches;
    std::vector<char> &hasDepT = sel.departureTransitions;
    std::vector<char> &hasArrT = sel.arrivalTransitions;
    std::vector<char> &hasAppT = sel.approachTransitions;
    std::vector<char> &hasRwDepT = sel.runwayDepartureTransitions;
    std::vector<char> &hasRwArrT = sel.runwayArrivalTransitions;

    std::vector<char> isMatch(nAp, 0);
    for (int m : airportMatches)
        isMatch[m] = 1;

    auto addPoint = [&](qint32 p) {
        if (p >= 0 && p < nPoints) hasPoint[p] = 1;
    };
    auto addLegSeq = [&](qint32 l) {
        if (l >= 0 && l < nLsq) hasLsq[l] = 1;
    };
    auto addDep = [&](qint32 p) {
        if (p >= 0 && p < nDep && !hasDep[p]) {
            hasDep[p] = 1;
            if (p < static_cast<int>(mDepLegSeq.size())) addLegSeq(mDepLegSeq[p]);
        }
    };
    auto addArr = [&](qint32 p) {
        if (p >= 0 && p < nArr && !hasArr[p]) {
            hasArr[p] = 1;
            if (p < static_cast<int>(mArrLegSeq.size())) addLegSeq(mArrLegSeq[p]);
        }
    };
    auto addApp = [&](qint32 p) {
        if (p >= 0 && p < nApp && !hasApp[p]) {
            hasApp[p] = 1;
            if (p < static_cast<int>(mAppLegSeq.size())) addLegSeq(mAppLegSeq[p]);
        }
    };

    // 2) Aéroports ciblés + leurs points.
    for (int m : airportMatches) {
        hasAp[m] = 1;
        if (m < static_cast<int>(mAirportPoint.size())) addPoint(mAirportPoint[m]);
    }

    // 3) Pistes de ces aéroports + points de seuil.
    for (int r = 0; r < nRw; ++r) {
        const qint32 ai = mRunwayAirport[r];
        if (ai >= 0 && ai < nAp && isMatch[ai]) {
            hasRw[r] = 1;
            addPoint(mRunwayPoint[r]);
        }
    }

    // 4) Départs / arrivées rattachés à l'aéroport.
    for (int p = 0; p < nDep; ++p)
        if (mDepAirport[p] >= 0 && mDepAirport[p] < nAp && isMatch[mDepAirport[p]])
            addDep(p);
    for (int p = 0; p < nArr; ++p)
        if (mArrAirport[p] >= 0 && mArrAirport[p] < nAp && isMatch[mArrAirport[p]])
            addArr(p);

    // 5) Approches rattachées aux pistes.
    for (int a = 0; a < nApp; ++a) {
        const qint32 r = mAppRunway[a];
        if (r >= 0 && r < nRw && hasRw[r])
            addApp(a);
    }

    // 6) Transitions de procédures.
    for (int t = 0; t < nDepT; ++t) {
        const qint32 p = mDepTProc[t];
        if (p >= 0 && p < nDep && hasDep[p]) {
            hasDepT[t] = 1;
            addLegSeq(mDepTLegSeq[t]);
        }
    }
    for (int t = 0; t < nArrT; ++t) {
        const qint32 p = mArrTProc[t];
        if (p >= 0 && p < nArr && hasArr[p]) {
            hasArrT[t] = 1;
            addLegSeq(mArrTLegSeq[t]);
        }
    }
    for (int t = 0; t < nAppT; ++t) {
        const qint32 a = mAppTApproach[t];
        if (a >= 0 && a < nApp && hasApp[a]) {
            hasAppT[t] = 1;
            addLegSeq(mAppTLegSeq[t]);
        }
    }

    // 7) Transitions par piste (départs et arrivées) : procédures + engine-out.
    for (int t = 0; t < nRwDepT; ++t) {
        const qint32 r = mRwyDepRunway[t];
        if (r >= 0 && r < nRw && hasRw[r]) {
            hasRwDepT[t] = 1;
            addDep(mRwyDepProc[t]);
            addDep(mRwyDepEngineOut[t]);
            addLegSeq(mRwyDepLegSeq[t]);
        }
    }
    for (int t = 0; t < nRwArrT; ++t) {
        const qint32 r = mRwyArrRunway[t];
        if (r >= 0 && r < nRw && hasRw[r]) {
            hasRwArrT[t] = 1;
            addArr(mRwyArrProc[t]);
            addArr(mRwyArrEngineOut[t]);
            addLegSeq(mRwyArrLegSeq[t]);
        }
    }

    // 8) Legs des séquences retenues + leurs points / navaids.
    for (int l = 0; l < nLeg; ++l) {
        const qint32 ls = mLegLegSeq[l];
        if (ls >= 0 && ls < nLsq && hasLsq[ls]) {
            hasLeg[l] = 1;
            addPoint(mLegPoint[l]);
            const qint32 nav = mLegNavaid[l];
            if (nav >= 0 && nav < nNav)
                hasNav[nav] = 1;
        }
    }

    // 9) Navaids liés aux points utilisés ou aux pistes (ILS/LOC/GS/DME...).
    //    Puis leurs propres points (souvent des points de type LOC/GS).
    for (int v = 0; v < nNav; ++v)
        if (hasNav[v])
            addPoint(mNavaidPoint[v]);

    bool addedNew = true;
    while (addedNew) {
        addedNew = false;
        for (int v = 0; v < nNav; ++v) {
            if (hasNav[v])
                continue;
            const qint32 p = mNavaidPoint[v];
            const qint32 r = mNavaidRunway[v];
            const bool byPoint = p >= 0 && p < nPoints && hasPoint[p];
            const bool byRunway = r >= 0 && r < nRw && hasRw[r];
            if (byPoint || byRunway) {
                hasNav[v] = 1;
                addPoint(p);
                addedNew = true;
            }
        }
    }

    // 10) Waypoints correspondant aux points retenus.
    for (int w = 0; w < nWp; ++w) {
        const qint32 p = mWaypointPoint[w];
        if (p >= 0 && p < nPoints && hasPoint[p])
            hasWp[w] = 1;
    }

    if (out)
        *out = std::move(sel);
    return true;
}

bool NavDataBase::extractAirport(const QString &icao, const QString &outPath,
                                 QString *error, ExtractStats *stats,
                                 AirportSelection *selection) const
{
    // La sélection des enregistrements rattachés à l'aéroport est factorisée
    // dans selectAirport() : extractAirport() ne fait plus que l'écrire.
    AirportSelection sel;
    if (!selectAirport(icao, &sel, error))
        return false;

    if (selection)
        *selection = sel;

    ExtractStats st;
    st.airports = 0;
    for (char v : sel.airports)
        if (v) ++st.airports;

    // 11) Écriture du fichier extrait, sections dans l'ordre du format source.
    QFile out(outPath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        setError(error, QStringLiteral("Impossible de créer le fichier de sortie : %1").arg(out.errorString()));
        return false;
    }
    QTextStream ts(&out);

    ts << "# NavData Text Format\n\n";

    auto writeUnfiltered = [&](const char *name, const std::vector<qint64> &offs, int *count) {
        ts << name << "\n";
        *count = static_cast<int>(offs.size());
        ts << "# Count: " << *count << "\n";
        for (qint64 o : offs)
            if (o >= 0) ts << lineAt(o) << "\n";
        ts << "\n";
    };

    auto writeFiltered = [&](const char *name, const std::vector<qint64> &offs,
                             const std::vector<char> &inc, int *count) {
        ts << name << "\n";
        int c = 0;
        for (char v : inc)
            if (v) ++c;
        *count = c;
        ts << "# Count: " << c << "\n";
        for (int i = 0; i < static_cast<int>(offs.size()); ++i)
            if (i < static_cast<int>(inc.size()) && inc[i] && offs[i] >= 0)
                ts << lineAt(offs[i]) << "\n";
        ts << "\n";
    };

    writeUnfiltered("[CONFIG]", mConfigOff, &st.config);

    writeFiltered("[POINTS]", mPointOff, sel.points, &st.points);
    writeFiltered("[WAYPOINTS]", mWaypointOff, sel.waypoints, &st.waypoints);
    writeFiltered("[NAVAIDS]", mNavaidOff, sel.navaids, &st.navaids);
    writeFiltered("[AIRPORTS]", mAirportOff, sel.airports, &st.airports);
    writeFiltered("[RUNWAYS]", mRunwayOff, sel.runways, &st.runways);
    writeFiltered("[LEGSEQUENCES]", mLegSeqOff, sel.legSequences, &st.legSequences);
    writeFiltered("[LEGS]", mLegOff, sel.legs, &st.legs);
    writeFiltered("[DEPARTURES]", mDepOff, sel.departures, &st.departures);
    writeFiltered("[ARRIVALS]", mArrOff, sel.arrivals, &st.arrivals);
    writeFiltered("[APPROACHES]", mAppOff, sel.approaches, &st.approaches);
    writeFiltered("[DEPARTURETRANSITIONS]", mDepTOff, sel.departureTransitions, &st.depTransitions);
    writeFiltered("[ARRIVALTRANSITIONS]", mArrTOff, sel.arrivalTransitions, &st.arrTransitions);
    writeFiltered("[APPROACHTRANSITIONS]", mAppTOff, sel.approachTransitions, &st.appTransitions);
    writeFiltered("[RUNWAYDEPARTURETRANSITIONS]", mRwyDepOff, sel.runwayDepartureTransitions, &st.rwyDepTransitions);
    writeFiltered("[RUNWAYARRIVALTRANSITIONS]", mRwyArrOff, sel.runwayArrivalTransitions, &st.rwyArrTransitions);

    ts.flush();
    out.close();

    if (stats)
        *stats = st;
    return true;
}

} // namespace navstud::extract