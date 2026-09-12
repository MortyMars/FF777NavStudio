#include "WorldFileRewriter.h"

#include <QByteArray>
#include <QFile>
#include <QHash>
#include <QVector>

#include <algorithm>

namespace navstud::extract {

namespace {

// ---------------------------------------------------------------------------
// Sections du fichier mondial, dans l'ordre du format (cf. FileConverter).
// ---------------------------------------------------------------------------
enum Sec {
    SConfig = 0,
    SPoints,
    SWaypoints,
    SNavaids,
    SAirports,
    SRunways,
    SLegSequences,
    SLegs,
    SDepartures,
    SArrivals,
    SApproaches,
    SDepartureTransitions,
    SArrivalTransitions,
    SApproachTransitions,
    SRunwayDepartureTransitions,
    SRunwayArrivalTransitions,
    SAirways,
    SAirwaySegments,
    SAirwaySegmentLegs,
    SRoutes,
    SRouteSegments,
    SecCount
};

// Champ de référence : index du token dans la ligne + section visée.
struct Ref
{
    int token;
    int section;
};

// Un enregistrement peut référencer un id d'une autre section ; les tables
// ci-dessous listent, pour chaque section, les positions (token) des champs
// de référence et la section cible. Le token 0 est le mot-clé, le token 1
// l'id de l'enregistrement lui-même.
const QVector<Ref>& refsPoints()       { static const QVector<Ref> v = {}; return v; }
const QVector<Ref>& refsWaypoints()    { static const QVector<Ref> v = { {2, SPoints} }; return v; }
const QVector<Ref>& refsNavaids()      { static const QVector<Ref> v = { {3, SPoints}, {4, SNavaids}, {12, SRunways} }; return v; }
const QVector<Ref>& refsAirports()     { static const QVector<Ref> v = { {2, SPoints} }; return v; }
const QVector<Ref>& refsRunways()      { static const QVector<Ref> v = { {2, SAirports}, {3, SPoints} }; return v; }
const QVector<Ref>& refsLegSequences() { static const QVector<Ref> v = {}; return v; }
const QVector<Ref>& refsLegs()         { static const QVector<Ref> v = { {3, SLegSequences}, {4, SPoints}, {8, SNavaids} }; return v; }
const QVector<Ref>& refsDepartures()   { static const QVector<Ref> v = { {2, SAirports}, {3, SLegSequences} }; return v; }
const QVector<Ref>& refsArrivals()     { static const QVector<Ref> v = { {2, SAirports}, {3, SLegSequences} }; return v; }
const QVector<Ref>& refsApproaches()   { static const QVector<Ref> v = { {2, SRunways}, {3, SLegSequences} }; return v; }
const QVector<Ref>& refsDepTrans()     { static const QVector<Ref> v = { {2, SDepartures}, {3, SLegSequences} }; return v; }
const QVector<Ref>& refsArrTrans()     { static const QVector<Ref> v = { {2, SArrivals}, {3, SLegSequences} }; return v; }
const QVector<Ref>& refsAppTrans()     { static const QVector<Ref> v = { {2, SApproaches}, {3, SLegSequences} }; return v; }
const QVector<Ref>& refsRwyDepTrans()  { static const QVector<Ref> v = { {2, SRunways}, {3, SDepartures}, {4, SDepartures}, {5, SLegSequences} }; return v; }
const QVector<Ref>& refsRwyArrTrans()  { static const QVector<Ref> v = { {2, SRunways}, {3, SArrivals}, {4, SArrivals}, {5, SLegSequences} }; return v; }
const QVector<Ref>& refsAirways()      { static const QVector<Ref> v = {}; return v; }
const QVector<Ref>& refsAirwaySegs()   { static const QVector<Ref> v = { {2, SAirways} }; return v; }
const QVector<Ref>& refsAirwaySegLegs(){ static const QVector<Ref> v = { {2, SAirwaySegments}, {3, SPoints} }; return v; }
const QVector<Ref>& refsRoutes()       { static const QVector<Ref> v = {
        {4, SAirports}, {5, SRunways}, {6, SDepartures}, {7, SDepartureTransitions},
        {8, SArrivals}, {9, SArrivalTransitions}, {10, SApproaches},
        {11, SApproachTransitions}, {12, SAirports}, {13, SRoutes} }; return v; }
const QVector<Ref>& refsRouteSegs()    { static const QVector<Ref> v = { {2, SRoutes}, {3, SAirways}, {4, SPoints} }; return v; }

struct SectionDef
{
    const char* name;    // nom de section, ex. "POINTS"
    bool        renumber; // l'id de chaque enregistrement est-il renuméroté ?
    const QVector<Ref>* refs;
};

const SectionDef* sectionDefs()
{
    static const SectionDef defs[SecCount] = {
        { "CONFIG",                     false, nullptr },
        { "POINTS",                     true,  nullptr },
        { "WAYPOINTS",                  true,  nullptr },
        { "NAVAIDS",                    true,  nullptr },
        { "AIRPORTS",                   true,  nullptr },
        { "RUNWAYS",                    true,  nullptr },
        { "LEGSEQUENCES",               true,  nullptr },
        { "LEGS",                       true,  nullptr },
        { "DEPARTURES",                 true,  nullptr },
        { "ARRIVALS",                   true,  nullptr },
        { "APPROACHES",                 true,  nullptr },
        { "DEPARTURETRANSITIONS",       true,  nullptr },
        { "ARRIVALTRANSITIONS",         true,  nullptr },
        { "APPROACHTRANSITIONS",        true,  nullptr },
        { "RUNWAYDEPARTURETRANSITIONS", true,  nullptr },
        { "RUNWAYARRIVALTRANSITIONS",   true,  nullptr },
        { "AIRWAYS",                    true,  nullptr },
        { "AIRWAYSEGMENTS",             true,  nullptr },
        { "AIRWAYSEGMENTLEGS",          true,  nullptr },
        { "ROUTES",                     true,  nullptr },
        { "ROUTESEGMENTS",              true,  nullptr },
    };
    return defs;
}

// Tables de références, résolues paresseusement (évite l'ordre statique).
const QVector<Ref>* refsFor(int sec)
{
    switch (sec) {
    case SPoints:                   return &refsPoints();
    case SWaypoints:                return &refsWaypoints();
    case SNavaids:                  return &refsNavaids();
    case SAirports:                 return &refsAirports();
    case SRunways:                  return &refsRunways();
    case SLegSequences:             return &refsLegSequences();
    case SLegs:                     return &refsLegs();
    case SDepartures:               return &refsDepartures();
    case SArrivals:                 return &refsArrivals();
    case SApproaches:               return &refsApproaches();
    case SDepartureTransitions:     return &refsDepTrans();
    case SArrivalTransitions:       return &refsArrTrans();
    case SApproachTransitions:      return &refsAppTrans();
    case SRunwayDepartureTransitions: return &refsRwyDepTrans();
    case SRunwayArrivalTransitions: return &refsRwyArrTrans();
    case SAirways:                  return &refsAirways();
    case SAirwaySegments:           return &refsAirwaySegs();
    case SAirwaySegmentLegs:        return &refsAirwaySegLegs();
    case SRoutes:                   return &refsRoutes();
    case SRouteSegments:            return &refsRouteSegs();
    default:                        return nullptr;
    }
}

// Vecteur de sélection (marquage « à supprimer ») associé à une section.
const std::vector<char>* selectionFor(int sec, const NavDataBase::AirportSelection& sel)
{
    switch (sec) {
    case SPoints:                   return &sel.points;
    case SWaypoints:                return &sel.waypoints;
    case SNavaids:                  return &sel.navaids;
    case SAirports:                 return &sel.airports;
    case SRunways:                  return &sel.runways;
    case SLegSequences:             return &sel.legSequences;
    case SLegs:                     return &sel.legs;
    case SDepartures:               return &sel.departures;
    case SArrivals:                 return &sel.arrivals;
    case SApproaches:               return &sel.approaches;
    case SDepartureTransitions:     return &sel.departureTransitions;
    case SArrivalTransitions:       return &sel.arrivalTransitions;
    case SApproachTransitions:      return &sel.approachTransitions;
    case SRunwayDepartureTransitions: return &sel.runwayDepartureTransitions;
    case SRunwayArrivalTransitions: return &sel.runwayArrivalTransitions;
    default:                        return nullptr; // AIRWAYS/ROUTES jamais supprimées
    }
}

// ---------------------------------------------------------------------------
// Découpage d'une ligne en tokens avec leurs positions, en respectant les
// portions entre guillemets. valueStart/valueLen délimitent le CONTENU (sans
// guillemets) ; start/end délimitent le token complet.
// ---------------------------------------------------------------------------
struct Tok
{
    int start = 0;
    int end = 0;
    int valueStart = 0;
    int valueLen = 0;
};

QVector<Tok> splitTokens(const QByteArray& line)
{
    QVector<Tok> out;
    const char* d = line.constData();
    const int n = line.size();
    int i = 0;
    while (i < n) {
        while (i < n && (d[i] == ' ' || d[i] == '\t' || d[i] == '\r'))
            ++i;
        if (i >= n)
            break;
        Tok t;
        t.start = i;
        if (d[i] == '"') {
            int j = i + 1;
            while (j < n && d[j] != '"')
                ++j;
            t.valueStart = i + 1;
            t.valueLen = j - i - 1;
            i = (j < n) ? j + 1 : j;
            t.end = i;
        } else {
            int j = i;
            while (j < n && d[j] != ' ' && d[j] != '\t' && d[j] != '\r')
                ++j;
            t.valueStart = i;
            t.valueLen = j - i;
            i = j;
            t.end = i;
        }
        out.push_back(t);
    }
    return out;
}

qint32 tokenInt(const QByteArray& line, const QVector<Tok>& toks, int index)
{
    if (index < 0 || index >= toks.size())
        return -1;
    bool ok = false;
    const qint32 v = line.mid(toks.at(index).valueStart, toks.at(index).valueLen).toInt(&ok);
    return ok ? v : -1;
}

// Remplace, dans une ligne, les tokens désignés par (index, nouvelle valeur).
QByteArray rewriteLine(const QByteArray& line, const QVector<Tok>& toks,
                       QVector<QPair<int, QByteArray>>& replacements)
{
    std::sort(replacements.begin(), replacements.end(),
              [&toks](const QPair<int, QByteArray>& a, const QPair<int, QByteArray>& b) {
                  return toks.at(a.first).valueStart > toks.at(b.first).valueStart;
              });

    QByteArray result = line;
    for (const auto& r : replacements)
        result.replace(toks.at(r.first).valueStart, toks.at(r.first).valueLen, r.second);
    return result;
}

} // namespace

// ---------------------------------------------------------------------------
WorldFileRewriter::Result WorldFileRewriter::removeAirport(
    const QString& sourcePath, const NavDataBase::AirportSelection& selection)
{
    return removeAirport(sourcePath, selection, sourcePath + QStringLiteral(".bak"));
}

WorldFileRewriter::Result WorldFileRewriter::removeAirport(
    const QString& sourcePath, const NavDataBase::AirportSelection& selection,
    const QString& backupPath)
{
    Result result;

    QFile in(sourcePath);
    if (!in.open(QIODevice::ReadOnly)) {
        result.error = QStringLiteral("Impossible d'ouvrir %1 : %2").arg(sourcePath, in.errorString());
        return result;
    }
    const QByteArray data = in.readAll();
    in.close();

    // Correspondance nom de section -> index (construite à la volée).
    QHash<QByteArray, int> sectionByName;
    const SectionDef* defs = sectionDefs();
    for (int s = 0; s < SecCount; ++s)
        sectionByName.insert(QByteArray(defs[s].name), s);

    // map[s][oldId] : -2 = inconnu, -1 = supprimé, >= 0 = nouvel id.
    std::vector<qint32> map[SecCount];
    qint32 nextId[SecCount] = {0};

    auto ensureMap = [&](int s, int id) {
        if (id < 0)
            return;
        if (id >= static_cast<int>(map[s].size()))
            map[s].resize(id + 1, -2);
    };

    // ---------------- Passe 1 : calcul des correspondances d'ids -------------
    {
        int current = -1;
        int pos = 0;
        while (pos < data.size()) {
            int nl = data.indexOf('\n', pos);
            if (nl < 0)
                nl = data.size();
            const QByteArray line = data.mid(pos, nl - pos);
            pos = nl + 1;

            const QByteArray trimmed = line.trimmed();
            if (trimmed.isEmpty() || trimmed.startsWith('#'))
                continue;

            if (trimmed.startsWith('[') && trimmed.endsWith(']')) {
                const QByteArray name = trimmed.mid(1, trimmed.size() - 2);
                current = sectionByName.value(name, -1);
                continue;
            }

            if (current < 0 || current == SConfig || !defs[current].renumber)
                continue;

            const QVector<Tok> toks = splitTokens(line);
            if (toks.size() < 2)
                continue;

            const qint32 id = tokenInt(line, toks, 1);
            if (id < 0)
                continue;

            ensureMap(current, id);
            if (map[current][id] != -2)
                continue; // id déjà vu (ne devrait pas arriver) : on garde le premier

            const std::vector<char>* sel = selectionFor(current, selection);
            const bool removed = sel && id < static_cast<int>(sel->size()) && (*sel)[id] != 0;
            if (removed) {
                map[current][id] = -1;
                ++result.removedRecords;
            } else {
                map[current][id] = nextId[current]++;
            }
        }
    }

    // ---------------- Passe 2 : réécriture ----------------------------------
    QByteArray out;
    out.reserve(data.size());

    {
        int current = -1;
        int pos = 0;
        while (pos < data.size()) {
            int nl = data.indexOf('\n', pos);
            if (nl < 0)
                nl = data.size();
            const QByteArray line = data.mid(pos, nl - pos);
            pos = nl + 1;

            const QByteArray trimmed = line.trimmed();

            if (trimmed.isEmpty()) {
                out.append(line);
                out.append('\n');
                continue;
            }

            if (trimmed.startsWith('[') && trimmed.endsWith(']')) {
                const QByteArray name = trimmed.mid(1, trimmed.size() - 2);
                current = sectionByName.value(name, -1);
                out.append(line);
                out.append('\n');
                continue;
            }

            if (trimmed.startsWith('#')) {
                // Recalcule le compteur de la section courante.
                if (trimmed.startsWith("# Count:") && current >= 0 && defs[current].renumber) {
                    out.append(QByteArray("# Count: ") + QByteArray::number(nextId[current]));
                    out.append('\n');
                } else {
                    out.append(line);
                    out.append('\n');
                }
                continue;
            }

            if (current < 0 || current == SConfig || !defs[current].renumber) {
                out.append(line);
                out.append('\n');
                continue;
            }

            const QVector<Tok> toks = splitTokens(line);
            if (toks.size() < 2) {
                out.append(line);
                out.append('\n');
                continue;
            }

            const qint32 id = tokenInt(line, toks, 1);
            if (id >= 0 && id < static_cast<int>(map[current].size())
                && map[current][id] == -1) {
                continue; // enregistrement supprimé
            }

            QVector<QPair<int, QByteArray>> replacements;

            // Id de l'enregistrement lui-même.
            if (id >= 0 && id < static_cast<int>(map[current].size())
                && map[current][id] >= 0 && map[current][id] != id) {
                replacements.push_back({ 1, QByteArray::number(map[current][id]) });
            }

            // Champs de référence croisée.
            const QVector<Ref>* refs = refsFor(current);
            if (refs) {
                for (const Ref& r : *refs) {
                    const qint32 oldRef = tokenInt(line, toks, r.token);
                    if (oldRef < 0 || r.section < 0 || r.section >= SecCount)
                        continue;
                    if (oldRef >= static_cast<int>(map[r.section].size()))
                        continue;
                    const qint32 mapped = map[r.section][oldRef];
                    if (mapped == -2)
                        continue;
                    qint32 newRef = mapped;
                    if (mapped == -1) {
                        newRef = -1;
                        ++result.danglingReferences;
                    }
                    if (newRef != oldRef)
                        replacements.push_back({ r.token, QByteArray::number(newRef) });
                }
            }

            if (replacements.isEmpty())
                out.append(line);
            else
                out.append(rewriteLine(line, toks, replacements));
            out.append('\n');
        }
    }

    // ---------------- Sauvegarde puis écriture atomique ---------------------
    QFile::remove(backupPath);
    if (!QFile::copy(sourcePath, backupPath)) {
        result.error = QStringLiteral("Impossible de créer la sauvegarde %1.").arg(backupPath);
        return result;
    }

    const QString tmpPath = sourcePath + QStringLiteral(".tmp");
    QFile tmp(tmpPath);
    if (!tmp.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        result.error = QStringLiteral("Impossible d'écrire %1 : %2").arg(tmpPath, tmp.errorString());
        return result;
    }
    if (tmp.write(out) != out.size()) {
        result.error = QStringLiteral("Écriture incomplète de %1.").arg(tmpPath);
        tmp.close();
        QFile::remove(tmpPath);
        return result;
    }
    tmp.close();

    if (!QFile::remove(sourcePath)) {
        result.error = QStringLiteral("Impossible de remplacer %1 (suppression impossible).").arg(sourcePath);
        QFile::remove(tmpPath);
        return result;
    }
    if (!QFile::rename(tmpPath, sourcePath)) {
        result.error = QStringLiteral("Impossible de renommer %1 vers %2 (sauvegarde : %3).")
                           .arg(tmpPath, sourcePath, backupPath);
        return result;
    }

    result.success = true;
    return result;
}

} // namespace navstud::extract
