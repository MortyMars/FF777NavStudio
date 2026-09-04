#include "ProjectStore.h"

#include <QDateTime>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace navstud::persistence {

using namespace navstud::model;
using namespace navstud::userdata;

namespace {

// ============================================================================
// Schéma — une table par structure de saisie, plus `projects`. Les paires
// Sid/Star (Procedure, ProcedureTransition, RunwayProcedureTransition) ont
// chacune DEUX tables SQL indépendantes, cohérent avec les deux EntityTable
// indépendantes côté C++ (cf. UserProject.h) — pas de colonne `kind` qui
// réintroduirait l'ambiguïté déjà corrigée côté modèle.
// ============================================================================
// -----------------------------------------------------------------------------------------------------------
// Fournit la liste des instructions CREATE TABLE définissant le schéma SQL.
const QStringList& schemaStatements()
{
    static const QStringList statements = {
        QStringLiteral(R"sql(
            CREATE TABLE IF NOT EXISTS projects (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                name TEXT UNIQUE NOT NULL,
                si_point INTEGER NOT NULL, si_waypoint INTEGER NOT NULL,
                si_navaid INTEGER NOT NULL, si_airport INTEGER NOT NULL,
                si_runway INTEGER NOT NULL, si_leg_sequence INTEGER NOT NULL,
                si_leg INTEGER NOT NULL, si_approach INTEGER NOT NULL,
                si_approach_transition INTEGER NOT NULL,
                si_sid_procedure INTEGER NOT NULL, si_star_procedure INTEGER NOT NULL,
                si_sid_procedure_transition INTEGER NOT NULL,
                si_star_procedure_transition INTEGER NOT NULL,
                si_sid_runway_procedure_transition INTEGER NOT NULL,
                si_star_runway_procedure_transition INTEGER NOT NULL,
                updated_at TEXT NOT NULL
            )
        )sql"),
        QStringLiteral(R"sql(
            CREATE TABLE IF NOT EXISTS user_points (
                project_id INTEGER NOT NULL, id INTEGER NOT NULL,
                ident TEXT, latitude REAL, longitude REAL, mag_var REAL,
                hold_course REAL, hold_dist_in_meters REAL, hold_time REAL, hold_side INTEGER,
                PRIMARY KEY (project_id, id)
            )
        )sql"),
        QStringLiteral(R"sql(
            CREATE TABLE IF NOT EXISTS user_waypoints (
                project_id INTEGER NOT NULL, id INTEGER NOT NULL,
                point_ident TEXT,
                PRIMARY KEY (project_id, id)
            )
        )sql"),
        QStringLiteral(R"sql(
            CREATE TABLE IF NOT EXISTS user_navaids (
                project_id INTEGER NOT NULL, id INTEGER NOT NULL,
                type TEXT, point_ident TEXT, associated_navaid_ident TEXT,
                elevation_in_meters REAL, declination REAL, frequency_mhz_times100 INTEGER,
                category TEXT, course REAL, angle REAL, runway_ident TEXT,
                PRIMARY KEY (project_id, id)
            )
        )sql"),
        QStringLiteral(R"sql(
            CREATE TABLE IF NOT EXISTS user_airports (
                project_id INTEGER NOT NULL, id INTEGER NOT NULL,
                point_ident TEXT, elevation_in_meters REAL, limit_speed_in_mps REAL,
                limit_altitude_in_meters REAL, transition_altitude_in_meters REAL,
                transition_level_in_meters REAL,
                PRIMARY KEY (project_id, id)
            )
        )sql"),
        QStringLiteral(R"sql(
            CREATE TABLE IF NOT EXISTS user_runways (
                project_id INTEGER NOT NULL, id INTEGER NOT NULL,
                airport_ident TEXT, threshold_ident TEXT, elevation_in_meters REAL,
                gradient REAL, course REAL, length_in_meters REAL,
                displaced_in_meters REAL, stopway_in_meters REAL, cross_in_meters REAL,
                PRIMARY KEY (project_id, id)
            )
        )sql"),
        QStringLiteral(R"sql(
            CREATE TABLE IF NOT EXISTS user_leg_sequences (
                project_id INTEGER NOT NULL, id INTEGER NOT NULL,
                ident TEXT, ils_or_rnav TEXT, procedure_kind TEXT,
                altitude_level_trans_in_feet REAL,
                PRIMARY KEY (project_id, id)
            )
        )sql"),
        QStringLiteral(R"sql(
            CREATE TABLE IF NOT EXISTS user_legs (
                project_id INTEGER NOT NULL, id INTEGER NOT NULL,
                code_path TEXT, leg_sequence_ident TEXT, point_ident TEXT, wp_description TEXT,
                course REAL, distance_in_meters REAL, navaid_ident TEXT,
                navaid_course REAL, navaid_distance_in_meters REAL,
                altitude_limit_min_in_feet REAL, altitude_limit_max_in_feet REAL,
                air_speed_limit REAL, path REAL, turn_dir INTEGER, rnp_in_meters REAL,
                PRIMARY KEY (project_id, id)
            )
        )sql"),
        QStringLiteral(R"sql(
            CREATE TABLE IF NOT EXISTS user_approaches (
                project_id INTEGER NOT NULL, id INTEGER NOT NULL,
                runway_ident TEXT, leg_sequence_ident TEXT,
                decision_height_in_feet REAL, minimum_descent_in_feet REAL,
                PRIMARY KEY (project_id, id)
            )
        )sql"),
        QStringLiteral(R"sql(
            CREATE TABLE IF NOT EXISTS user_approach_transitions (
                project_id INTEGER NOT NULL, id INTEGER NOT NULL,
                approach_ident TEXT, leg_sequence_ident TEXT,
                PRIMARY KEY (project_id, id)
            )
        )sql"),
        QStringLiteral(R"sql(CREATE TABLE IF NOT EXISTS user_sid_procedures (
                project_id INTEGER NOT NULL, id INTEGER NOT NULL,
                airport_ident TEXT, leg_sequence_ident TEXT,
                PRIMARY KEY (project_id, id)
            )
        )sql"),
        QStringLiteral(R"sql(CREATE TABLE IF NOT EXISTS user_star_procedures (
                project_id INTEGER NOT NULL, id INTEGER NOT NULL,
                airport_ident TEXT, leg_sequence_ident TEXT,
                PRIMARY KEY (project_id, id)
            )
        )sql"),
        QStringLiteral(R"sql(CREATE TABLE IF NOT EXISTS user_sid_procedure_transitions (
                project_id INTEGER NOT NULL, id INTEGER NOT NULL,
                procedure_ident TEXT, leg_sequence_ident TEXT,
                PRIMARY KEY (project_id, id)
            )
        )sql"),
        QStringLiteral(R"sql(CREATE TABLE IF NOT EXISTS user_star_procedure_transitions (
                project_id INTEGER NOT NULL, id INTEGER NOT NULL,
                procedure_ident TEXT, leg_sequence_ident TEXT,
                PRIMARY KEY (project_id, id)
            )
        )sql"),
        QStringLiteral(R"sql(CREATE TABLE IF NOT EXISTS user_sid_runway_procedure_transitions (
                project_id INTEGER NOT NULL, id INTEGER NOT NULL,
                runway_ident TEXT, procedure_ident TEXT, leg_sequence_ident TEXT,
                PRIMARY KEY (project_id, id)
            )
        )sql"),
        QStringLiteral(R"sql(CREATE TABLE IF NOT EXISTS user_star_runway_procedure_transitions (
                project_id INTEGER NOT NULL, id INTEGER NOT NULL,
                runway_ident TEXT, procedure_ident TEXT, leg_sequence_ident TEXT,
                PRIMARY KEY (project_id, id)
            )
        )sql"),
    };
    return statements;
}

