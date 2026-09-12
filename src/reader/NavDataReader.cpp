#include "NavDataReader.h"
#include "TextParse.h"

#include <QFile>
#include <QIODevice>
#include <QTextStream>

namespace navstud::reader {

using namespace navstud::model;

// -----------------------------------------------------------------------------------------------------------
// Lit un fichier texte entier et retourne ses lignes non vides avec état d'ouverture.
NavDataReader::RawLines NavDataReader::readRawLines(const QString& fileName, const QDir& dir) const
{
    RawLines result;
    QFile file(dir.filePath(fileName));
    result.fileExists = file.exists();
    if (!result.fileExists) {
        result.errorMessage = QStringLiteral("Fichier introuvable : %1").arg(file.fileName());
        return result;
    }
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result.errorMessage = file.errorString();
        return result;
    }

    result.opened = true;
    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    while (!in.atEnd()) {
        const QString line = in.readLine();
        if (!line.trimmed().isEmpty())
            result.lines << line;
    }
    return result;
}

// -----------------------------------------------------------------------------------------------------------
// Analyse le fichier _Point.txt et ajoute les points au dépôt.
NavDataReader::FileResult NavDataReader::readPoints(ProjectRepository& repo, const QDir& dir) const
{
    FileResult result;
    result.fileName = QStringLiteral("_Point.txt");

    const RawLines raw = readRawLines(result.fileName, dir);
    if (!raw.opened) {
        result.errors << raw.errorMessage;
        return result;
    }
    result.success = true;

    for (int i = 0; i < raw.lines.size(); ++i) {
        const QStringList tk = parse::tokenize(raw.lines.at(i));
        // POINT index ident lat lon magVar holdCourse holdDist holdTime holdSide
        if (tk.size() != 10 || tk.at(0) != QStringLiteral("POINT")) {
            result.errors << QStringLiteral("ligne %1 : format inattendu").arg(i + 1);
            continue;
        }
        const auto pid         = parse::id<PointTag>(tk.at(1));
        const auto lat         = parse::real(tk.at(3));
        const auto lon         = parse::real(tk.at(4));
        const auto magVar      = parse::real(tk.at(5));
        const auto holdCourse  = parse::real(tk.at(6));
        const auto holdDist    = parse::real(tk.at(7));
        const auto holdTime    = parse::real(tk.at(8));
        const auto holdSide    = parse::integer(tk.at(9));
        if (!pid || !lat || !lon || !magVar || !holdCourse || !holdDist || !holdTime || !holdSide) {
            result.errors << QStringLiteral("ligne %1 : champ numérique invalide").arg(i + 1);
            continue;
        }
        Point p;
        p.ident             = tk.at(2);
        p.latitude           = *lat;
        p.longitude          = *lon;
        p.magVar             = *magVar;
        p.holdCourse         = *holdCourse;
        p.holdDistInMeters   = *holdDist;
        p.holdTime           = *holdTime;
        p.holdSide           = static_cast<qint8>(*holdSide);
        repo.points().add(p, *pid);
        ++result.entitiesLoaded;
    }
    return result;
}

// -----------------------------------------------------------------------------------------------------------
// Lit le fichier _Waypoint.txt et ajoute les waypoints au dépôt.
NavDataReader::FileResult NavDataReader::readWaypoints(ProjectRepository& repo, const QDir& dir) const
{
    FileResult result;
    result.fileName = QStringLiteral("_Waypoint.txt");

    const RawLines raw = readRawLines(result.fileName, dir);
    if (!raw.opened) {
        result.errors << raw.errorMessage;
        return result;
    }
    result.success = true;

    for (int i = 0; i < raw.lines.size(); ++i) {
        const QStringList tk = parse::tokenize(raw.lines.at(i));
        if (tk.size() != 3 || tk.at(0) != QStringLiteral("WAYPOINT")) {
            result.errors << QStringLiteral("ligne %1 : format inattendu").arg(i + 1);
            continue;
        }
        const auto wid     = parse::id<WaypointTag>(tk.at(1));
        const auto pointId = parse::id<PointTag>(tk.at(2));
        if (!wid || !pointId) {
            result.errors << QStringLiteral("ligne %1 : champ invalide").arg(i + 1);
            continue;
        }
        Waypoint w;
        w.pointId = *pointId;
        repo.waypoints().add(w, *wid);
        ++result.entitiesLoaded;
    }
    return result;
}

