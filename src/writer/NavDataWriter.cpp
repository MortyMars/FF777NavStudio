#include "NavDataWriter.h"
#include "TextFormat.h"

#include <QFile>
#include <QIODevice>
#include <QTextStream>

namespace navstud::writer {

using namespace navstud::model;
namespace fmt = navstud::writer::format;

// -----------------------------------------------------------------------------------------------------------
// Écrit une liste de lignes dans un fichier texte UTF-8 du répertoire cible.
NavDataWriter::FileResult NavDataWriter::writeLines(const QStringList& lines, const QString& fileName, const QDir& dir) const
{
    FileResult result;
    result.fileName = fileName;

    if (!dir.exists() && !QDir().mkpath(dir.absolutePath())) {
        result.errorMessage = QStringLiteral("Impossible de créer le répertoire de sortie : %1").arg(dir.absolutePath());
        return result;
    }

    QFile file(dir.filePath(fileName));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        result.errorMessage = file.errorString();
        return result;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    for (const QString& line : lines)
        out << line << '\n';

    result.success      = true;
    result.linesWritten = lines.size();
    return result;
}

// -----------------------------------------------------------------------------------------------------------
// Formate un point en une ligne texte au format de fichier NavData.
QString NavDataWriter::formatPointLine(PointId id, const Point& point)
{
    return QStringList{
        QStringLiteral("POINT"),
        fmt::id(id),
        fmt::quoted(point.ident),
        fmt::fixed(point.latitude, 12),
        fmt::fixed(point.longitude, 12),
        fmt::fixed(point.magVar, 6),
        fmt::fixed(point.holdCourse, 6),
        fmt::fixed(point.holdDistInMeters, 3),
        fmt::fixed(point.holdTime, 3),
        QString::number(point.holdSide),
    }.join(QLatin1Char(' '));
}

// -----------------------------------------------------------------------------------------------------------
// Écrit tous les points du dépôt dans le fichier _Point.txt.
NavDataWriter::FileResult NavDataWriter::writePoints(const ProjectRepository& repo, const QDir& dir) const
{
    QStringList lines;
    for (const PointId pid : repo.points().order()) {
        const Point* p = repo.points().find(pid);
        if (!p)
            continue;
        lines << formatPointLine(pid, *p);
    }
    return writeLines(lines, QStringLiteral("_Point.txt"), dir);
}

// -----------------------------------------------------------------------------------------------------------
// Formate un waypoint en une ligne texte au format de fichier NavData.
QString NavDataWriter::formatWaypointLine(WaypointId id, const Waypoint& w)
{
    return QStringList{ QStringLiteral("WAYPOINT"), fmt::id(id), fmt::id(w.pointId) }.join(QLatin1Char(' '));
}

// -----------------------------------------------------------------------------------------------------------
// Écrit tous les waypoints du dépôt dans le fichier _Waypoint.txt.
NavDataWriter::FileResult NavDataWriter::writeWaypoints(const ProjectRepository& repo, const QDir& dir) const
{
    QStringList lines;
    for (const WaypointId wid : repo.waypoints().order()) {
        const Waypoint* w = repo.waypoints().find(wid);
        if (!w)
            continue;
        lines << formatWaypointLine(wid, *w);
    }
    return writeLines(lines, QStringLiteral("_Waypoint.txt"), dir);
}

// -----------------------------------------------------------------------------------------------------------
// Formate un navaid en une ligne texte au format de fichier NavData.
QString NavDataWriter::formatNavaidLine(NavaidId id, const Navaid& n)
{
    return QStringList{
        QStringLiteral("NAVAID"),
        fmt::id(id),
        fmt::flagsToInt(n.type),
        fmt::id(n.pointId),
        fmt::id(n.distanceNavaidId),
        fmt::fixed(n.elevationInMeters, 6),
        fmt::fixed(n.declination, 6),
        QString::number(n.figureOfMerit),
        QString::number(n.frequencyMHzTimes100),
        fmt::navaidCategory(n.category),
        fmt::fixed(n.course, 6),
        fmt::fixed(n.angle, 6),
        fmt::id(n.runwayId),
    }.join(QLatin1Char(' '));
}

// -----------------------------------------------------------------------------------------------------------
// Écrit tous les navaids du dépôt dans le fichier _Navaid.txt.
NavDataWriter::FileResult NavDataWriter::writeNavaids(const ProjectRepository& repo, const QDir& dir) const
{
    QStringList lines;
    for (const NavaidId nid : repo.navaids().order()) {
        const Navaid* n = repo.navaids().find(nid);
        if (!n)
            continue;
        lines << formatNavaidLine(nid, *n);
    }
    return writeLines(lines, QStringLiteral("_Navaid.txt"), dir);
}

// -----------------------------------------------------------------------------------------------------------
// Formate un aéroport en une ligne texte au format de fichier NavData.
QString NavDataWriter::formatAirportLine(AirportId id, const Airport& a)
{
    return QStringList{
        QStringLiteral("AIRPORT"),
        fmt::id(id),
        fmt::id(a.pointId),
        fmt::fixed(a.elevationInMeters, 6),
        fmt::fixed(a.limitSpeedInMetersPerSec, 6),
        fmt::fixed(a.limitAltitudeInMeters, 6),
        fmt::fixed(a.transitionAltitudeInMeters, 6),
        fmt::fixed(a.transitionLevelInMeters, 6),
    }.join(QLatin1Char(' '));
}

// -----------------------------------------------------------------------------------------------------------
// Écrit tous les aéroports du dépôt dans le fichier _Airport.txt.
NavDataWriter::FileResult NavDataWriter::writeAirports(const ProjectRepository& repo, const QDir& dir) const
{
    QStringList lines;
    for (const AirportId aid : repo.airports().order()) {
        const Airport* a = repo.airports().find(aid);
        if (!a)
            continue;
        lines << formatAirportLine(aid, *a);
    }
    return writeLines(lines, QStringLiteral("_Airport.txt"), dir);
}

// -----------------------------------------------------------------------------------------------------------
// Formate une piste en une ligne texte au format de fichier NavData.
QString NavDataWriter::formatRunwayLine(RunwayId id, const Runway& r)
{
    return QStringList{
        QStringLiteral("RUNWAY"),
        fmt::id(id),
        fmt::id(r.airportId),
        fmt::id(r.pointId),
        fmt::fixed(r.elevationInMeters, 6),
        fmt::fixed(r.gradient, 6),
        fmt::fixed(r.course, 6),
        fmt::fixed(r.lengthInMeters, 6),
        fmt::fixed(r.displacedInMeters, 6),
        fmt::fixed(r.stopwayInMeters, 6),
        fmt::fixed(r.crossInMeters, 6),
    }.join(QLatin1Char(' '));
}

// -----------------------------------------------------------------------------------------------------------
// Écrit toutes les pistes du dépôt dans le fichier _Runway.txt.
NavDataWriter::FileResult NavDataWriter::writeRunways(const ProjectRepository& repo, const QDir& dir) const
{
    QStringList lines;
    for (const RunwayId rid : repo.runways().order()) {
        const Runway* r = repo.runways().find(rid);
        if (!r)
            continue;
        lines << formatRunwayLine(rid, *r);
    }
    return writeLines(lines, QStringLiteral("_Runway.txt"), dir);
}

// -----------------------------------------------------------------------------------------------------------
// Formate une séquence de legs en une ligne texte au format NavData.
QString NavDataWriter::formatLegSequenceLine(LegSequenceId id, const LegSequence& ls)
{
    return QStringList{
        QStringLiteral("LEGSEQUENCE"),
        fmt::id(id),
        fmt::quoted(ls.ident),
        QString::number(ls.sequenceTypeRaw),
        fmt::fixed(ls.transitionInMeters, 6),
    }.join(QLatin1Char(' '));
}

// -----------------------------------------------------------------------------------------------------------
// Écrit toutes les séquences de legs du dépôt dans _LegSequence.txt.
NavDataWriter::FileResult NavDataWriter::writeLegSequences(const ProjectRepository& repo, const QDir& dir) const
{
    QStringList lines;
    for (const LegSequenceId lsid : repo.legSequences().order()) {
        const LegSequence* ls = repo.legSequences().find(lsid);
        if (!ls)
            continue;
        lines << formatLegSequenceLine(lsid, *ls);
    }
    return writeLines(lines, QStringLiteral("_LegSequence.txt"), dir);
}

// -----------------------------------------------------------------------------------------------------------
// Formate un leg en une ligne texte au format de fichier NavData.
QString NavDataWriter::formatLegLine(LegId id, const Leg& l)
{
    return QStringList{
        QStringLiteral("LEG"),
        fmt::id(id),
        QString::number(fmt::packLegCode(l.code)),
        fmt::id(l.legSequenceId),
        fmt::id(l.pointId),
        fmt::flagsToInt(l.pointUsage),
        fmt::fixed(l.course, 6),
        fmt::fixed(l.distanceInMeters, 6),
        fmt::id(l.navaidId),
        fmt::fixed(l.navaidCourse, 6),
        fmt::fixed(l.navaidDistanceInMeters, 6),
        fmt::fixed(l.altitudeLimitMinInMeters, 6),
        fmt::fixed(l.altitudeLimitMaxInMeters, 6),
        fmt::fixed(l.airSpeedLimit, 6),
        fmt::fixed(l.path, 6),
        QString::number(l.turnDir),
        fmt::fixed(l.rnpInMeters, 6),
    }.join(QLatin1Char(' '));
}

// -----------------------------------------------------------------------------------------------------------
// Écrit tous les legs du dépôt dans le fichier _Leg.txt.
NavDataWriter::FileResult NavDataWriter::writeLegs(const ProjectRepository& repo, const QDir& dir) const
{
    QStringList lines;
    for (const LegId lid : repo.legs().order()) {
        const Leg* l = repo.legs().find(lid);
        if (!l)
            continue;
        lines << formatLegLine(lid, *l);
    }
    return writeLines(lines, QStringLiteral("_Leg.txt"), dir);
}

// -----------------------------------------------------------------------------------------------------------
// Formate une approche en une ligne texte au format de fichier NavData.
QString NavDataWriter::formatApproachLine(ApproachId id, const Approach& a)
{
    return QStringList{
        QStringLiteral("APPROACH"),
        fmt::id(id),
        fmt::id(a.runwayId),
        fmt::id(a.legSequenceId),
        fmt::fixed(a.decisionHeightInMeters, 6),
        fmt::fixed(a.minimumDescentInMeters, 6),
    }.join(QLatin1Char(' '));
}

// -----------------------------------------------------------------------------------------------------------
// Écrit toutes les approches du dépôt dans le fichier _Approach.txt.
NavDataWriter::FileResult NavDataWriter::writeApproaches(const ProjectRepository& repo, const QDir& dir) const
{
    QStringList lines;
    for (const ApproachId apid : repo.approaches().order()) {
        const Approach* a = repo.approaches().find(apid);
        if (!a)
            continue;
        lines << formatApproachLine(apid, *a);
    }
    return writeLines(lines, QStringLiteral("_Approach.txt"), dir);
}

// -----------------------------------------------------------------------------------------------------------
// Formate une transition d'approche en une ligne texte au format NavData.
QString NavDataWriter::formatApproachTransitionLine(ApproachTransitionId id, const ApproachTransition& t)
{
    return QStringList{
        QStringLiteral("APPROACHTRANSITION"),
        fmt::id(id),
        fmt::id(t.approachId),
        fmt::id(t.legSequenceId),
    }.join(QLatin1Char(' '));
}

// -----------------------------------------------------------------------------------------------------------
// Écrit toutes les transitions d'approche dans _ApproachTransition.txt.
NavDataWriter::FileResult NavDataWriter::writeApproachTransitions(const ProjectRepository& repo, const QDir& dir) const
{
    QStringList lines;
    for (const ApproachTransitionId tid : repo.approachTransitions().order()) {
        const ApproachTransition* t = repo.approachTransitions().find(tid);
        if (!t)
            continue;
        lines << formatApproachTransitionLine(tid, *t);
    }
    return writeLines(lines, QStringLiteral("_ApproachTransition.txt"), dir);
}

// -----------------------------------------------------------------------------------------------------------
// Formate une procédure en une ligne texte au format de fichier NavData.
QString NavDataWriter::formatProcedureLine(ProcedureId id, const Procedure& p)
{
    return QStringList{
        QStringLiteral("PROCEDURE"),
        fmt::id(id),
        fmt::id(p.airportId),
        fmt::id(p.legSequenceId),
    }.join(QLatin1Char(' '));
}

// -----------------------------------------------------------------------------------------------------------
// Écrit toutes les procédures SID ou STAR dans le fichier fourni.
NavDataWriter::FileResult NavDataWriter::writeProcedures(const ProjectRepository& repo, ProcedureKind kind,
                                                          const QString& fileName, const QDir& dir) const
{
    QStringList lines;
    const auto& table = repo.procedures(kind);
    for (const ProcedureId pid : table.order()) {
        const Procedure* p = table.find(pid);
        if (!p)
            continue;
        lines << formatProcedureLine(pid, *p);
    }
    return writeLines(lines, fileName, dir);
}

// -----------------------------------------------------------------------------------------------------------
// Formate une transition de procédure en ligne texte au format NavData.
QString NavDataWriter::formatProcedureTransitionLine(ProcedureTransitionId id, const ProcedureTransition& t)
{
    return QStringList{
        QStringLiteral("PROCEDURETRANSITION"),
        fmt::id(id),
        fmt::id(t.procedureId),
        fmt::id(t.legSequenceId),
    }.join(QLatin1Char(' '));
}

// -----------------------------------------------------------------------------------------------------------
// Écrit toutes les transitions de procédures SID/STAR dans le fichier fourni.
NavDataWriter::FileResult NavDataWriter::writeProcedureTransitions(const ProjectRepository& repo, ProcedureKind kind,
                                                                   const QString& fileName, const QDir& dir) const
{
    QStringList lines;
    const auto& table = repo.procedureTransitions(kind);
    for (const ProcedureTransitionId tid : table.order()) {
        const ProcedureTransition* t = table.find(tid);
        if (!t)
            continue;
        lines << formatProcedureTransitionLine(tid, *t);
    }
    return writeLines(lines, fileName, dir);
}

// -----------------------------------------------------------------------------------------------------------
// Formate une transition piste/procédure en ligne texte au format NavData.
QString NavDataWriter::formatRunwayProcedureTransitionLine(RunwayProcedureTransitionId id, const RunwayProcedureTransition& t)
{
    return QStringList{
        QStringLiteral("RUNWAYPROCEDURETRANSITION"),
        fmt::id(id),
        fmt::id(t.runwayId),
        fmt::id(t.procedureId),
        fmt::id(t.engineOutProcedureId),
        fmt::id(t.legSequenceId),
    }.join(QLatin1Char(' '));
}

// -----------------------------------------------------------------------------------------------------------
// Écrit toutes les transitions piste/procédure SID/STAR dans le fichier fourni.
NavDataWriter::FileResult NavDataWriter::writeRunwayProcedureTransitions(const ProjectRepository& repo, ProcedureKind kind,
                                                                         const QString& fileName, const QDir& dir) const
{
    QStringList lines;
    const auto& table = repo.runwayProcedureTransitions(kind);
    for (const RunwayProcedureTransitionId tid : table.order()) {
        const RunwayProcedureTransition* t = table.find(tid);
        if (!t)
            continue;
        lines << formatRunwayProcedureTransitionLine(tid, *t);
    }
    return writeLines(lines, fileName, dir);
}

// -----------------------------------------------------------------------------------------------------------
// Écrit successivement tous les fichiers de données NavData du dépôt.
QVector<NavDataWriter::FileResult> NavDataWriter::writeAll(const ProjectRepository& repo, const QDir& outputDir) const
{
    QVector<FileResult> results;
    results << writeAirports(repo, outputDir);
    results << writeRunways(repo, outputDir);
    results << writePoints(repo, outputDir);
    results << writeWaypoints(repo, outputDir);
    results << writeNavaids(repo, outputDir);
    results << writeLegSequences(repo, outputDir);
    results << writeLegs(repo, outputDir);
    results << writeApproaches(repo, outputDir);
    results << writeApproachTransitions(repo, outputDir);
    results << writeProcedures(repo, ProcedureKind::Sid, QStringLiteral("_ProcedureSID.txt"), outputDir);
    results << writeProcedures(repo, ProcedureKind::Star, QStringLiteral("_ProcedureSTAR.txt"), outputDir);
    results << writeProcedureTransitions(repo, ProcedureKind::Sid, QStringLiteral("_ProcTransSID.txt"), outputDir);
    results << writeProcedureTransitions(repo, ProcedureKind::Star, QStringLiteral("_ProcTransSTAR.txt"), outputDir);
    results << writeRunwayProcedureTransitions(repo, ProcedureKind::Sid, QStringLiteral("_RunProcTransSID.txt"), outputDir);
    results << writeRunwayProcedureTransitions(repo, ProcedureKind::Star, QStringLiteral("_RunProcTransSTAR.txt"), outputDir);
    return results;
}

} // namespace navstud::writer
