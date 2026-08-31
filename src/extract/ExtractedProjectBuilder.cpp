#include "ExtractedProjectBuilder.h"

#include "TextParse.h" // reader::parse::tokenize / real / integer / unsignedInteger / unpackLegCode

#include <QFile>
#include <QHash>
#include <QSet>
#include <QTextStream>

namespace navstud::extract {

using namespace navstud::model;
using namespace navstud::userdata;

namespace {

constexpr double kFeet = 3.28084; // m -> ft (inverse du facteur de conversion du convertisseur)

void addUnique(QStringList& warnings, const QString& message)
{
    if (!warnings.contains(message))
        warnings << message;
}

int toIntToken(const QStringList& tk, int i)
{
    if (i < 0 || i >= tk.size())
        return -1;
    const auto v = reader::parse::integer(tk.at(i));
    return v ? *v : -1;
}

quint32 toUIntToken(const QStringList& tk, int i, bool* ok = nullptr)
{
    if (i < 0 || i >= tk.size()) {
        if (ok) *ok = false;
        return 0;
    }
    const auto v = reader::parse::unsignedInteger(tk.at(i));
    if (ok) *ok = v.has_value();
    return v ? *v : 0;
}

double toRealToken(const QStringList& tk, int i)
{
    if (i < 0 || i >= tk.size())
        return 0.0;
    const auto v = reader::parse::real(tk.at(i));
    return v ? *v : 0.0;
}

// Lignes brutes structurées, une table de hash par famille (clé = index du
// monde d'origine) — sert aussi de registre index -> ident pour résoudre les
// références par ident en clair.
struct RawPoint   { quint32 id; QString ident; double lat, lon, magVar, holdCourse, holdDist, holdTime; int holdSide; };
struct RawWaypoint{ quint32 id; int point; };
struct RawNavaid  { quint32 id; quint32 type; int point; int attached; double elev, decl; quint32 freq; QString cat; double course, angle; int runway; };
struct RawAirport { quint32 id; int point; double elev, limitSpeed, limitAlt, transAlt, transLevel; };
struct RawRunway  { quint32 id; int airport, point; double elev, gradient, course, length, displaced, stopway, crossing; };
struct RawLegSeq  { quint32 id; QString ident; quint32 type; double transitionMeters; };
struct RawLeg     { quint32 id; quint32 codeRaw; int legSeq, point; quint32 usage; double course, dist; int navaidPoint; double navaidCourse, navaidDist; double altMin, altMax, airSpeed; double path; int turn; double rnp; };
struct RawProc    { quint32 id; int airport, legSeq; };
struct RawApproach{ quint32 id; int runway, legSeq; double dh, mda; };
struct RawTrans   { quint32 id; int ref, legSeq; }; // ref = approach ou proc selon la section
struct RawRpt     { quint32 id; int runway, proc, engineOut, legSeq; };

struct RawData
{
    QHash<quint32, RawPoint>     points;
    QHash<quint32, RawWaypoint>  waypoints;
    QHash<quint32, RawNavaid>    navaids;
    QHash<quint32, RawAirport>   airports;
    QHash<quint32, RawRunway>    runways;
    QHash<quint32, RawLegSeq>    legSequences;
    QHash<quint32, RawLeg>       legs;
    QHash<quint32, RawProc>      sids;
    QHash<quint32, RawProc>      stars;
    QHash<quint32, RawApproach>  approaches;
    QHash<quint32, RawTrans>     appTrans;
    QHash<quint32, RawTrans>     sidTrans;
    QHash<quint32, RawTrans>     starTrans;
    QHash<quint32, RawRpt>       sidRpts;
    QHash<quint32, RawRpt>       starRpts;

    // registres index -> ident (pour la résolution des références)
    QHash<quint32, QString> pointIdentByIdx;
    QHash<quint32, int>     runwayPointByIdx;   // runway -> point (seuil)
    QHash<quint32, int>     airportPointByIdx;  // airport -> point
    QHash<quint32, QString> legSeqIdentByIdx;
};

RawData parseRawFile(const QString& filePath, QString* fatalError, QStringList* warnings)
{
    RawData data;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (fatalError)
            *fatalError = QStringLiteral("Impossible d'ouvrir le fichier extrait : %1").arg(file.errorString());
        return data;
    }
    QTextStream in(&file);
    // Qt6 : UTF-8 par défaut — aucun setCodec nécessaire.