// -----------------------------------------------------------------------------------------------------------
// Lit le fichier _Navaid.txt et ajoute les navaids au dépôt.
NavDataReader::FileResult NavDataReader::readNavaids(ProjectRepository& repo, const QDir& dir) const
{
    FileResult result;
    result.fileName = QStringLiteral("_Navaid.txt");

    const RawLines raw = readRawLines(result.fileName, dir);
    if (!raw.opened) {
        result.errors << raw.errorMessage;
        return result;
    }
    result.success = true;

    for (int i = 0; i < raw.lines.size(); ++i) {
        const QStringList tk = parse::tokenize(raw.lines.at(i));
        // NAVAID index type pointId distanceNavaidId elevation declination
        //        figureOfMerit freq category course angle runwayId
        if (tk.size() != 13 || tk.at(0) != QStringLiteral("NAVAID")) {
            result.errors << QStringLiteral("ligne %1 : format inattendu").arg(i + 1);
            continue;
        }
        const auto nid            = parse::id<NavaidTag>(tk.at(1));
        const auto type           = parse::flags<NavaidType>(tk.at(2));
        const auto pointId        = parse::id<PointTag>(tk.at(3));
        const auto distanceId     = parse::id<NavaidTag>(tk.at(4));
        const auto elevation      = parse::real(tk.at(5));
        const auto declination    = parse::real(tk.at(6));
        const auto figureOfMerit  = parse::unsignedInteger(tk.at(7));
        const auto freq           = parse::unsignedInteger(tk.at(8));
        const auto category       = parse::navaidCategory(tk.at(9));
        const auto course         = parse::real(tk.at(10));
        const auto angle          = parse::real(tk.at(11));
        const auto runwayId       = parse::id<RunwayTag>(tk.at(12));
        if (!nid || !type || !pointId || !distanceId || !elevation || !declination
            || !figureOfMerit || !freq || !category || !course || !angle || !runwayId) {
            result.errors << QStringLiteral("ligne %1 : champ invalide").arg(i + 1);
            continue;
        }
        Navaid n;
        n.type                  = *type;
        n.pointId                = *pointId;
        n.distanceNavaidId       = *distanceId;
        n.elevationInMeters      = *elevation;
        n.declination            = *declination;
        n.figureOfMerit          = *figureOfMerit;
        n.frequencyMHzTimes100   = *freq;
        n.category                = *category;
        n.course                  = *course;
        n.angle                   = *angle;
        n.runwayId                = *runwayId;
        repo.navaids().add(n, *nid);
        ++result.entitiesLoaded;
    }
    return result;
}

// -----------------------------------------------------------------------------------------------------------
// Lit le fichier _Airport.txt et ajoute les aéroports au dépôt.
NavDataReader::FileResult NavDataReader::readAirports(ProjectRepository& repo, const QDir& dir) const
{
    FileResult result;
    result.fileName = QStringLiteral("_Airport.txt");

    const RawLines raw = readRawLines(result.fileName, dir);
    if (!raw.opened) {
        result.errors << raw.errorMessage;
        return result;
    }
    result.success = true;

    for (int i = 0; i < raw.lines.size(); ++i) {
        const QStringList tk = parse::tokenize(raw.lines.at(i));
        if (tk.size() != 8 || tk.at(0) != QStringLiteral("AIRPORT")) {
            result.errors << QStringLiteral("ligne %1 : format inattendu").arg(i + 1);
            continue;
        }
        const auto aid            = parse::id<AirportTag>(tk.at(1));
        const auto pointId        = parse::id<PointTag>(tk.at(2));
        const auto elevation      = parse::real(tk.at(3));
        const auto limitSpeed     = parse::real(tk.at(4));
        const auto limitAltitude  = parse::real(tk.at(5));
        const auto transAltitude  = parse::real(tk.at(6));
        const auto transLevel     = parse::real(tk.at(7));
        if (!aid || !pointId || !elevation || !limitSpeed || !limitAltitude || !transAltitude || !transLevel) {
            result.errors << QStringLiteral("ligne %1 : champ invalide").arg(i + 1);
            continue;
        }
        Airport a;
        a.pointId                    = *pointId;
        a.elevationInMeters          = *elevation;
        a.limitSpeedInMetersPerSec   = *limitSpeed;
        a.limitAltitudeInMeters      = *limitAltitude;
        a.transitionAltitudeInMeters = *transAltitude;
        a.transitionLevelInMeters    = *transLevel;
        repo.airports().add(a, *aid);
        ++result.entitiesLoaded;
    }
    return result;
}