// ============================================================================
// Helpers génériques — factorisent le DELETE+INSERT et le SELECT communs à
// toutes les tables. Chaque structure ne fournit que ses noms de colonnes et
// deux petites fonctions de liaison (écriture) / lecture (relecture).
// ============================================================================

// -----------------------------------------------------------------------------------------------------------
// Sauvegarde une table d'entités en supprimant puis réinsérant ses lignes.
template <typename Tag, typename Entity, typename BindFn>
bool saveEntityTable(const QSqlDatabase& db, qint64 projectId, const QString& tableName,
                      const QStringList& columns, const EntityTable<Tag, Entity>& table,
                      BindFn bind, QString* errorMessage)
{
    QSqlQuery del(db);
    del.prepare(QStringLiteral("DELETE FROM %1 WHERE project_id = ?").arg(tableName));
    del.addBindValue(projectId);
    if (!del.exec()) {
        if (errorMessage) *errorMessage = del.lastError().text();
        return false;
    }

    QStringList placeholders;
    for (const QString& col : columns)
        placeholders << QLatin1Char(':') + col;

    QSqlQuery ins(db);
    ins.prepare(QStringLiteral("INSERT INTO %1 (project_id, id, %2) VALUES (:project_id, :id, %3)")
                    .arg(tableName, columns.join(QStringLiteral(", ")), placeholders.join(QStringLiteral(", "))));

    for (const Id<Tag> id : table.order()) {
        const Entity* e = table.find(id);
        if (!e)
            continue;
        ins.bindValue(QStringLiteral(":project_id"), projectId);
        ins.bindValue(QStringLiteral(":id"), id.value());
        bind(ins, *e);
        if (!ins.exec()) {
            if (errorMessage) *errorMessage = ins.lastError().text();
            return false;
        }
    }
    return true;
}

// -----------------------------------------------------------------------------------------------------------
// Recharge une table d'entités depuis la base en exécutant un SELECT.
template <typename Tag, typename Entity, typename RowFn>
bool loadEntityTable(const QSqlDatabase& db, qint64 projectId, const QString& tableName,
                      EntityTable<Tag, Entity>& table, RowFn rowFn, QString* errorMessage)
{
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT * FROM %1 WHERE project_id = ? ORDER BY id").arg(tableName));
    q.addBindValue(projectId);
    if (!q.exec()) {
        if (errorMessage) *errorMessage = q.lastError().text();
        return false;
    }
    while (q.next()) {
        const qint32 rawId = static_cast<qint32>(q.value(QStringLiteral("id")).toLongLong());
        table.add(rowFn(q), Id<Tag>(rawId));
    }
    return true;
}

