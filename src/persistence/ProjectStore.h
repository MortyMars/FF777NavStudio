#pragma once

// ============================================================================
// ProjectStore.h
// Persistance SQLite de userdata::UserProject. Multi-projets (une table
// `projects` avec un id par aéroport fictif), sauvegarde EXPLICITE (pas
// d'écriture automatique — l'appelant décide quand appeler save()).
//
// Les 15 index de départ (StartingIndices) sont capturés UNE SEULE FOIS à
// la création du projet (typiquement depuis WorldIndexReader) et persistés
// tels quels sur la ligne `projects` correspondante. Ils ne servent qu'à
// amorcer correctement l'IdAllocator des tables encore VIDES au rechargement
// — pour les tables qui contiennent déjà des entités, EntityTable::add(id)
// avance de lui-même l'allocateur au-delà du plus grand id chargé (cf.
// ProjectRepository.h, IdAllocator::reserve appelé par add()), sans besoin
// de persister l'état de l'allocateur séparément.
// ============================================================================

#include "UserProject.h"

#include <QSqlDatabase>
#include <QString>
#include <QVector>

#include <optional>

namespace navstud::persistence {

struct ProjectSummary
{
    qint64  id = -1;
    QString name;
    QString updatedAt; // ISO 8601, tel que stocké — affichage seulement
};

class ProjectStore
{
public:
    ProjectStore() = default;
    ~ProjectStore();

    // Idem pour le déplacement : un move par défaut ne libérerait pas la
    // connexion déjà détenue par l'objet de destination avant de l'écraser
    // — pas de cas d'usage actuel, plus sûr de l'interdire explicitement
    // que d'écrire une sémantique de déplacement correcte pour rien.
    ProjectStore(const ProjectStore&) = delete;
    ProjectStore& operator=(const ProjectStore&) = delete;
    ProjectStore(ProjectStore&&) = delete;
    ProjectStore& operator=(ProjectStore&&) = delete;

    // Ouvre (ou crée) le fichier SQLite à sqlitePath et s'assure que le schéma
    // existe. À appeler une fois avant tout usage des méthodes ci-dessous.
    bool open(const QString& sqlitePath, QString* errorMessage = nullptr);

    QVector<ProjectSummary> listProjects() const;

    // Échoue si le nom existe déjà (contrainte UNIQUE) — retourne -1.
    qint64 createProject(const QString& name, const userdata::StartingIndices& startingIndices, QString* errorMessage = nullptr);

    // Remplace les 15 compteurs de départ persistés pour un projet existant
    // — utilisé quand son fichier mondial associé est mis à jour. N'affecte
    // JAMAIS les entités déjà saisies (leurs id restent ceux qu'on leur a
    // donnés) : ne sert qu'à amorcer correctement une table encore VIDE au
    // prochain rechargement, cf. remarque en tête de ce fichier. L'appelant
    // est responsable d'avoir déjà renuméroté les tables non vides via
    // UserProject::renumberFrom AVANT d'appeler ceci, sans quoi les deux
    // se désynchronisent.
    bool updateStartingIndices(qint64 projectId, const userdata::StartingIndices& startingIndices, QString* errorMessage = nullptr);

    // std::nullopt si l'id n'existe pas.
    std::optional<userdata::UserProject> loadProject(qint64 projectId, QString* errorMessage = nullptr) const;

    // Remplace intégralement le contenu persisté du projet par l'état
    // actuel de project (supprime puis réinsère, dans une transaction —
    // pas de diff incrémental, plus simple et robuste pour une sauvegarde
    // explicite déclenchée par un bouton).
    bool saveProject(qint64 projectId, const userdata::UserProject& project, QString* errorMessage = nullptr);

private:
    QSqlDatabase mDb;
    QString      mConnectionName; // vide tant que open() n'a pas réussi
};

} // namespace navstud::persistence