// -----------------------------------------------------------------------------------------------------------
// Lit le fichier _Runway.txt et ajoute les pistes au dépôt.
NavDataReader::FileResult NavDataReader::readRunways(ProjectRepository& repo, const QDir& dir) const
{
    FileResult result;
    result.fileName = QStringLiteral("_Runway.txt");

    const RawLines raw = readRawLines(result.fileName, dir);
    if (!raw.opened) {
        result.errors << raw.errorMessage;
        return result;
    }
    result.success = true;

    for (int i = 0; i < raw.lines.size(); ++i) {
        const QStringList tk = parse::tokenize(raw.lines.at(i));
        if (tk.size() != 11 || tk.at(0) != QStringLiteral("RUNWAY")) {
            result.errors << QStringLiteral("ligne %1 : format inattendu").arg(i + 1);
            continue;
        }
        const auto rid        = parse::id<RunwayTag>(tk.at(1));
        const auto airportId  = parse::id<AirportTag>(tk.at(2));
        const auto pointId    = parse::id<PointTag>(tk.at(3));
        const auto elevation  = parse::real(tk.at(4));
        const auto gradient   = parse::real(tk.at(5));
        const auto course     = parse::real(tk.at(6));
        const auto length     = parse::real(tk.at(7));
        const auto displaced  = parse::real(tk.at(8));
        const auto stopway    = parse::real(tk.at(9));
        const auto cross      = parse::real(tk.at(10));
        if (!rid || !airportId || !pointId || !elevation || !gradient || !course
            || !length || !displaced || !stopway || !cross) {
            result.errors << QStringLiteral("ligne %1 : champ invalide").arg(i + 1);
            continue;
        }
        Runway r;
        r.airportId        = *airportId;
        r.pointId           = *pointId;
        r.elevationInMeters = *elevation;
        r.gradient          = *gradient;
        r.course            = *course;
        r.lengthInMeters    = *length;
        r.displacedInMeters = *displaced;
        r.stopwayInMeters   = *stopway;
        r.crossInMeters     = *cross;
        repo.runways().add(r, *rid);
        ++result.entitiesLoaded;
    }
    return result;
}

// -----------------------------------------------------------------------------------------------------------
// Lit le fichier _LegSequence.txt et ajoute les séquences de legs au dépôt.
NavDataReader::FileResult NavDataReader::readLegSequences(ProjectRepository& repo, const QDir& dir) const
{
    FileResult result;
    result.fileName = QStringLiteral("_LegSequence.txt");

    const RawLines raw = readRawLines(result.fileName, dir);
    if (!raw.opened) {
        result.errors << raw.errorMessage;
        return result;
    }
    result.success = true;

    for (int i = 0; i < raw.lines.size(); ++i) {
        const QStringList tk = parse::tokenize(raw.lines.at(i));
        if (tk.size() != 5 || tk.at(0) != QStringLiteral("LEGSEQUENCE")) {
            result.errors << QStringLiteral("ligne %1 : format inattendu").arg(i + 1);
            continue;
        }
        const auto lsid        = parse::id<LegSequenceTag>(tk.at(1));
        const auto sequenceType = parse::unsignedInteger(tk.at(3));
        const auto transition   = parse::real(tk.at(4));
        if (!lsid || !sequenceType || !transition) {
            result.errors << QStringLiteral("ligne %1 : champ invalide").arg(i + 1);
            continue;
        }
        LegSequence ls;
        ls.ident              = tk.at(2);
        ls.sequenceTypeRaw    = static_cast<quint8>(*sequenceType);
        ls.transitionInMeters = *transition;
        repo.legSequences().add(ls, *lsid);
        ++result.entitiesLoaded;
    }
    return result;
}