// Un compteur simple suffit : ProjectStore n'est pas pensé pour une
// ouverture concurrente multi-thread à ce stade (usage CLI/UI séquentiel).
// À revoir si un jour plusieurs threads ouvrent des ProjectStore en parallèle.
// -----------------------------------------------------------------------------------------------------------
// Génère un nom de connexion SQLite unique par ProjectStore ouvert.
QString nextConnectionName()
{
    static int counter = 0;
    return QStringLiteral("FF777NavStudio_%1").arg(++counter);
}

} // namespace

// -----------------------------------------------------------------------------------------------------------
// Libère la connexion SQLite avant de retirer la connexion nommée enregistrée.
ProjectStore::~ProjectStore()
{
    if (mConnectionName.isEmpty())
        return;
    mDb = QSqlDatabase(); // libère la référence AVANT removeDatabase, sinon Qt avertit "connection still in use"
    QSqlDatabase::removeDatabase(mConnectionName);
}

// -----------------------------------------------------------------------------------------------------------
// Ouvre la base SQLite, crée le schéma si besoin et applique les migrations.
bool ProjectStore::open(const QString& sqlitePath, QString* errorMessage)
{
    mConnectionName = nextConnectionName();
    mDb = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), mConnectionName);
    mDb.setDatabaseName(sqlitePath);
    if (!mDb.open()) {
        if (errorMessage) *errorMessage = mDb.lastError().text();
        return false;
    }
    for (const QString& statement : schemaStatements()) {
        QSqlQuery q(mDb);
        if (!q.exec(statement)) {
            if (errorMessage) *errorMessage = q.lastError().text();
            return false;
        }
    }

    // Migration : les colonnes altitude_limit_min/max de user_legs
    // s'appelaient "..._in_meters" tant qu'elles contenaient effectivement
    // des mètres — renommées suite au passage de la saisie Leg en pieds
    // (cf. UserEntities.h). Idempotente : une base fraîchement créée ou
    // déjà migrée n'a plus jamais l'ancien nom de colonne, donc rien à
    // faire ; CREATE TABLE IF NOT EXISTS ci-dessus ne renomme jamais une
    // colonne d'une table déjà existante, d'où ce passage explicite.
    {
        QSqlQuery info(mDb);
        info.exec(QStringLiteral("PRAGMA table_info(user_legs)"));
        QSet<QString> columns;
        while (info.next())
            columns.insert(info.value(1).toString()); // colonne 1 du pragma = nom de colonne
        if (columns.contains(QStringLiteral("altitude_limit_min_in_meters"))) {
            QSqlQuery alter(mDb);
            alter.exec(QStringLiteral(
                "ALTER TABLE user_legs RENAME COLUMN altitude_limit_min_in_meters TO altitude_limit_min_in_feet"));
        }
        if (columns.contains(QStringLiteral("altitude_limit_max_in_meters"))) {
            QSqlQuery alter(mDb);
            alter.exec(QStringLiteral(
                "ALTER TABLE user_legs RENAME COLUMN altitude_limit_max_in_meters TO altitude_limit_max_in_feet"));
        }
    }

    return true;
}

// -----------------------------------------------------------------------------------------------------------
// Liste les projets existants triés par date de modification décroissante.
QVector<ProjectSummary> ProjectStore::listProjects() const
{
    QVector<ProjectSummary> result;
    QSqlQuery q(mDb);
    q.exec(QStringLiteral("SELECT id, name, updated_at FROM projects ORDER BY updated_at DESC"));
    while (q.next()) {
        result.push_back(ProjectSummary{
            q.value(0).toLongLong(), q.value(1).toString(), q.value(2).toString() });
    }
    return result;
}