    enum class Section {
        Unknown, Config, Points, Waypoints, Navaids, Airports, Runways,
        LegSequences, Legs, Departures, Arrivals, Approaches,
        DepartureTransitions, ArrivalTransitions, ApproachTransitions,
        RunwayDepartureTransitions, RunwayArrivalTransitions
    };
    const QHash<QString, Section> sectionByName = {
        { QStringLiteral("POINTS"),                   Section::Points },
        { QStringLiteral("WAYPOINTS"),                Section::Waypoints },
        { QStringLiteral("NAVAIDS"),                  Section::Navaids },
        { QStringLiteral("AIRPORTS"),                 Section::Airports },
        { QStringLiteral("RUNWAYS"),                  Section::Runways },
        { QStringLiteral("LEGSEQUENCES"),             Section::LegSequences },
        { QStringLiteral("LEGS"),                     Section::Legs },
        { QStringLiteral("DEPARTURES"),               Section::Departures },
        { QStringLiteral("ARRIVALS"),                 Section::Arrivals },
        { QStringLiteral("APPROACHES"),               Section::Approaches },
        { QStringLiteral("DEPARTURETRANSITIONS"),     Section::DepartureTransitions },
        { QStringLiteral("ARRIVALTRANSITIONS"),       Section::ArrivalTransitions },
        { QStringLiteral("APPROACHTRANSITIONS"),      Section::ApproachTransitions },
        { QStringLiteral("RUNWAYDEPARTURETRANSITIONS"), Section::RunwayDepartureTransitions },
        { QStringLiteral("RUNWAYARRIVALTRANSITIONS"), Section::RunwayArrivalTransitions },
    };

    auto badLine = [warnings](int lineNo) {
        if (warnings)
            addUnique(*warnings, QStringLiteral("ligne %1 : champ invalide, enregistrement ignoré").arg(lineNo));
    };