// -----------------------------------------------------------------------------------------------------------
// Lit le fichier _Leg.txt et ajoute les legs au dépôt.
NavDataReader::FileResult NavDataReader::readLegs(ProjectRepository& repo, const QDir& dir) const
{
    FileResult result;
    result.fileName = QStringLiteral("_Leg.txt");

    const RawLines raw = readRawLines(result.fileName, dir);
    if (!raw.opened) {
        result.errors << raw.errorMessage;
        return result;
    }
    result.success = true;

    for (int i = 0; i < raw.lines.size(); ++i) {
        const QStringList tk = parse::tokenize(raw.lines.at(i));
        // LEG index code legSequenceId pointId pointUsage course distance
        //     navaidId navaidCourse navaidDistance altMin altMax airSpeed
        //     path turnDir rnp
        if (tk.size() != 17 || tk.at(0) != QStringLiteral("LEG")) {
            result.errors << QStringLiteral("ligne %1 : format inattendu").arg(i + 1);
            continue;
        }
        const auto lid            = parse::id<LegTag>(tk.at(1));
        const auto code           = parse::unsignedInteger(tk.at(2));
        const auto legSequenceId  = parse::id<LegSequenceTag>(tk.at(3));
        const auto pointId        = parse::id<PointTag>(tk.at(4));
        const auto pointUsage     = parse::flags<PointUsage>(tk.at(5));
        const auto course         = parse::real(tk.at(6));
        const auto distance       = parse::real(tk.at(7));
        const auto navaidId       = parse::id<NavaidTag>(tk.at(8));
        const auto navaidCourse   = parse::real(tk.at(9));
        const auto navaidDistance = parse::real(tk.at(10));
        const auto altMin         = parse::real(tk.at(11));
        const auto altMax         = parse::real(tk.at(12));
        const auto airSpeed       = parse::real(tk.at(13));
        const auto path           = parse::real(tk.at(14));
        const auto turnDir        = parse::integer(tk.at(15));
        const auto rnp            = parse::real(tk.at(16));
        if (!lid || !code || !legSequenceId || !pointId || !pointUsage || !course || !distance
            || !navaidId || !navaidCourse || !navaidDistance || !altMin || !altMax
            || !airSpeed || !path || !turnDir || !rnp) {
            result.errors << QStringLiteral("ligne %1 : champ invalide").arg(i + 1);
            continue;
        }
        Leg l;
        l.code                       = parse::unpackLegCode(static_cast<quint16>(*code));
        l.legSequenceId               = *legSequenceId;
        l.pointId                     = *pointId;
        l.pointUsage                  = *pointUsage;
        l.course                      = *course;
        l.distanceInMeters            = *distance;
        l.navaidId                    = *navaidId;
        l.navaidCourse                = *navaidCourse;
        l.navaidDistanceInMeters      = *navaidDistance;
        l.altitudeLimitMinInMeters    = *altMin;
        l.altitudeLimitMaxInMeters    = *altMax;
        l.airSpeedLimit                = *airSpeed;
        l.path                         = *path;
        l.turnDir                      = static_cast<qint8>(*turnDir);
        l.rnpInMeters                   = *rnp;
        repo.legs().add(l, *lid);
        ++result.entitiesLoaded;
    }
    return result;
}

// -----------------------------------------------------------------------------------------------------------
// Lit le fichier _Approach.txt et ajoute les approches au dépôt.
NavDataReader::FileResult NavDataReader::readApproaches(ProjectRepository& repo, const QDir& dir) const
{
    FileResult result;
    result.fileName = QStringLiteral("_Approach.txt");

    const RawLines raw = readRawLines(result.fileName, dir);
    if (!raw.opened) {
        result.errors << raw.errorMessage;
        return result;
    }
    result.success = true;

    for (int i = 0; i < raw.lines.size(); ++i) {
        const QStringList tk = parse::tokenize(raw.lines.at(i));
        if (tk.size() != 6 || tk.at(0) != QStringLiteral("APPROACH")) {
            result.errors << QStringLiteral("ligne %1 : format inattendu").arg(i + 1);
            continue;
        }
        const auto apid            = parse::id<ApproachTag>(tk.at(1));
        const auto runwayId        = parse::id<RunwayTag>(tk.at(2));
        const auto legSequenceId   = parse::id<LegSequenceTag>(tk.at(3));
        const auto decisionHeight  = parse::real(tk.at(4));
        const auto minimumDescent  = parse::real(tk.at(5));
        if (!apid || !runwayId || !legSequenceId || !decisionHeight || !minimumDescent) {
            result.errors << QStringLiteral("ligne %1 : champ invalide").arg(i + 1);
            continue;
        }
        Approach a;
        a.runwayId                = *runwayId;
        a.legSequenceId            = *legSequenceId;
        a.decisionHeightInMeters   = *decisionHeight;
        a.minimumDescentInMeters   = *minimumDescent;
        repo.approaches().add(a, *apid);
        ++result.entitiesLoaded;
    }
    return result;
}

