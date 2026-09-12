#pragma once

// ============================================================================
// WorldFileRewriter.h
// Réindexeur du fichier mondial nav1.txt.
//
// Utilisé par le menu « Aéroport -> Projet » : une fois qu'un aéroport a été
// extrait vers un projet, ses enregistrements doivent disparaître du fichier
// mondial (pour éviter tout doublon lors de la réintégration ultérieure) et
// les enregistrements subsistants doivent être renumérotés de façon contiguë.
//
// La renumérotation est GLOBALE : les identifiants de chaque section sont
// renumérotés séquentiellement à partir de 0 et TOUS les champs de référence
// croisée sont remappés, y compris dans les sections enroute AIRWAYS /
// AIRWAYSEGMENTS / AIRWAYSEGMENTLEGS / ROUTES / ROUTESEGMENTS. Les `# Count:`
// de chaque section sont recalculés.
//
// Une référence qui pointait vers un enregistrement supprimé est ramenée à
// -1 (« non spécifié ») ; le nombre de ces références orphelines est remonté
// dans le résultat pour information.
// ============================================================================

#include "NavDataBase.h" // navstud::extract::NavDataBase::AirportSelection

#include <QString>

namespace navstud::extract {

class WorldFileRewriter
{
public:
    struct Result
    {
        bool    success = false;
        QString error;
        int     removedRecords = 0;      // enregistrements supprimés (toutes sections)
        int     danglingReferences = 0;  // références remplacées par -1
    };

    // Supprime les enregistrements marqués dans selection, réindexe le reste
    // et réécrit sourcePath. Une copie de sauvegarde est créée dans
    // sourcePath + ".bak" avant toute écriture.
    static Result removeAirport(const QString& sourcePath,
                                const NavDataBase::AirportSelection& selection);

    // Variante avec chemin de sauvegarde explicite.
    static Result removeAirport(const QString& sourcePath,
                                const NavDataBase::AirportSelection& selection,
                                const QString& backupPath);
};

} // namespace navstud::extract