// -----------------------------------------------------------------------------------------------------------
// Crée un nouveau projet et retourne son identifiant, ou -1 en cas d'échec.
qint64 ProjectStore::createProject(const QString& name, const StartingIndices& si, QString* errorMessage)
{
    QSqlQuery q(mDb);
    q.prepare(QStringLiteral(R"sql(
        INSERT INTO projects (
            name, si_point, si_waypoint, si_navaid, si_airport, si_runway,
            si_leg_sequence, si_leg, si_approach, si_approach_transition,
            si_sid_procedure, si_star_procedure,
            si_sid_procedure_transition, si_star_procedure_transition,
            si_sid_runway_procedure_transition, si_star_runway_procedure_transition,
            updated_at
        ) VALUES (
            :name, :point, :waypoint, :navaid, :airport, :runway,
            :legSequence, :leg, :approach, :approachTransition,
            :sidProcedure, :starProcedure,
            :sidProcedureTransition, :starProcedureTransition,
            :sidRunwayProcedureTransition, :starRunwayProcedureTransition,
            :updatedAt
        )
    )sql"));
    q.bindValue(QStringLiteral(":name"), name);
    q.bindValue(QStringLiteral(":point"), si.point);
    q.bindValue(QStringLiteral(":waypoint"), si.waypoint);
    q.bindValue(QStringLiteral(":navaid"), si.navaid);
    q.bindValue(QStringLiteral(":airport"), si.airport);
    q.bindValue(QStringLiteral(":runway"), si.runway);
    q.bindValue(QStringLiteral(":legSequence"), si.legSequence);
    q.bindValue(QStringLiteral(":leg"), si.leg);
    q.bindValue(QStringLiteral(":approach"), si.approach);
    q.bindValue(QStringLiteral(":approachTransition"), si.approachTransition);
    q.bindValue(QStringLiteral(":sidProcedure"), si.sidProcedure);
    q.bindValue(QStringLiteral(":starProcedure"), si.starProcedure);
    q.bindValue(QStringLiteral(":sidProcedureTransition"), si.sidProcedureTransition);
    q.bindValue(QStringLiteral(":starProcedureTransition"), si.starProcedureTransition);
    q.bindValue(QStringLiteral(":sidRunwayProcedureTransition"), si.sidRunwayProcedureTransition);
    q.bindValue(QStringLiteral(":starRunwayProcedureTransition"), si.starRunwayProcedureTransition);
    q.bindValue(QStringLiteral(":updatedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));

    if (!q.exec()) {
        if (errorMessage) *errorMessage = q.lastError().text(); // ex. contrainte UNIQUE(name) violée
        return -1;
    }
    return q.lastInsertId().toLongLong();
}

// -----------------------------------------------------------------------------------------------------------
// Met à jour les indices de départ d'un projet et sa date de modification.
bool ProjectStore::updateStartingIndices(qint64 projectId, const StartingIndices& si, QString* errorMessage)
{
    QSqlQuery q(mDb);
    q.prepare(QStringLiteral(R"sql(
        UPDATE projects SET
            si_point = :point, si_waypoint = :waypoint, si_navaid = :navaid,
            si_airport = :airport, si_runway = :runway,
            si_leg_sequence = :legSequence, si_leg = :leg,
            si_approach = :approach, si_approach_transition = :approachTransition,
            si_sid_procedure = :sidProcedure, si_star_procedure = :starProcedure,
            si_sid_procedure_transition = :sidProcedureTransition,
            si_star_procedure_transition = :starProcedureTransition,
            si_sid_runway_procedure_transition = :sidRunwayProcedureTransition,
            si_star_runway_procedure_transition = :starRunwayProcedureTransition,
            updated_at = :updatedAt
        WHERE id = :id
    )sql"));
    q.bindValue(QStringLiteral(":point"), si.point);
    q.bindValue(QStringLiteral(":waypoint"), si.waypoint);
    q.bindValue(QStringLiteral(":navaid"), si.navaid);
    q.bindValue(QStringLiteral(":airport"), si.airport);
    q.bindValue(QStringLiteral(":runway"), si.runway);
    q.bindValue(QStringLiteral(":legSequence"), si.legSequence);
    q.bindValue(QStringLiteral(":leg"), si.leg);
    q.bindValue(QStringLiteral(":approach"), si.approach);
    q.bindValue(QStringLiteral(":approachTransition"), si.approachTransition);
    q.bindValue(QStringLiteral(":sidProcedure"), si.sidProcedure);
    q.bindValue(QStringLiteral(":starProcedure"), si.starProcedure);
    q.bindValue(QStringLiteral(":sidProcedureTransition"), si.sidProcedureTransition);
    q.bindValue(QStringLiteral(":starProcedureTransition"), si.starProcedureTransition);
    q.bindValue(QStringLiteral(":sidRunwayProcedureTransition"), si.sidRunwayProcedureTransition);
    q.bindValue(QStringLiteral(":starRunwayProcedureTransition"), si.starRunwayProcedureTransition);
    q.bindValue(QStringLiteral(":updatedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    q.bindValue(QStringLiteral(":id"), projectId);

    if (!q.exec()) {
        if (errorMessage) *errorMessage = q.lastError().text();
        return false;
    }
    if (q.numRowsAffected() == 0) {
        if (errorMessage) *errorMessage = QStringLiteral("Projet id=%1 introuvable").arg(projectId);
        return false;
    }
    return true;
}

// -----------------------------------------------------------------------------------------------------------
// Recontruit un UserProject complet en rechargeant toutes ses tables.
std::optional<UserProject> ProjectStore::loadProject(qint64 projectId, QString* errorMessage) const
{
    QSqlQuery meta(mDb);
    meta.prepare(QStringLiteral("SELECT * FROM projects WHERE id = ?"));
    meta.addBindValue(projectId);
    if (!meta.exec() || !meta.next()) {
        if (errorMessage) *errorMessage = meta.lastError().text().isEmpty()
            ? QStringLiteral("Projet #%1 introuvable").arg(projectId) : meta.lastError().text();
        return std::nullopt;
    }

    StartingIndices si;
    si.point                          = meta.value(QStringLiteral("si_point")).toInt();
    si.waypoint                       = meta.value(QStringLiteral("si_waypoint")).toInt();
    si.navaid                         = meta.value(QStringLiteral("si_navaid")).toInt();
    si.airport                        = meta.value(QStringLiteral("si_airport")).toInt();
    si.runway                         = meta.value(QStringLiteral("si_runway")).toInt();
    si.legSequence                    = meta.value(QStringLiteral("si_leg_sequence")).toInt();
    si.leg                            = meta.value(QStringLiteral("si_leg")).toInt();
    si.approach                       = meta.value(QStringLiteral("si_approach")).toInt();
    si.approachTransition             = meta.value(QStringLiteral("si_approach_transition")).toInt();
    si.sidProcedure                   = meta.value(QStringLiteral("si_sid_procedure")).toInt();
    si.starProcedure                  = meta.value(QStringLiteral("si_star_procedure")).toInt();
    si.sidProcedureTransition         = meta.value(QStringLiteral("si_sid_procedure_transition")).toInt();
    si.starProcedureTransition        = meta.value(QStringLiteral("si_star_procedure_transition")).toInt();
    si.sidRunwayProcedureTransition   = meta.value(QStringLiteral("si_sid_runway_procedure_transition")).toInt();
    si.starRunwayProcedureTransition  = meta.value(QStringLiteral("si_star_runway_procedure_transition")).toInt();

    UserProject project(si);
    QString err;
    bool ok = true;

    ok &= loadEntityTable(mDb, projectId, QStringLiteral("user_points"), project.points(),
        [](QSqlQuery& q) {
            return UserPoint{
                q.value(QStringLiteral("ident")).toString(), q.value(QStringLiteral("latitude")).toDouble(),
                q.value(QStringLiteral("longitude")).toDouble(), q.value(QStringLiteral("mag_var")).toDouble(),
                q.value(QStringLiteral("hold_course")).toDouble(), q.value(QStringLiteral("hold_dist_in_meters")).toDouble(),
                q.value(QStringLiteral("hold_time")).toDouble(), static_cast<qint8>(q.value(QStringLiteral("hold_side")).toInt()) };
        }, &err);

    ok &= loadEntityTable(mDb, projectId, QStringLiteral("user_waypoints"), project.waypoints(),
        [](QSqlQuery& q) { return UserWaypoint{ q.value(QStringLiteral("point_ident")).toString() }; }, &err);

    ok &= loadEntityTable(mDb, projectId, QStringLiteral("user_navaids"), project.navaids(),
        [](QSqlQuery& q) {
            return UserNavaid{
                q.value(QStringLiteral("type")).toString(), q.value(QStringLiteral("point_ident")).toString(),
                q.value(QStringLiteral("associated_navaid_ident")).toString(), q.value(QStringLiteral("elevation_in_meters")).toDouble(),
                q.value(QStringLiteral("declination")).toDouble(), q.value(QStringLiteral("frequency_mhz_times100")).toUInt(),
                q.value(QStringLiteral("category")).toString(), q.value(QStringLiteral("course")).toDouble(),
                q.value(QStringLiteral("angle")).toDouble(), q.value(QStringLiteral("runway_ident")).toString() };
        }, &err);

    ok &= loadEntityTable(mDb, projectId, QStringLiteral("user_airports"), project.airports(),
        [](QSqlQuery& q) {
            return UserAirport{
                q.value(QStringLiteral("point_ident")).toString(), q.value(QStringLiteral("elevation_in_meters")).toDouble(),
                q.value(QStringLiteral("limit_speed_in_mps")).toDouble(), q.value(QStringLiteral("limit_altitude_in_meters")).toDouble(),
                q.value(QStringLiteral("transition_altitude_in_meters")).toDouble(), q.value(QStringLiteral("transition_level_in_meters")).toDouble() };
        }, &err);

    ok &= loadEntityTable(mDb, projectId, QStringLiteral("user_runways"), project.runways(),
        [](QSqlQuery& q) {
            return UserRunway{
                q.value(QStringLiteral("airport_ident")).toString(), q.value(QStringLiteral("threshold_ident")).toString(),
                q.value(QStringLiteral("elevation_in_meters")).toDouble(), q.value(QStringLiteral("gradient")).toDouble(),
                q.value(QStringLiteral("course")).toDouble(), q.value(QStringLiteral("length_in_meters")).toDouble(),
                q.value(QStringLiteral("displaced_in_meters")).toDouble(), q.value(QStringLiteral("stopway_in_meters")).toDouble(),
                q.value(QStringLiteral("cross_in_meters")).toDouble() };
        }, &err);

    ok &= loadEntityTable(mDb, projectId, QStringLiteral("user_leg_sequences"), project.legSequences(),
        [](QSqlQuery& q) {
            return UserLegSequence{
                q.value(QStringLiteral("ident")).toString(), q.value(QStringLiteral("ils_or_rnav")).toString(),
                q.value(QStringLiteral("procedure_kind")).toString(), q.value(QStringLiteral("altitude_level_trans_in_feet")).toDouble() };
        }, &err);

    ok &= loadEntityTable(mDb, projectId, QStringLiteral("user_legs"), project.legs(),
        [](QSqlQuery& q) {
            return UserLeg{
                q.value(QStringLiteral("code_path")).toString(), q.value(QStringLiteral("leg_sequence_ident")).toString(),
                q.value(QStringLiteral("point_ident")).toString(), q.value(QStringLiteral("wp_description")).toString(),
                q.value(QStringLiteral("course")).toDouble(), q.value(QStringLiteral("distance_in_meters")).toDouble(),
                q.value(QStringLiteral("navaid_ident")).toString(), q.value(QStringLiteral("navaid_course")).toDouble(),
                q.value(QStringLiteral("navaid_distance_in_meters")).toDouble(), q.value(QStringLiteral("altitude_limit_min_in_feet")).toDouble(),
                q.value(QStringLiteral("altitude_limit_max_in_feet")).toDouble(), q.value(QStringLiteral("air_speed_limit")).toDouble(),
                q.value(QStringLiteral("path")).toDouble(), static_cast<qint8>(q.value(QStringLiteral("turn_dir")).toInt()),
                q.value(QStringLiteral("rnp_in_meters")).toDouble() };
        }, &err);

    ok &= loadEntityTable(mDb, projectId, QStringLiteral("user_approaches"), project.approaches(),
        [](QSqlQuery& q) {
            return UserApproach{
                q.value(QStringLiteral("runway_ident")).toString(), q.value(QStringLiteral("leg_sequence_ident")).toString(),
                q.value(QStringLiteral("decision_height_in_feet")).toDouble(), q.value(QStringLiteral("minimum_descent_in_feet")).toDouble() };
        }, &err);

    ok &= loadEntityTable(mDb, projectId, QStringLiteral("user_approach_transitions"), project.approachTransitions(),
        [](QSqlQuery& q) {
            return UserApproachTransition{
                q.value(QStringLiteral("approach_ident")).toString(), q.value(QStringLiteral("leg_sequence_ident")).toString() };
        }, &err);

    auto procedureRow = [](QSqlQuery& q) {
        return UserProcedure{
            q.value(QStringLiteral("airport_ident")).toString(), q.value(QStringLiteral("leg_sequence_ident")).toString() };
    };
    ok &= loadEntityTable(mDb, projectId, QStringLiteral("user_sid_procedures"), project.sidProcedures(), procedureRow, &err);
    ok &= loadEntityTable(mDb, projectId, QStringLiteral("user_star_procedures"), project.starProcedures(), procedureRow, &err);

    auto procedureTransitionRow = [](QSqlQuery& q) {
        return UserProcedureTransition{
            q.value(QStringLiteral("procedure_ident")).toString(), q.value(QStringLiteral("leg_sequence_ident")).toString() };
    };
    ok &= loadEntityTable(mDb, projectId, QStringLiteral("user_sid_procedure_transitions"), project.sidProcedureTransitions(), procedureTransitionRow, &err);
    ok &= loadEntityTable(mDb, projectId, QStringLiteral("user_star_procedure_transitions"), project.starProcedureTransitions(), procedureTransitionRow, &err);

    auto rptRow = [](QSqlQuery& q) {
        return UserRunwayProcedureTransition{
            q.value(QStringLiteral("runway_ident")).toString(), q.value(QStringLiteral("procedure_ident")).toString(),
            q.value(QStringLiteral("leg_sequence_ident")).toString() };
    };
    ok &= loadEntityTable(mDb, projectId, QStringLiteral("user_sid_runway_procedure_transitions"), project.sidRunwayProcedureTransitions(), rptRow, &err);
    ok &= loadEntityTable(mDb, projectId, QStringLiteral("user_star_runway_procedure_transitions"), project.starRunwayProcedureTransitions(), rptRow, &err);

    if (!ok) {
        if (errorMessage) *errorMessage = err;
        return std::nullopt;
    }
    return project;
}

// -----------------------------------------------------------------------------------------------------------
// Sauvegarde toutes les tables d'un projet en une seule transaction atomique.
bool ProjectStore::saveProject(qint64 projectId, const UserProject& project, QString* errorMessage)
{
    if (!mDb.transaction()) {
        if (errorMessage) *errorMessage = mDb.lastError().text();
        return false;
    }

    bool ok = true;
    QString err;

    ok &= saveEntityTable(mDb, projectId, QStringLiteral("user_points"),
        { QStringLiteral("ident"), QStringLiteral("latitude"), QStringLiteral("longitude"), QStringLiteral("mag_var"),
          QStringLiteral("hold_course"), QStringLiteral("hold_dist_in_meters"), QStringLiteral("hold_time"), QStringLiteral("hold_side") },
        project.points(),
        [](QSqlQuery& q, const UserPoint& p) {
            q.bindValue(QStringLiteral(":ident"), p.ident);
            q.bindValue(QStringLiteral(":latitude"), p.latitude);
            q.bindValue(QStringLiteral(":longitude"), p.longitude);
            q.bindValue(QStringLiteral(":mag_var"), p.magVar);
            q.bindValue(QStringLiteral(":hold_course"), p.holdCourse);
            q.bindValue(QStringLiteral(":hold_dist_in_meters"), p.holdDistInMeters);
            q.bindValue(QStringLiteral(":hold_time"), p.holdTime);
            q.bindValue(QStringLiteral(":hold_side"), p.holdSide);
        }, &err);

    ok &= saveEntityTable(mDb, projectId, QStringLiteral("user_waypoints"),
        { QStringLiteral("point_ident") }, project.waypoints(),
        [](QSqlQuery& q, const UserWaypoint& w) { q.bindValue(QStringLiteral(":point_ident"), w.pointIdent); }, &err);

    ok &= saveEntityTable(mDb, projectId, QStringLiteral("user_navaids"),
        { QStringLiteral("type"), QStringLiteral("point_ident"), QStringLiteral("associated_navaid_ident"),
          QStringLiteral("elevation_in_meters"), QStringLiteral("declination"), QStringLiteral("frequency_mhz_times100"),
          QStringLiteral("category"), QStringLiteral("course"), QStringLiteral("angle"), QStringLiteral("runway_ident") },
        project.navaids(),
        [](QSqlQuery& q, const UserNavaid& n) {
            q.bindValue(QStringLiteral(":type"), n.type);
            q.bindValue(QStringLiteral(":point_ident"), n.pointIdent);
            q.bindValue(QStringLiteral(":associated_navaid_ident"), n.associatedNavaidIdent);
            q.bindValue(QStringLiteral(":elevation_in_meters"), n.elevationInMeters);
            q.bindValue(QStringLiteral(":declination"), n.declination);
            q.bindValue(QStringLiteral(":frequency_mhz_times100"), n.frequencyMHzTimes100);
            q.bindValue(QStringLiteral(":category"), n.category);
            q.bindValue(QStringLiteral(":course"), n.course);
            q.bindValue(QStringLiteral(":angle"), n.angle);
            q.bindValue(QStringLiteral(":runway_ident"), n.runwayIdent);
        }, &err);

    ok &= saveEntityTable(mDb, projectId, QStringLiteral("user_airports"),
        { QStringLiteral("point_ident"), QStringLiteral("elevation_in_meters"), QStringLiteral("limit_speed_in_mps"),
          QStringLiteral("limit_altitude_in_meters"), QStringLiteral("transition_altitude_in_meters"), QStringLiteral("transition_level_in_meters") },
        project.airports(),
        [](QSqlQuery& q, const UserAirport& a) {
            q.bindValue(QStringLiteral(":point_ident"), a.pointIdent);
            q.bindValue(QStringLiteral(":elevation_in_meters"), a.elevationInMeters);
            q.bindValue(QStringLiteral(":limit_speed_in_mps"), a.limitSpeedInMetersPerSec);
            q.bindValue(QStringLiteral(":limit_altitude_in_meters"), a.limitAltitudeInMeters);
            q.bindValue(QStringLiteral(":transition_altitude_in_meters"), a.transitionAltitudeInMeters);
            q.bindValue(QStringLiteral(":transition_level_in_meters"), a.transitionLevelInMeters);
        }, &err);

    ok &= saveEntityTable(mDb, projectId, QStringLiteral("user_runways"),
        { QStringLiteral("airport_ident"), QStringLiteral("threshold_ident"), QStringLiteral("elevation_in_meters"),
          QStringLiteral("gradient"), QStringLiteral("course"), QStringLiteral("length_in_meters"),
          QStringLiteral("displaced_in_meters"), QStringLiteral("stopway_in_meters"), QStringLiteral("cross_in_meters") },
        project.runways(),
        [](QSqlQuery& q, const UserRunway& r) {
            q.bindValue(QStringLiteral(":airport_ident"), r.airportIdent);
            q.bindValue(QStringLiteral(":threshold_ident"), r.thresholdIdent);
            q.bindValue(QStringLiteral(":elevation_in_meters"), r.elevationInMeters);
            q.bindValue(QStringLiteral(":gradient"), r.gradient);
            q.bindValue(QStringLiteral(":course"), r.course);
            q.bindValue(QStringLiteral(":length_in_meters"), r.lengthInMeters);
            q.bindValue(QStringLiteral(":displaced_in_meters"), r.displacedInMeters);
            q.bindValue(QStringLiteral(":stopway_in_meters"), r.stopwayInMeters);
            q.bindValue(QStringLiteral(":cross_in_meters"), r.crossInMeters);
        }, &err);

    ok &= saveEntityTable(mDb, projectId, QStringLiteral("user_leg_sequences"),
        { QStringLiteral("ident"), QStringLiteral("ils_or_rnav"), QStringLiteral("procedure_kind"), QStringLiteral("altitude_level_trans_in_feet") },
        project.legSequences(),
        [](QSqlQuery& q, const UserLegSequence& ls) {
            q.bindValue(QStringLiteral(":ident"), ls.ident);
            q.bindValue(QStringLiteral(":ils_or_rnav"), ls.ilsOrRnav);
            q.bindValue(QStringLiteral(":procedure_kind"), ls.procedureKind);
            q.bindValue(QStringLiteral(":altitude_level_trans_in_feet"), ls.altitudeLevelTransInFeet);
        }, &err);

    ok &= saveEntityTable(mDb, projectId, QStringLiteral("user_legs"),
        { QStringLiteral("code_path"), QStringLiteral("leg_sequence_ident"), QStringLiteral("point_ident"), QStringLiteral("wp_description"),
          QStringLiteral("course"), QStringLiteral("distance_in_meters"), QStringLiteral("navaid_ident"),
          QStringLiteral("navaid_course"), QStringLiteral("navaid_distance_in_meters"), QStringLiteral("altitude_limit_min_in_feet"),
          QStringLiteral("altitude_limit_max_in_feet"), QStringLiteral("air_speed_limit"), QStringLiteral("path"),
          QStringLiteral("turn_dir"), QStringLiteral("rnp_in_meters") },
        project.legs(),
        [](QSqlQuery& q, const UserLeg& l) {
            q.bindValue(QStringLiteral(":code_path"), l.codePath);
            q.bindValue(QStringLiteral(":leg_sequence_ident"), l.legSequenceIdent);
            q.bindValue(QStringLiteral(":point_ident"), l.pointIdent);
            q.bindValue(QStringLiteral(":wp_description"), l.wpDescription);
            q.bindValue(QStringLiteral(":course"), l.course);
            q.bindValue(QStringLiteral(":distance_in_meters"), l.distanceInMeters);
            q.bindValue(QStringLiteral(":navaid_ident"), l.navaidIdent);
            q.bindValue(QStringLiteral(":navaid_course"), l.navaidCourse);
            q.bindValue(QStringLiteral(":navaid_distance_in_meters"), l.navaidDistanceInMeters);
            q.bindValue(QStringLiteral(":altitude_limit_min_in_feet"), l.altitudeLimitMinInFeet);
            q.bindValue(QStringLiteral(":altitude_limit_max_in_feet"), l.altitudeLimitMaxInFeet);
            q.bindValue(QStringLiteral(":air_speed_limit"), l.airSpeedLimit);
            q.bindValue(QStringLiteral(":path"), l.path);
            q.bindValue(QStringLiteral(":turn_dir"), l.turnDir);
            q.bindValue(QStringLiteral(":rnp_in_meters"), l.rnpInMeters);
        }, &err);

    ok &= saveEntityTable(mDb, projectId, QStringLiteral("user_approaches"),
        { QStringLiteral("runway_ident"), QStringLiteral("leg_sequence_ident"), QStringLiteral("decision_height_in_feet"), QStringLiteral("minimum_descent_in_feet") },
        project.approaches(),
        [](QSqlQuery& q, const UserApproach& a) {
            q.bindValue(QStringLiteral(":runway_ident"), a.runwayIdent);
            q.bindValue(QStringLiteral(":leg_sequence_ident"), a.legSequenceIdent);
            q.bindValue(QStringLiteral(":decision_height_in_feet"), a.decisionHeightInFeet);
            q.bindValue(QStringLiteral(":minimum_descent_in_feet"), a.minimumDescentInFeet);
        }, &err);

    ok &= saveEntityTable(mDb, projectId, QStringLiteral("user_approach_transitions"),
        { QStringLiteral("approach_ident"), QStringLiteral("leg_sequence_ident") },
        project.approachTransitions(),
        [](QSqlQuery& q, const UserApproachTransition& t) {
            q.bindValue(QStringLiteral(":approach_ident"), t.approachIdent);
            q.bindValue(QStringLiteral(":leg_sequence_ident"), t.legSequenceIdent);
        }, &err);

    auto bindProcedure = [](QSqlQuery& q, const UserProcedure& p) {
        q.bindValue(QStringLiteral(":airport_ident"), p.airportIdent);
        q.bindValue(QStringLiteral(":leg_sequence_ident"), p.legSequenceIdent);
    };
    const QStringList procedureCols = { QStringLiteral("airport_ident"), QStringLiteral("leg_sequence_ident") };
    ok &= saveEntityTable(mDb, projectId, QStringLiteral("user_sid_procedures"), procedureCols, project.sidProcedures(), bindProcedure, &err);
    ok &= saveEntityTable(mDb, projectId, QStringLiteral("user_star_procedures"), procedureCols, project.starProcedures(), bindProcedure, &err);

    auto bindProcedureTransition = [](QSqlQuery& q, const UserProcedureTransition& t) {
        q.bindValue(QStringLiteral(":procedure_ident"), t.procedureIdent);
        q.bindValue(QStringLiteral(":leg_sequence_ident"), t.legSequenceIdent);
    };
    const QStringList procTransCols = { QStringLiteral("procedure_ident"), QStringLiteral("leg_sequence_ident") };
    ok &= saveEntityTable(mDb, projectId, QStringLiteral("user_sid_procedure_transitions"), procTransCols, project.sidProcedureTransitions(), bindProcedureTransition, &err);
    ok &= saveEntityTable(mDb, projectId, QStringLiteral("user_star_procedure_transitions"), procTransCols, project.starProcedureTransitions(), bindProcedureTransition, &err);

    auto bindRpt = [](QSqlQuery& q, const UserRunwayProcedureTransition& t) {
        q.bindValue(QStringLiteral(":runway_ident"), t.runwayIdent);
        q.bindValue(QStringLiteral(":procedure_ident"), t.procedureIdent);
        q.bindValue(QStringLiteral(":leg_sequence_ident"), t.legSequenceIdent);
    };
    const QStringList rptCols = { QStringLiteral("runway_ident"), QStringLiteral("procedure_ident"), QStringLiteral("leg_sequence_ident") };
    ok &= saveEntityTable(mDb, projectId, QStringLiteral("user_sid_runway_procedure_transitions"), rptCols, project.sidRunwayProcedureTransitions(), bindRpt, &err);
    ok &= saveEntityTable(mDb, projectId, QStringLiteral("user_star_runway_procedure_transitions"), rptCols, project.starRunwayProcedureTransitions(), bindRpt, &err);

    if (!ok) {
        mDb.rollback();
        if (errorMessage) *errorMessage = err;
        return false;
    }

    QSqlQuery touch(mDb);
    touch.prepare(QStringLiteral("UPDATE projects SET updated_at = :updatedAt WHERE id = :id"));
    touch.bindValue(QStringLiteral(":updatedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    touch.bindValue(QStringLiteral(":id"), projectId);
    touch.exec();

    if (!mDb.commit()) {
        if (errorMessage) *errorMessage = mDb.lastError().text();
        return false;
    }
    return true;
}

} // namespace navstud::persistence