// -----------------------------------------------------------------------------------------------------------
// Lit le fichier _ApproachTransition.txt et charge les transitions d'approche.
NavDataReader::FileResult NavDataReader::readApproachTransitions(ProjectRepository& repo, const QDir& dir) const
{
    FileResult result;
    result.fileName = QStringLiteral("_ApproachTransition.txt");

    const RawLines raw = readRawLines(result.fileName, dir);
    if (!raw.opened) {
        result.errors << raw.errorMessage;
        return result;
    }
    result.success = true;

    for (int i = 0; i < raw.lines.size(); ++i) {
        const QStringList tk = parse::tokenize(raw.lines.at(i));
        if (tk.size() != 4 || tk.at(0) != QStringLiteral("APPROACHTRANSITION")) {
            result.errors << QStringLiteral("ligne %1 : format inattendu").arg(i + 1);
            continue;
        }
        const auto tid           = parse::id<ApproachTransitionTag>(tk.at(1));
        const auto approachId    = parse::id<ApproachTag>(tk.at(2));
        const auto legSequenceId = parse::id<LegSequenceTag>(tk.at(3));
        if (!tid || !approachId || !legSequenceId) {
            result.errors << QStringLiteral("ligne %1 : champ invalide").arg(i + 1);
            continue;
        }
        ApproachTransition t;
        t.approachId    = *approachId;
        t.legSequenceId = *legSequenceId;
        repo.approachTransitions().add(t, *tid);
        ++result.entitiesLoaded;
    }
    return result;
}

// -----------------------------------------------------------------------------------------------------------
// Lit un fichier de procédures (SID ou STAR) et les ajoute au dépôt.
NavDataReader::FileResult NavDataReader::readProcedures(ProjectRepository& repo, ProcedureKind kind,
                                                         const QString& fileName, const QDir& dir) const
{
    FileResult result;
    result.fileName = fileName;

    const RawLines raw = readRawLines(fileName, dir);
    if (!raw.opened) {
        result.errors << raw.errorMessage;
        return result;
    }
    result.success = true;

    for (int i = 0; i < raw.lines.size(); ++i) {
        const QStringList tk = parse::tokenize(raw.lines.at(i));
        if (tk.size() != 4 || tk.at(0) != QStringLiteral("PROCEDURE")) {
            result.errors << QStringLiteral("ligne %1 : format inattendu").arg(i + 1);
            continue;
        }
        const auto pid           = parse::id<ProcedureTag>(tk.at(1));
        const auto airportId     = parse::id<AirportTag>(tk.at(2));
        const auto legSequenceId = parse::id<LegSequenceTag>(tk.at(3));
        if (!pid || !airportId || !legSequenceId) {
            result.errors << QStringLiteral("ligne %1 : champ invalide").arg(i + 1);
            continue;
        }
        Procedure p;
        p.airportId      = *airportId;
        p.legSequenceId = *legSequenceId;
        repo.procedures(kind).add(p, *pid);
        ++result.entitiesLoaded;
    }
    return result;
}

// -----------------------------------------------------------------------------------------------------------
// Lit un fichier de transitions de procédures (SID ou STAR) et les ajoute au dépôt.
NavDataReader::FileResult NavDataReader::readProcedureTransitions(ProjectRepository& repo, ProcedureKind kind,
                                                                   const QString& fileName, const QDir& dir) const
{
    FileResult result;
    result.fileName = fileName;

    const RawLines raw = readRawLines(fileName, dir);
    if (!raw.opened) {
        result.errors << raw.errorMessage;
        return result;
    }
    result.success = true;

    for (int i = 0; i < raw.lines.size(); ++i) {
        const QStringList tk = parse::tokenize(raw.lines.at(i));
        if (tk.size() != 4 || tk.at(0) != QStringLiteral("PROCEDURETRANSITION")) {
            result.errors << QStringLiteral("ligne %1 : format inattendu").arg(i + 1);
            continue;
        }
        const auto tid            = parse::id<ProcedureTransitionTag>(tk.at(1));
        const auto procedureId    = parse::id<ProcedureTag>(tk.at(2));
        const auto legSequenceId  = parse::id<LegSequenceTag>(tk.at(3));
        if (!tid || !procedureId || !legSequenceId) {
            result.errors << QStringLiteral("ligne %1 : champ invalide").arg(i + 1);
            continue;
        }
        ProcedureTransition t;
        t.procedureId     = *procedureId;
        t.legSequenceId   = *legSequenceId;
        repo.procedureTransitions(kind).add(t, *tid);
        ++result.entitiesLoaded;
    }
    return result;
}

