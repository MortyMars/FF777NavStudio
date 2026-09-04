// ============================================================================
// FF777NavStudioCli
// ------------------------------------------------------------------------------------------------------------------------------------------
//   validate <dossier>
//       Lit les 15 fichiers texte de <dossier>, lance le Validator, affiche
//       les diagnostics. Code de sortie 0 si aucune Severity::Error (les
//       Warning n'empêchent pas un code 0), 1 sinon, 2 en cas d'erreur
//       d'usage (dossier introuvable, arguments manquants).
//
//   export <dossier_source> <dossier_destination>
//       Lit <dossier_source>, affiche les diagnostics du Validator (sans
//       bloquer l'export dessus — à l'utilisateur de juger), régénère les
//       15 fichiers dans <dossier_destination>.
// ============================================================================

#include "NavDataReader.h"
#include "NavDataWriter.h"
#include "ProjectRepository.h"
#include "Validator.h"
#include "UserEntities.h"
#include "UserProject.h"
#include "UserToModelConverter.h"
#include "Regenerator.h"
#include "WorldIndexReader.h"
#include "ProjectStore.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QStringList>
#include <QTextStream>

using namespace navstud::model;
using namespace navstud::reader;
using namespace navstud::writer;
using namespace navstud::validator;
using namespace navstud::userdata;
using namespace navstud::conversion;
using namespace navstud::worldindex;
using namespace navstud::persistence;