    Section section = Section::Unknown;
    int lineNo = 0;
    bool first = true;
    while (!in.atEnd()) {
        const QString line = in.readLine();
        ++lineNo;
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#')))
            continue;
        if (trimmed.startsWith(QLatin1Char('[')) && trimmed.endsWith(QLatin1Char(']'))) {
            section = sectionByName.value(trimmed.mid(1, trimmed.size() - 2), Section::Unknown);
            first = false;
            continue;
        }
        if (section == Section::Unknown)
            continue; // CONFIG et sections inconnues ignorées

        const QStringList tk = reader::parse::tokenize(line);
        if (tk.size() < 2) {
            badLine(lineNo);
            continue;
        }

        switch (section) {
        case Section::Points: {
            RawPoint p;
            p.id       = toUIntToken(tk, 1);
            p.ident    = tk.size() > 2 ? tk.at(2) : QString();
            p.lat      = toRealToken(tk, 3);
            p.lon      = toRealToken(tk, 4);
            p.magVar   = toRealToken(tk, 5);
            p.holdCourse = toRealToken(tk, 6);
            p.holdDist   = toRealToken(tk, 7);
            p.holdTime   = toRealToken(tk, 8);
            p.holdSide   = toIntToken(tk, 9);
            if (p.ident.isEmpty()) { badLine(lineNo); break; }
            data.points.insert(p.id, p);
            data.pointIdentByIdx.insert(p.id, p.ident);
            break;
        }
        case Section::Waypoints: {
            RawWaypoint w;
            w.id = toUIntToken(tk, 1);
            w.point = toIntToken(tk, 2);
            data.waypoints.insert(w.id, w);
            break;
        }
        case Section::Navaids: {
            RawNavaid n;
            n.id       = toUIntToken(tk, 1);
            n.type     = toUIntToken(tk, 2);
            n.point    = toIntToken(tk, 3);
            n.attached = toIntToken(tk, 4);
            n.elev     = toRealToken(tk, 5);
            n.decl     = toRealToken(tk, 6);
            n.freq     = toUIntToken(tk, 8);
            n.cat      = tk.size() > 9 ? tk.at(9) : QString();
            n.course   = toRealToken(tk, 10);
            n.angle    = toRealToken(tk, 11);
            n.runway   = toIntToken(tk, 12);
            data.navaids.insert(n.id, n);
            break;
        }
        case Section::Airports: {
            RawAirport a;
            a.id      = toUIntToken(tk, 1);
            a.point   = toIntToken(tk, 2);
            a.elev    = toRealToken(tk, 3);
            a.limitSpeed = toRealToken(tk, 4);
            a.limitAlt   = toRealToken(tk, 5);
            a.transAlt   = toRealToken(tk, 6);
            a.transLevel = toRealToken(tk, 7);
            data.airports.insert(a.id, a);
            data.airportPointByIdx.insert(a.id, a.point);
            break;
        }
        case Section::Runways: {
            RawRunway r;
            r.id       = toUIntToken(tk, 1);
            r.airport  = toIntToken(tk, 2);
            r.point    = toIntToken(tk, 3);
            r.elev     = toRealToken(tk, 4);
            r.gradient = toRealToken(tk, 5);
            r.course   = toRealToken(tk, 6);
            r.length   = toRealToken(tk, 7);
            r.displaced = toRealToken(tk, 8);
            r.stopway   = toRealToken(tk, 9);
            r.crossing  = toRealToken(tk, 10);
            data.runways.insert(r.id, r);
            data.runwayPointByIdx.insert(r.id, r.point);
            break;
        }
        case Section::LegSequences: {
            RawLegSeq l;
            l.id = toUIntToken(tk, 1);
            l.ident = tk.size() > 2 ? tk.at(2) : QString();
            l.type = toUIntToken(tk, 3);
            l.transitionMeters = toRealToken(tk, 4);
            if (l.ident.isEmpty()) { badLine(lineNo); break; }
            data.legSequences.insert(l.id, l);
            data.legSeqIdentByIdx.insert(l.id, l.ident);
            break;
        }
        case Section::Legs: {
            RawLeg l;
            l.id = toUIntToken(tk, 1);
            l.codeRaw   = toUIntToken(tk, 2);
            l.legSeq    = toIntToken(tk, 3);
            l.point     = toIntToken(tk, 4);
            l.usage     = toUIntToken(tk, 5);
            l.course    = toRealToken(tk, 6);
            l.dist      = toRealToken(tk, 7);
            l.navaidPoint = toIntToken(tk, 8);
            l.navaidCourse = toRealToken(tk, 9);
            l.navaidDist   = toRealToken(tk, 10);
            l.altMin    = toRealToken(tk, 11);
            l.altMax    = toRealToken(tk, 12);
            l.airSpeed  = toRealToken(tk, 13);
            l.path      = toRealToken(tk, 14);
            l.turn      = toIntToken(tk, 15);
            l.rnp       = toRealToken(tk, 16);
            data.legs.insert(l.id, l);
            break;
        }
        case Section::Departures: {
            RawProc p;
            p.id = toUIntToken(tk, 1);
            p.airport = toIntToken(tk, 2);
            p.legSeq  = toIntToken(tk, 3);
            data.sids.insert(p.id, p);
            break;
        }
        case Section::Arrivals: {
            RawProc p;
            p.id = toUIntToken(tk, 1);
            p.airport = toIntToken(tk, 2);
            p.legSeq  = toIntToken(tk, 3);
            data.stars.insert(p.id, p);
            break;
        }
        case Section::Approaches: {
            RawApproach a;
            a.id = toUIntToken(tk, 1);
            a.runway = toIntToken(tk, 2);
            a.legSeq = toIntToken(tk, 3);
            a.dh  = toRealToken(tk, 4);
            a.mda = toRealToken(tk, 5);
            data.approaches.insert(a.id, a);
            break;
        }
        case Section::ApproachTransitions: {
            RawTrans t;
            t.id = toUIntToken(tk, 1);
            t.ref = toIntToken(tk, 2);    // approach
            t.legSeq = toIntToken(tk, 3);
            data.appTrans.insert(t.id, t);
            break;
        }
        case Section::DepartureTransitions: {
            RawTrans t;
            t.id = toUIntToken(tk, 1);
            t.ref = toIntToken(tk, 2);    // proc SID
            t.legSeq = toIntToken(tk, 3);
            data.sidTrans.insert(t.id, t);
            break;
        }
        case Section::ArrivalTransitions: {
            RawTrans t;
            t.id = toUIntToken(tk, 1);
            t.ref = toIntToken(tk, 2);    // proc STAR
            t.legSeq = toIntToken(tk, 3);
            data.starTrans.insert(t.id, t);
            break;
        }
        case Section::RunwayDepartureTransitions: {
            RawRpt r;
            r.id = toUIntToken(tk, 1);
            r.runway = toIntToken(tk, 2);
            r.proc   = toIntToken(tk, 3);
            r.engineOut = toIntToken(tk, 4);
            r.legSeq = toIntToken(tk, 5);
            data.sidRpts.insert(r.id, r);
            break;
        }
        case Section::RunwayArrivalTransitions: {
            RawRpt r;
            r.id = toUIntToken(tk, 1);
            r.runway = toIntToken(tk, 2);
            r.proc   = toIntToken(tk, 3);
            r.engineOut = toIntToken(tk, 4);
            r.legSeq = toIntToken(tk, 5);
            data.starRpts.insert(r.id, r);
            break;
        }
        default:
            break;
        }
    }