// -----------------------------------------------------------------------------------------------------------
// Lit un fichier de transitions piste/procédure (SID ou STAR) et les charge.
NavDataReader::FileResult NavDataReader::readRunwayProcedureTransitions(ProjectRepository& repo, ProcedureKind kind,
                                                                         const QString& fileName, const QDir& dir) const
{
    FileResult result;
    result.fileName = fileName;

    const RawLines raw = readRawLines(fileName, dir);
    if (!raw.opened) {
        result.errors << raw.errorMessage;
        return result;
    }
    result.success = true;

    for (int i = 0; i < raw.lines.size(); ++i) {
        const QStringList tk = parse::tokenize(raw.lines.at(i));
        if (tk.size() != 6 || tk.at(0) != QStringLiteral("RUNWAYPROCEDURETRANSITION")) {
            result.errors << QStringLiteral("ligne %1 : format inattendu").arg(i + 1);
            continue;
        }
        const auto tid                  = parse::id<RunwayProcedureTransitionTag>(tk.at(1));
        const auto runwayId             = parse::id<RunwayTag>(tk.at(2));
        const auto procedureId          = parse::id<ProcedureTag>(tk.at(3));
        const auto engineOutProcedureId = parse::id<ProcedureTag>(tk.at(4));
        const auto legSequenceId        = parse::id<LegSequenceTag>(tk.at(5));
        if (!tid || !runwayId || !procedureId || !engineOutProcedureId || !legSequenceId) {
            result.errors << QStringLiteral("ligne %1 : champ invalide").arg(i + 1);
            continue;
        }
        RunwayProcedureTransition t;
        t.runwayId               = *runwayId;
        t.procedureId            = *procedureId;
        t.engineOutProcedureId  = *engineOutProcedureId;
        t.legSequenceId          = *legSequenceId;
        repo.runwayProcedureTransitions(kind).add(t, *tid);
        ++result.entitiesLoaded;
    }
    return result;
}

// -----------------------------------------------------------------------------------------------------------
// Charge successivement tous les fichiers texte de données en une seule passe.
QVector<NavDataReader::FileResult> NavDataReader::readAll(ProjectRepository& repo, const QDir& inputDir) const
{
    QVector<FileResult> results;
    results << readAirports(repo, inputDir);
    results << readRunways(repo, inputDir);
    results << readPoints(repo, inputDir);
    results << readWaypoints(repo, inputDir);
    results << readNavaids(repo, inputDir);
    results << readLegSequences(repo, inputDir);
    results << readLegs(repo, inputDir);
    results << readApproaches(repo, inputDir);
    results << readApproachTransitions(repo, inputDir);
    results << readProcedures(repo, ProcedureKind::Sid, QStringLiteral("_ProcedureSID.txt"), inputDir);
    results << readProcedures(repo, ProcedureKind::Star, QStringLiteral("_ProcedureSTAR.txt"), inputDir);
    results << readProcedureTransitions(repo, ProcedureKind::Sid, QStringLiteral("_ProcTransSID.txt"), inputDir);
    results << readProcedureTransitions(repo, ProcedureKind::Star, QStringLiteral("_ProcTransSTAR.txt"), inputDir);
    results << readRunwayProcedureTransitions(repo, ProcedureKind::Sid, QStringLiteral("_RunProcTransSID.txt"), inputDir);
    results << readRunwayProcedureTransitions(repo, ProcedureKind::Star, QStringLiteral("_RunProcTransSTAR.txt"), inputDir);
    return results;
}

} // namespace navstud::reader