namespace {

QTextStream& out()
{
    static QTextStream stream(stdout);
    return stream;
}

QTextStream& err()
{
    static QTextStream stream(stderr);
    return stream;
}

QString toString(Severity severity)
{
    switch (severity) {
    case Severity::Error:   return QStringLiteral("ERREUR ");
    case Severity::Warning: return QStringLiteral("ALERTE ");
    }
    return QStringLiteral("?");
}

QString toString(EntityKind kind)
{
    switch (kind) {
    case EntityKind::Point:                          return QStringLiteral("Point");
    case EntityKind::Waypoint:                       return QStringLiteral("Waypoint");
    case EntityKind::Navaid:                         return QStringLiteral("Navaid");
    case EntityKind::Airport:                        return QStringLiteral("Airport");
    case EntityKind::Runway:                         return QStringLiteral("Runway");
    case EntityKind::LegSequence:                     return QStringLiteral("LegSequence");
    case EntityKind::Leg:                            return QStringLiteral("Leg");
    case EntityKind::Approach:                       return QStringLiteral("Approach");
    case EntityKind::ApproachTransition:              return QStringLiteral("ApproachTransition");
    case EntityKind::SidProcedure:                    return QStringLiteral("Procedure(SID)");
    case EntityKind::StarProcedure:                   return QStringLiteral("Procedure(STAR)");
    case EntityKind::SidProcedureTransition:          return QStringLiteral("ProcedureTransition(SID)");
    case EntityKind::StarProcedureTransition:         return QStringLiteral("ProcedureTransition(STAR)");
    case EntityKind::SidRunwayProcedureTransition:    return QStringLiteral("RunwayProcedureTransition(SID)");
    case EntityKind::StarRunwayProcedureTransition:   return QStringLiteral("RunwayProcedureTransition(STAR)");
    }
    return QStringLiteral("?");
}

void printUsage()
{
    err() << QStringLiteral(
        "Usage :\n"
        "  FF777NavStudioCli validate <dossier>\n"
        "  FF777NavStudioCli export <dossier_source> <dossier_destination>\n"
        "  FF777NavStudioCli test-regen\n"
        "  FF777NavStudioCli test-regen-full\n"
        "  FF777NavStudioCli test-navaid\n"
        "  FF777NavStudioCli world-index <fichier_mondial>\n"
        "  FF777NavStudioCli test-persistence\n");
}

// Extrait les 15 index de départ du fichier mondial et les affiche —
// vérifie aussi qu'aucune section attendue n'a été manquée (fichier
// tronqué ou format inattendu).
int runWorldIndex(const QStringList& args)
{
    if (args.size() != 1) {
        printUsage();
        return 2;
    }

    const WorldIndexReader reader;
    const WorldIndexResult result = reader.readStartingIndices(args.at(0));

    if (!result.success) {
        err() << QStringLiteral("ECHEC : %1\n").arg(result.errorMessage);
        return 1;
    }

    const StartingIndices& si = result.indices;
    out() << QStringLiteral("point                          = %1\n").arg(si.point);
    out() << QStringLiteral("waypoint                       = %1\n").arg(si.waypoint);
    out() << QStringLiteral("navaid                         = %1\n").arg(si.navaid);
    out() << QStringLiteral("airport                        = %1\n").arg(si.airport);
    out() << QStringLiteral("runway                         = %1\n").arg(si.runway);
    out() << QStringLiteral("legSequence                    = %1\n").arg(si.legSequence);
    out() << QStringLiteral("leg                            = %1\n").arg(si.leg);
    out() << QStringLiteral("sidProcedure                   = %1\n").arg(si.sidProcedure);
    out() << QStringLiteral("starProcedure                  = %1\n").arg(si.starProcedure);
    out() << QStringLiteral("approach                       = %1\n").arg(si.approach);
    out() << QStringLiteral("sidProcedureTransition         = %1\n").arg(si.sidProcedureTransition);
    out() << QStringLiteral("starProcedureTransition        = %1\n").arg(si.starProcedureTransition);
    out() << QStringLiteral("approachTransition             = %1\n").arg(si.approachTransition);
    out() << QStringLiteral("sidRunwayProcedureTransition   = %1\n").arg(si.sidRunwayProcedureTransition);
    out() << QStringLiteral("starRunwayProcedureTransition  = %1\n").arg(si.starRunwayProcedureTransition);

    if (!result.missingSections.isEmpty()) {
        out() << QStringLiteral("ATTENTION — sections non trouvées (valeur par défaut 1 conservée) : %1\n")
            .arg(result.missingSections.join(QStringLiteral(", ")));
        return 1;
    }

    out() << QStringLiteral("Toutes les sections attendues ont été trouvées.\n");
    return 0;
}

void printRegenerationResult(const QString& label, const RegenerationResult& result)
{
    out() << QStringLiteral("=== Régénération : %1 ===\n").arg(label);
    for (const ConversionFailure& f : result.conversionFailures) {
        out() << QStringLiteral("  ALERTE [conversion] %1#%2 : %3\n")
            .arg(toString(f.kind)).arg(f.userId).arg(f.errors.join(QStringLiteral("; ")));
    }
    for (const Diagnostic& d : result.validationDiagnostics) {
        out() << QStringLiteral("  %1 [%2] %3#%4 : %5\n")
            .arg(toString(d.severity), d.ruleId, toString(d.ref.kind))
            .arg(d.ref.id)
            .arg(d.message);
    }
    out() << QStringLiteral("%1 alerte(s) au total — %2\n")
        .arg(result.alertCount())
        .arg(result.isValid() ? QStringLiteral("VALIDATION OK") : QStringLiteral("NON VALIDE"));
}

// Repository pré-rempli à la main (Point, Runway, un premier Navaid GS) pour
// tester convertNavaid + IdentResolver sans attendre les convertisseurs
// Airport/Runway (pas encore écrits) — même logique que les tout premiers
// smoke tests du Writer.
int runTestNavaid()
{
    ProjectRepository repo;

    const PointId point07  = repo.points().add(Point{ QStringLiteral("07"), 49.265336595, 3.143170287, 2.3, -1.0, -1.0, -1.0, 0 }, PointId(326256));
    const PointId pointGS07 = repo.points().add(Point{ QStringLiteral("GS07"), 49.264644688, 3.145580018, 2.3, -1.0, -1.0, -1.0, 0 }, PointId(326292));
    const PointId pointLFFA = repo.points().add(Point{ QStringLiteral("LFFA"), 49.266236111, 3.166947222, 2.3, -1.0, -1.0, -1.0, 0 }, PointId(326303));

    Airport airport;
    airport.pointId = pointLFFA;
    const AirportId airportId = repo.airports().add(airport, AirportId(17105));

    Runway runway;
    runway.airportId = airportId;
    runway.pointId    = point07;
    const RunwayId runwayId = repo.runways().add(runway, RunwayId(42408));

    // Navaid GS déjà présent, pour exercer la résolution à deux sauts vers
    // un AUTRE navaid ("GS/DME associé").
    Navaid gs;
    gs.type                  = NavaidTypeFlags(NavaidType::Gs);
    gs.pointId                = pointGS07;
    gs.frequencyMHzTimes100   = 10895;
    gs.category                = NavaidCategory::CategoryIII;
    gs.runwayId                = runwayId;
    repo.navaids().add(gs, NavaidId(23035));

    const IdentResolver resolver(repo);

    const QVector<UserNavaid> samples = {
        // OK : LOC référençant le GS existant comme navaid associé (2 sauts pt -> Navaid)
        UserNavaid{ QStringLiteral("LOC+GS+DME"), QStringLiteral("GS07"), QStringLiteral("GS07"),
                    135.0, 2.3, 10895, QStringLiteral("3"), 78.0, 0.0, QStringLiteral("07") },
        // OK : aucun navaid associé (-1)
        UserNavaid{ QStringLiteral("VOR"), QStringLiteral("07"), QStringLiteral("-1"),
                    135.0, 2.3, 11100, QStringLiteral("0"), 0.0, 0.0, QStringLiteral("07") },
        // ECHEC : piste introuvable
        UserNavaid{ QStringLiteral("DME"), QStringLiteral("07"), QStringLiteral("-1"),
                    135.0, 2.3, 11100, QStringLiteral("0"), 0.0, 0.0, QStringLiteral("99") },
    };

    for (const UserNavaid& sample : samples) {
        const ConversionResult<Navaid> result = convertNavaid(sample, resolver);
        if (result.isOk())
            out() << QStringLiteral("OK    type=\"%1\" pointIdent=\"%2\" -> runwayId=%3, distanceNavaidId=%4\n")
                .arg(sample.type, sample.pointIdent).arg(result.value().runwayId.value()).arg(result.value().distanceNavaidId.value());
        else
            out() << QStringLiteral("ECHEC type=\"%1\" pointIdent=\"%2\" : %3\n")
                .arg(sample.type, sample.pointIdent, result.errors().join(QStringLiteral("; ")));
    }

    return 0;
}

// Deux scénarios : un UserProject avec un ident trop long (Alerte, mais le
// reste régénère normalement), puis un UserProject propre (Validation OK).
// Ne teste que Point pour l'instant, cf. Regenerator::regenerate.
int runTestRegen()
{
    const Regenerator regenerator;

    {
        UserProject project;
        project.points().add(UserPoint{ QStringLiteral("07"), 49.265336595, 3.143170287, 2.3, -1.0, -1.0, -1.0, 0 });
        project.points().add(UserPoint{ QStringLiteral("TROPLONG"), 0.0, 0.0, 0.0, -1.0, -1.0, -1.0, 0 });
        printRegenerationResult(QStringLiteral("projet avec 1 ident invalide"), regenerator.regenerate(project));
    }
    {
        // Chaîne complète Point -> Airport -> Runway -> Navaid, sous-ensemble
        // LFFA réel, pour vérifier que le Regenerator enchaîne correctement
        // les étapes qui dépendent les unes des autres.
        UserProject project;
        project.points().add(UserPoint{ QStringLiteral("07"), 49.265336595, 3.143170287, 2.3, -1.0, -1.0, -1.0, 0 });
        project.points().add(UserPoint{ QStringLiteral("GS07"), 49.264644688, 3.145580018, 2.3, -1.0, -1.0, -1.0, 0 });
        project.points().add(UserPoint{ QStringLiteral("LFFA"), 49.266236111, 3.166947222, 2.3, -1.0, -1.0, -1.0, 0 });
        project.airports().add(UserAirport{ QStringLiteral("LFFA"), 135.0, -1.0, -1.0, 1524.0, 2134.0 });
        project.runways().add(UserRunway{ QStringLiteral("LFFA"), QStringLiteral("07"), 135.0, 0.0, 78.0, 3500.0, 0.0, 0.0, 15.0 });
        project.navaids().add(UserNavaid{ QStringLiteral("GS+DME non coloc"), QStringLiteral("GS07"), QStringLiteral("-1"),
                                           135.0, 2.3, 10895, QStringLiteral("3"), 0.0, 3.2, QStringLiteral("07") });
        printRegenerationResult(QStringLiteral("chaîne complète Point->Airport->Runway->Navaid"), regenerator.regenerate(project));
    }
    {
        // Même chaîne, mais avec une piste inexistante référencée par le
        // Navaid : Point/Airport/Runway doivent régénérer normalement,
        // seul le Navaid doit remonter en alerte.
        UserProject project;
        project.points().add(UserPoint{ QStringLiteral("07"), 49.265336595, 3.143170287, 2.3, -1.0, -1.0, -1.0, 0 });
        project.points().add(UserPoint{ QStringLiteral("GS07"), 49.264644688, 3.145580018, 2.3, -1.0, -1.0, -1.0, 0 });
        project.points().add(UserPoint{ QStringLiteral("LFFA"), 49.266236111, 3.166947222, 2.3, -1.0, -1.0, -1.0, 0 });
        project.airports().add(UserAirport{ QStringLiteral("LFFA"), 135.0, -1.0, -1.0, 1524.0, 2134.0 });
        project.runways().add(UserRunway{ QStringLiteral("LFFA"), QStringLiteral("07"), 135.0, 0.0, 78.0, 3500.0, 0.0, 0.0, 15.0 });
        project.navaids().add(UserNavaid{ QStringLiteral("GS+DME non coloc"), QStringLiteral("GS07"), QStringLiteral("-1"),
                                           135.0, 2.3, 10895, QStringLiteral("3"), 0.0, 3.2, QStringLiteral("99") });
        printRegenerationResult(QStringLiteral("Navaid avec piste introuvable"), regenerator.regenerate(project));
    }

    return 0;
}

// Test complet : les 11 structures enchaînées, sur un sous-ensemble LFFA
// réel (Point/Airport/Runway/Navaid déjà vus dans les tests précédents,
// complété par LegSequence "DPR07"/"RW07", le Leg d'ancrage, la Procedure
// SID et sa RunwayProcedureTransition). Les altitudes sont saisies en
// PIEDS (4000 ft -> 1219.199961 m pour DPR07, 443 ft -> 135.026396 m pour
// l'ancrage RW07), comme l'utilisateur les saisirait réellement — c'est le
// test le plus engageant à ce stade : il traverse toute la chaîne de
// résolution ET la conversion d'unité en une seule régénération.
// Sous-ensemble LFFA réel, réutilisé par test-regen-full et test-persistence.
UserProject buildLffaSubsetUserProject()
{
    UserProject project;

    project.points().add(UserPoint{ QStringLiteral("07"), 49.265336595, 3.143170287, 2.3, -1.0, -1.0, -1.0, 0 });
    project.points().add(UserPoint{ QStringLiteral("GS07"), 49.264644688, 3.145580018, 2.3, -1.0, -1.0, -1.0, 0 });
    project.points().add(UserPoint{ QStringLiteral("LFFA"), 49.266236111, 3.166947222, 2.3, -1.0, -1.0, -1.0, 0 });

    project.airports().add(UserAirport{ QStringLiteral("LFFA"), 135.0, -1.0, -1.0, 1524.0, 2134.0 });

    project.runways().add(UserRunway{ QStringLiteral("LFFA"), QStringLiteral("07"), 135.0, 0.0, 78.0, 3500.0, 0.0, 0.0, 15.0 });

    project.navaids().add(UserNavaid{ QStringLiteral("GS+DME non coloc"), QStringLiteral("GS07"), QStringLiteral("-1"),
                                       135.0, 2.3, 10895, QStringLiteral("3"), 0.0, 3.2, QStringLiteral("07") });

    // Séquence "cœur" du SID (ILS, 4000 ft -> 1219.199961 m attendu)
    project.legSequences().add(UserLegSequence{ QStringLiteral("DPR07"), QStringLiteral("ILS"), QStringLiteral("SID"), 4000.0 });
    // Séquence d'ancrage dédiée à la RunwayProcedureTransition (443 ft -> 135.026396 m attendu)
    project.legSequences().add(UserLegSequence{ QStringLiteral("RW07"), QStringLiteral("ILS"), QStringLiteral("APP TRANS"), 443.0 });

    // Leg d'ancrage : IF posé sur "07", décrit comme point de piste (G___ -> PointUsage::Runway)
    project.legs().add(UserLeg{ QStringLiteral("IF"), QStringLiteral("RW07"), QStringLiteral("07"), QStringLiteral("G___"),
                                 -1.0, 0.0, QStringLiteral("-1"), -1.0, -1.0, -1.0, -1.0, -1.0, 0.0, 0, 0.0 });

    project.sidProcedures().add(UserProcedure{ QStringLiteral("LFFA"), QStringLiteral("DPR07") });

    project.sidRunwayProcedureTransitions().add(
        UserRunwayProcedureTransition{ QStringLiteral("07"), QStringLiteral("DPR07"), QStringLiteral("RW07") });

    return project;
}

// Chaîne complète des 11 structures, sur un sous-ensemble LFFA réel. Les
// altitudes sont saisies en PIEDS (4000 ft -> 1219.199961 m pour DPR07,
// 443 ft -> 135.026396 m pour l'ancrage RW07), comme l'utilisateur les
// saisirait réellement — c'est le test le plus engageant à ce stade : il
// traverse toute la chaîne de résolution ET la conversion d'unité en une
// seule régénération.
int runTestRegenFull()
{
    const UserProject project = buildLffaSubsetUserProject();

    const Regenerator regenerator;
    const RegenerationResult result = regenerator.regenerate(project);
    printRegenerationResult(QStringLiteral("chaîne complète des 11 structures (sous-ensemble LFFA)"), result);

    // Vérification ciblée de la conversion pieds->mètres, pour repérer
    // immédiatement un écart de précision si jamais il y en avait un
    // (cf. le bug float/double découvert lors du smoke test du Writer).
    if (const LegSequence* dpr07 = result.repository.legSequences().find(LegSequenceId(1))) {
        out() << QStringLiteral("DPR07 transitionInMeters = %1 (attendu 1219.199961)\n")
            .arg(dpr07->transitionInMeters, 0, 'f', 6);
    }

    return result.isValid() ? 0 : 1;
}

// Cycle complet : crée un projet SQLite temporaire, y sauvegarde le
// sous-ensemble LFFA, ROUVRE un ProjectStore neuf sur le même fichier (pour
// être sûr de ne rien lire depuis un cache mémoire) et recharge. Compare le
// résultat de régénération avant/après pour confirmer que rien ne s'est
// perdu ni altéré en base — le test le plus engageant pour la persistance,
// symétrique de test-regen-full pour le moteur de conversion.
int runTestPersistence()
{
    const QString sqlitePath = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                            + QStringLiteral("/FF777NavStudio_persistence_test.sqlite");

    qDebug() << "Chemin de la BDD = " << sqlitePath;

    QFile::remove(sqlitePath); // repart d'un fichier neuf à chaque exécution du test

    const UserProject original = buildLffaSubsetUserProject();
    const Regenerator regenerator;
    const RegenerationResult beforeSave = regenerator.regenerate(original);
    printRegenerationResult(QStringLiteral("avant sauvegarde"), beforeSave);

    qint64 projectId = -1;
    {
        ProjectStore store;
        QString errorMessage;
        if (!store.open(sqlitePath, &errorMessage)) {
            out() << QStringLiteral("ECHEC ouverture : %1\n").arg(errorMessage);
            return 1;
        }
        projectId = store.createProject(QStringLiteral("LFFA"), StartingIndices(), &errorMessage);
        if (projectId < 0) {
            out() << QStringLiteral("ECHEC création projet : %1\n").arg(errorMessage);
            return 1;
        }
        if (!store.saveProject(projectId, original, &errorMessage)) {
            out() << QStringLiteral("ECHEC sauvegarde : %1\n").arg(errorMessage);
            return 1;
        }
        out() << QStringLiteral("Projet #%1 sauvegardé dans %2\n").arg(projectId).arg(sqlitePath);
    }

    // Nouveau ProjectStore, nouvelle ouverture — aucun état en mémoire réutilisé.
    UserProject reloaded;
    {
        ProjectStore store;
        QString errorMessage;
        if (!store.open(sqlitePath, &errorMessage)) {
            out() << QStringLiteral("ECHEC réouverture : %1\n").arg(errorMessage);
            return 1;
        }
        const std::optional<UserProject> loaded = store.loadProject(projectId, &errorMessage);
        if (!loaded) {
            out() << QStringLiteral("ECHEC rechargement : %1\n").arg(errorMessage);
            return 1;
        }
        reloaded = *loaded;
    }

    const RegenerationResult afterReload = regenerator.regenerate(reloaded);
    printRegenerationResult(QStringLiteral("après rechargement depuis SQLite"), afterReload);

    const bool sameEntityCounts =
        beforeSave.repository.points().count()  == afterReload.repository.points().count() &&
        beforeSave.repository.airports().count() == afterReload.repository.airports().count() &&
        beforeSave.repository.runways().count()  == afterReload.repository.runways().count() &&
        beforeSave.repository.navaids().count()  == afterReload.repository.navaids().count() &&
        beforeSave.repository.legSequences().count() == afterReload.repository.legSequences().count() &&
        beforeSave.repository.legs().count()     == afterReload.repository.legs().count() &&
        beforeSave.repository.sidProcedures().count() == afterReload.repository.sidProcedures().count() &&
        beforeSave.repository.sidRunwayProcedureTransitions().count() == afterReload.repository.sidRunwayProcedureTransitions().count();

    const LegSequence* before = beforeSave.repository.legSequences().find(LegSequenceId(1));
    const LegSequence* after  = afterReload.repository.legSequences().find(LegSequenceId(1));
    const bool sameTransitionValue = before && after && qFuzzyCompare(before->transitionInMeters, after->transitionInMeters);

    out() << QStringLiteral("Effectifs identiques avant/après : %1\n").arg(sameEntityCounts ? QStringLiteral("oui") : QStringLiteral("NON"));
    out() << QStringLiteral("DPR07 transitionInMeters identique : %1\n").arg(sameTransitionValue ? QStringLiteral("oui") : QStringLiteral("NON"));

    return (afterReload.isValid() && sameEntityCounts && sameTransitionValue) ? 0 : 1;
}

// Lit inputDir vers un ProjectRepository, affiche sur stderr toute ligne
// rejetée ou fichier illisible. hadErrors à vrai si au moins un souci de
// lecture est survenu (le repository retourné reste utilisable : le Reader
// ignore les lignes fautives plutôt que d'abandonner tout le fichier).
ProjectRepository readProject(const QDir& inputDir, bool& hadErrors)
{
    ProjectRepository repo;
    const NavDataReader reader;
    const QVector<NavDataReader::FileResult> results = reader.readAll(repo, inputDir);

    hadErrors = false;
    for (const NavDataReader::FileResult& r : results) {
        if (!r.success) {
            err() << QStringLiteral("ECHEC lecture %1\n").arg(r.fileName);
            hadErrors = true;
            continue;
        }
        for (const QString& e : r.errors) {
            err() << QStringLiteral("%1 : %2\n").arg(r.fileName, e);
            hadErrors = true;
        }
    }
    return repo;
}

// Valide repo, affiche chaque diagnostic sur stdout suivi d'un résumé.
// Retourne true si au moins un diagnostic est de sévérité Error (les
// Warning seuls ne font pas échouer l'appelant).
bool printValidation(const ProjectRepository& repo)
{
    const Validator validator;
    const QVector<Diagnostic> diagnostics = validator.validate(repo);

    int errorCount = 0;
    int warningCount = 0;
    for (const Diagnostic& d : diagnostics) {
        if (d.severity == Severity::Error)
            ++errorCount;
        else
            ++warningCount;
        out() << QStringLiteral("%1 [%2] %3#%4 : %5\n")
            .arg(toString(d.severity), d.ruleId, toString(d.ref.kind))
            .arg(d.ref.id)
            .arg(d.message);
    }
    out() << QStringLiteral("%1 erreur(s), %2 alerte(s)\n").arg(errorCount).arg(warningCount);
    return errorCount > 0;
}

int runValidate(const QStringList& args)
{
    if (args.size() != 1) {
        printUsage();
        return 2;
    }
    const QDir inputDir(args.at(0));
    if (!inputDir.exists()) {
        err() << QStringLiteral("Dossier introuvable : %1\n").arg(inputDir.absolutePath());
        return 2;
    }

    bool hadReadErrors = false;
    const ProjectRepository repo = readProject(inputDir, hadReadErrors);
    const bool hadValidationErrors = printValidation(repo);

    return (hadReadErrors || hadValidationErrors) ? 1 : 0;
}

int runExport(const QStringList& args)
{
    if (args.size() != 2) {
        printUsage();
        return 2;
    }
    const QDir inputDir(args.at(0));
    const QDir outputDir(args.at(1));
    if (!inputDir.exists()) {
        err() << QStringLiteral("Dossier source introuvable : %1\n").arg(inputDir.absolutePath());
        return 2;
    }

    bool hadReadErrors = false;
    const ProjectRepository repo = readProject(inputDir, hadReadErrors);
    const bool hadValidationErrors = printValidation(repo);

    const NavDataWriter writer;
    const QVector<NavDataWriter::FileResult> writeResults = writer.writeAll(repo, outputDir);
    int writeFailures = 0;
    for (const NavDataWriter::FileResult& r : writeResults) {
        if (!r.success) {
            err() << QStringLiteral("ECHEC écriture %1 : %2\n").arg(r.fileName, r.errorMessage);
            ++writeFailures;
        }
    }
    out() << QStringLiteral("Export terminé vers %1\n").arg(outputDir.absolutePath());

    return (hadReadErrors || writeFailures > 0 || hadValidationErrors) ? 1 : 0;
}

} // namespace

int main(int argc, char* argv[])
{
    const QCoreApplication app(argc, argv);

    const QStringList args = QCoreApplication::arguments();
    if (args.size() < 2) {
        printUsage();
        return 2;
    }

    const QString command = args.at(1);
    const QStringList rest = args.mid(2);

    if (command == QStringLiteral("validate"))
        return runValidate(rest);
    if (command == QStringLiteral("export"))
        return runExport(rest);
    if (command == QStringLiteral("test-regen"))
        return runTestRegen();
    if (command == QStringLiteral("test-regen-full"))
        return runTestRegenFull();
    if (command == QStringLiteral("test-navaid"))
        return runTestNavaid();
    if (command == QStringLiteral("world-index"))
        return runWorldIndex(rest);
    if (command == QStringLiteral("test-persistence"))
        return runTestPersistence();

    err() << QStringLiteral("Commande inconnue : %1\n").arg(command);
    printUsage();
    return 2;
}