    if (first && warnings)
        addUnique(*warnings, QStringLiteral("aucune section reconnue — fichier vide ou au format inattendu ?"));
    return data;
}

// ---------------------------------------------------------------------------
// Tables inverses libellé <-> valeur (symétriques de UserToModelConverter).
// ---------------------------------------------------------------------------
QString navaidLabel(quint32 value)
{
    if (value == 0)
        return QString();
    static const QHash<quint32, QString> table = {
        { 1, QStringLiteral("DME") },
        { 4, QStringLiteral("VOR") },
        { 8, QStringLiteral("TAC") },
        { 32, QStringLiteral("LOC") },
        { 64, QStringLiteral("GS") },
        { 97, QStringLiteral("LOC+GS+DME") },
        { 4096, QStringLiteral("Nav coloc") },
        { 8192, QStringLiteral("Nav non coloc") },
        { 8257, QStringLiteral("GS+DME non coloc") },
    };
    const auto it = table.constFind(value);
    if (it != table.constEnd())
        return *it;
    return QString::number(value); // passthrough numérique exact
}

struct SeqLabel { QString ils; QString kind; bool exact = true; };

QString wpLabel(quint32 value)
{
    if (value == 0)
        return QString();
    static const QHash<quint32, QString> table = {
        { 2, QStringLiteral("_U__") },       { 512, QStringLiteral("A___") },
        { 32, QStringLiteral("E___") },      { 32800, QStringLiteral("E__A") },
        { 131104, QStringLiteral("E__F") },  { 262176, QStringLiteral("E__I") },
        { 1048608, QStringLiteral("E__M") }, { 8388640, QStringLiteral("E_C_") },
        { 8421408, QStringLiteral("E_CA") }, { 1082130464, QStringLiteral("E_CH") },
        { 16777248, QStringLiteral("EE__") },{ 16842784, QStringLiteral("EE_B") },
        { 1090519072, QStringLiteral("EE_H") }, { 25165856, QStringLiteral("EEC_") },
        { 1098907680, QStringLiteral("EECH") }, { 34603040, QStringLiteral("EY_M") },
        { 41943072, QStringLiteral("EYC_") },   { 1024, QStringLiteral("G___") },
        { 1049600, QStringLiteral("G__M") },    { 34604032, QStringLiteral("GY_M") },
        { 2048, QStringLiteral("N___") },       { 16, QStringLiteral("P___") },
        { 64, QStringLiteral("R___") },         { 128, QStringLiteral("T___") },
        { 4096, QStringLiteral("V___") },
    };
    const auto it = table.constFind(value);
    if (it != table.constEnd())
        return *it;
    return QString::number(value); // passthrough numérique exact
}

} // namespace

// ---------------------------------------------------------------------------
ExtractBuildResult ExtractedProjectBuilder::build(const QString& extractedFilePath) const
{
    ExtractBuildResult result;
    QStringList warnings;

    const RawData data = parseRawFile(extractedFilePath, &result.errorMessage, &warnings);
    if (!result.errorMessage.isEmpty())
        return result;
    result.success = true;

    result.points      = static_cast<int>(data.points.size());
    result.waypoints   = static_cast<int>(data.waypoints.size());
    result.navaids     = static_cast<int>(data.navaids.size());
    result.airports    = static_cast<int>(data.airports.size());
    result.runways     = static_cast<int>(data.runways.size());
    result.legSequences= static_cast<int>(data.legSequences.size());
    result.legs        = static_cast<int>(data.legs.size());
    result.departures  = static_cast<int>(data.sids.size());
    result.arrivals    = static_cast<int>(data.stars.size());
    result.approaches  = static_cast<int>(data.approaches.size());
    result.appTransitions   = static_cast<int>(data.appTrans.size());
    result.depTransitions   = static_cast<int>(data.sidTrans.size());
    result.arrTransitions   = static_cast<int>(data.starTrans.size());
    result.rwyDepTransitions = static_cast<int>(data.sidRpts.size());
    result.rwyArrTransitions = static_cast<int>(data.starRpts.size());

    // -- résolution des identifiants par index (avec repli défensif) --
    auto pointIdentFor = [&data, &warnings](int idx) -> QString {
        if (idx < 0)
            return QStringLiteral("-1");
        const auto it = data.pointIdentByIdx.constFind(static_cast<quint32>(idx));
        if (it == data.pointIdentByIdx.constEnd()) {
            if (warnings.indexOf(QStringLiteral("référence à un point %1 absent de l'extraction (remplacée par \"-1\")").arg(idx)) < 0)
                warnings << QStringLiteral("référence à un point %1 absent de l'extraction (remplacée par \"-1\")").arg(idx);
            return QStringLiteral("-1");
        }
        return *it;
    };
    auto legSeqIdentFor = [&data, &warnings](int idx) -> QString {
        if (idx < 0)
            return QStringLiteral("-1");
        const auto it = data.legSeqIdentByIdx.constFind(static_cast<quint32>(idx));
        if (it == data.legSeqIdentByIdx.constEnd()) {
            if (warnings.indexOf(QStringLiteral("référence à une séquence %1 absente de l'extraction (remplacée par \"-1\")").arg(idx)) < 0)
                warnings << QStringLiteral("référence à une séquence %1 absente de l'extraction (remplacée par \"-1\")").arg(idx);
            return QStringLiteral("-1");
        }
        return *it;
    };
    auto runwayIdentFor = [&data, &pointIdentFor](int runwayIdx) -> QString {
        if (runwayIdx < 0)
            return QStringLiteral("-1");
        return pointIdentFor(data.runwayPointByIdx.value(static_cast<quint32>(runwayIdx), -1));
    };
    auto airportIdentFor = [&data, &pointIdentFor](int airportIdx) -> QString {
        if (airportIdx < 0)
            return QStringLiteral("-1");
        return pointIdentFor(data.airportPointByIdx.value(static_cast<quint32>(airportIdx), -1));
    };
    // Un champ "navaid" (LEG.navaidPoint, NAVAID.attached) porte l'INDEX d'un
    // navaid, pas celui d'un point : on remonte d'abord navaid -> point.
    auto navaidIdentFor = [&data, &pointIdentFor, &warnings](int navaidIdx) -> QString {
        if (navaidIdx < 0)
            return QStringLiteral("-1");
        const auto it = data.navaids.constFind(static_cast<quint32>(navaidIdx));
        if (it == data.navaids.constEnd()) {
            if (warnings.indexOf(QStringLiteral("référence à un navaid %1 absent de l'extraction (remplacée par \"-1\")").arg(navaidIdx)) < 0)
                warnings << QStringLiteral("référence à un navaid %1 absent de l'extraction (remplacée par \"-1\")").arg(navaidIdx);
            return QStringLiteral("-1");
        }
        return pointIdentFor(it.value().point);
    };
    auto approachIdentFor = [&data, &legSeqIdentFor](int approachIdx) -> QString {
        if (approachIdx < 0)
            return QStringLiteral("-1");
        const auto it = data.approaches.constFind(static_cast<quint32>(approachIdx));
        if (it == data.approaches.constEnd())
            return QStringLiteral("-1");
        return legSeqIdentFor(it.value().legSeq);
    };
    // SID et STAR ont des espaces d'id SÉPARÉS (compte par section) : on passe
    // la table du kind concerné (data.sids ou data.stars) pour résoudre t.ref.
    auto procedureIdentFor = [&legSeqIdentFor](const QHash<quint32, RawProc>& table, int procIdx) -> QString {
        if (procIdx < 0)
            return QStringLiteral("-1");
        const auto it = table.constFind(static_cast<quint32>(procIdx));
        if (it == table.constEnd())
            return QStringLiteral("-1");
        return legSeqIdentFor(it.value().legSeq);
    };

    // -- contexte des séquences (pour le type brut -> libellés) --
    QSet<quint32> seqIsApproachCore;
    for (const RawApproach& a : data.approaches)
        if (a.legSeq >= 0)
            seqIsApproachCore.insert(static_cast<quint32>(a.legSeq));
    QSet<quint32> seqIsApproachTrans;
    for (const RawTrans& t : data.appTrans)
        if (t.legSeq >= 0)
            seqIsApproachTrans.insert(static_cast<quint32>(t.legSeq));
    QSet<quint32> seqIsSid;
    for (const RawProc& p : data.sids)
        if (p.legSeq >= 0) seqIsSid.insert(static_cast<quint32>(p.legSeq));
    for (const RawTrans& t : data.sidTrans)
        if (t.legSeq >= 0) seqIsSid.insert(static_cast<quint32>(t.legSeq));
    for (const RawRpt& r : data.sidRpts)
        if (r.legSeq >= 0) seqIsSid.insert(static_cast<quint32>(r.legSeq));
    QSet<quint32> seqIsStar;
    for (const RawProc& p : data.stars)
        if (p.legSeq >= 0) seqIsStar.insert(static_cast<quint32>(p.legSeq));
    for (const RawTrans& t : data.starTrans)
        if (t.legSeq >= 0) seqIsStar.insert(static_cast<quint32>(t.legSeq));
    for (const RawRpt& r : data.starRpts)
        if (r.legSeq >= 0) seqIsStar.insert(static_cast<quint32>(r.legSeq));

    auto sequenceLabelFor = [&](quint32 raw, quint32 lsqIdx) -> SeqLabel {
        const bool appCore = seqIsApproachCore.contains(lsqIdx);
        const bool appTrans = seqIsApproachTrans.contains(lsqIdx);
        const bool sid = seqIsSid.contains(lsqIdx);
        const bool star = seqIsStar.contains(lsqIdx);
        const QString name = data.legSeqIdentByIdx.value(lsqIdx, QString());

        if (raw == 0)
            return { QStringLiteral("ILS"), QStringLiteral("APP TRANS"), true };

        if (appCore) {
            if (raw == 12) return { QStringLiteral("RNAV"), QStringLiteral("APP"), true };
            if (raw == 5)  return { QStringLiteral("ILS"),  QStringLiteral("APP"), true };
            if (warnings.indexOf(QStringLiteral("type de séquence %1 (\"%2\") approché par \"ILS|APP\" (non représentable)").arg(raw).arg(name)) < 0)
                warnings << QStringLiteral("type de séquence %1 (\"%2\") approché par \"ILS|APP\" (non représentable)").arg(raw).arg(name);
            return { QStringLiteral("ILS"), QStringLiteral("APP"), false };
        }

        const bool starKind = star && !sid;
        const bool sidKind  = sid;
        const QString kind = starKind ? QStringLiteral("STAR") : (sidKind ? QStringLiteral("SID") : (appTrans ? QStringLiteral("APP TRANS") : QStringLiteral("APP TRANS")));
        if (raw == 2) {
            if (appTrans) {
                if (warnings.indexOf(QStringLiteral("type de séquence 2 sur transition d'approche (\"%1\") approché par \"ILS|APP TRANS\"").arg(name)) < 0)
                    warnings << QStringLiteral("type de séquence 2 sur transition d'approche (\"%1\") approché par \"ILS|APP TRANS\"").arg(name);
                return { QStringLiteral("ILS"), QStringLiteral("APP TRANS"), false };
            }
            return { QStringLiteral("ILS"), kind, true };
        }
        if (raw == 5) {
            if (appTrans) return { QStringLiteral("ILS"), QStringLiteral("APP TRANS"), false };
            if (starKind || sidKind) return { QStringLiteral("RNAV"), kind, true };
            return { QStringLiteral("ILS"), QStringLiteral("APP"), true };
        }
        if (raw == 12)
            return { QStringLiteral("RNAV"), QStringLiteral("APP"), true };

        // 1/3/4/6 : non représentables dans les libellés — approximation
        if (warnings.indexOf(QStringLiteral("type de séquence %1 (\"%2\") non représentable, approché par \"%3|%4\"").arg(raw).arg(name).arg(raw >= 4 ? "RNAV" : "ILS").arg(kind)) < 0)
            warnings << QStringLiteral("type de séquence %1 (\"%2\") non représentable, approché par \"%3|%4\"").arg(raw).arg(name).arg(raw >= 4 ? "RNAV" : "ILS").arg(kind);
        return { raw >= 4 ? QStringLiteral("RNAV") : QStringLiteral("ILS"), kind, false };
    };

    // -- construction (l'ordre des familles ne dépend d'aucune référence
    //    croisée : tout passe par les identifiants) --
    UserProject& out = result.project;

    for (const RawPoint& p : data.points) {
        UserPoint up;
        up.ident         = p.ident;
        up.latitude      = p.lat;
        up.longitude     = p.lon;
        up.magVar        = p.magVar;
        up.holdCourse    = p.holdCourse;
        up.holdDistInMeters = p.holdDist;
        up.holdTime      = p.holdTime;
        up.holdSide      = static_cast<qint8>(p.holdSide);
        out.points().add(up, PointId(static_cast<qint32>(p.id)));
    }
    for (const RawWaypoint& w : data.waypoints) {
        UserWaypoint uw;
        uw.pointIdent = pointIdentFor(w.point);
        out.waypoints().add(uw, WaypointId(static_cast<qint32>(w.id)));
    }
    for (const RawAirport& a : data.airports) {
        UserAirport ua;
        ua.pointIdent = pointIdentFor(a.point);
        ua.elevationInMeters          = a.elev;
        ua.limitSpeedInMetersPerSec   = a.limitSpeed;
        ua.limitAltitudeInMeters      = a.limitAlt;
        ua.transitionAltitudeInMeters = a.transAlt;
        ua.transitionLevelInMeters    = a.transLevel;
        out.airports().add(ua, AirportId(static_cast<qint32>(a.id)));
    }
    for (const RawRunway& r : data.runways) {
        UserRunway ur;
        ur.airportIdent             = airportIdentFor(r.airport);
        ur.thresholdIdent           = pointIdentFor(r.point);
        ur.elevationInMeters        = r.elev;
        ur.gradient                 = r.gradient;
        ur.course                   = r.course;
        ur.lengthInMeters           = r.length;
        ur.displacedInMeters        = r.displaced;
        ur.stopwayInMeters          = r.stopway;
        ur.crossInMeters            = r.crossing;
        out.runways().add(ur, RunwayId(static_cast<qint32>(r.id)));
    }
    for (const RawNavaid& n : data.navaids) {
        UserNavaid un;
        un.type                  = navaidLabel(n.type);
        un.pointIdent            = pointIdentFor(n.point);
        un.associatedNavaidIdent = n.attached >= 0 ? navaidIdentFor(n.attached) : QStringLiteral("-1");
        un.elevationInMeters     = n.elev;
        un.declination           = n.decl;
        un.frequencyMHzTimes100  = n.freq;
        un.category              = n.cat;
        un.course                = n.course;
        un.angle                 = n.angle;
        un.runwayIdent           = n.runway >= 0 ? runwayIdentFor(n.runway) : QStringLiteral("-1");
        if (un.type.isEmpty())
            addUnique(warnings, QStringLiteral("navaid %1 : type brut nul — à resaisir manuellement").arg(n.id));
        out.navaids().add(un, NavaidId(static_cast<qint32>(n.id)));
    }
    for (const RawLegSeq& l : data.legSequences) {
        const SeqLabel label = sequenceLabelFor(l.type, l.id);
        UserLegSequence ul;
        ul.ident                 = l.ident;
        ul.ilsOrRnav             = label.ils;
        ul.procedureKind         = label.kind;
        ul.altitudeLevelTransInFeet = l.transitionMeters * kFeet;
        out.legSequences().add(ul, LegSequenceId(static_cast<qint32>(l.id)));
    }
    for (const RawLeg& l : data.legs) {
        UserLeg ul;
        ul.codePath                 = reader::parse::unpackLegCode(static_cast<quint16>(l.codeRaw));
        ul.legSequenceIdent         = legSeqIdentFor(l.legSeq);
        ul.pointIdent               = pointIdentFor(l.point);
        ul.wpDescription            = wpLabel(l.usage);
        ul.course                   = l.course;
        ul.distanceInMeters         = l.dist;
        ul.navaidIdent              = l.navaidPoint >= 0 ? navaidIdentFor(l.navaidPoint) : QStringLiteral("-1");
        ul.navaidCourse             = l.navaidCourse;
        ul.navaidDistanceInMeters   = l.navaidDist;
        ul.altitudeLimitMinInFeet   = (l.altMin < 0.0) ? l.altMin : l.altMin * kFeet;
        ul.altitudeLimitMaxInFeet   = (l.altMax < 0.0) ? l.altMax : l.altMax * kFeet;
        ul.airSpeedLimit            = l.airSpeed;
        ul.path                     = l.path;
        ul.turnDir                  = static_cast<qint8>(l.turn);
        ul.rnpInMeters              = l.rnp;
        out.legs().add(ul, LegId(static_cast<qint32>(l.id)));
    }
    for (const RawProc& p : data.sids) {
        UserProcedure up;
        up.airportIdent     = airportIdentFor(p.airport);
        up.legSequenceIdent = legSeqIdentFor(p.legSeq);
        out.sidProcedures().add(up, ProcedureId(static_cast<qint32>(p.id)));
    }
    for (const RawProc& p : data.stars) {
        UserProcedure up;
        up.airportIdent     = airportIdentFor(p.airport);
        up.legSequenceIdent = legSeqIdentFor(p.legSeq);
        out.starProcedures().add(up, ProcedureId(static_cast<qint32>(p.id)));
    }
    for (const RawApproach& a : data.approaches) {
        UserApproach ua;
        ua.runwayIdent           = runwayIdentFor(a.runway);
        ua.legSequenceIdent      = legSeqIdentFor(a.legSeq);
        ua.decisionHeightInFeet  = (a.dh < 0.0) ? a.dh : a.dh * kFeet;
        ua.minimumDescentInFeet  = (a.mda < 0.0) ? a.mda : a.mda * kFeet;
        out.approaches().add(ua, ApproachId(static_cast<qint32>(a.id)));
    }
    for (const RawTrans& t : data.appTrans) {
        UserApproachTransition ut;
        ut.approachIdent    = approachIdentFor(t.ref);
        ut.legSequenceIdent = legSeqIdentFor(t.legSeq);
        out.approachTransitions().add(ut, ApproachTransitionId(static_cast<qint32>(t.id)));
    }
    for (const RawTrans& t : data.sidTrans) {
        UserProcedureTransition up;
        up.procedureIdent   = procedureIdentFor(data.sids, t.ref);
        up.legSequenceIdent = legSeqIdentFor(t.legSeq);
        out.sidProcedureTransitions().add(up, ProcedureTransitionId(static_cast<qint32>(t.id)));
    }
    for (const RawTrans& t : data.starTrans) {
        UserProcedureTransition up;
        up.procedureIdent   = procedureIdentFor(data.stars, t.ref);
        up.legSequenceIdent = legSeqIdentFor(t.legSeq);
        out.starProcedureTransitions().add(up, ProcedureTransitionId(static_cast<qint32>(t.id)));
    }
    for (const RawRpt& r : data.sidRpts) {
        UserRunwayProcedureTransition ur;
        ur.runwayIdent      = runwayIdentFor(r.runway);
        ur.procedureIdent   = procedureIdentFor(data.sids, r.proc);
        ur.legSequenceIdent = legSeqIdentFor(r.legSeq);
        out.sidRunwayProcedureTransitions().add(ur, RunwayProcedureTransitionId(static_cast<qint32>(r.id)));
    }
    for (const RawRpt& r : data.starRpts) {
        UserRunwayProcedureTransition ur;
        ur.runwayIdent      = runwayIdentFor(r.runway);
        ur.procedureIdent   = procedureIdentFor(data.stars, r.proc);
        ur.legSequenceIdent = legSeqIdentFor(r.legSeq);
        out.starRunwayProcedureTransitions().add(ur, RunwayProcedureTransitionId(static_cast<qint32>(r.id)));
    }

    result.warnings = warnings;
    return result;
}

} // namespace navstud::extract