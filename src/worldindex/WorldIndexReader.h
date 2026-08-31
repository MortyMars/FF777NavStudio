#pragma once

// ============================================================================
// WorldIndexReader.h
// Lit le fichier mondial (~200 Mo) pour en extraire, section par section, le
// "# Count: N" qui suit chaque en-tête [SECTION] — confirmé sur un extrait
// réel : N est directement la valeur à donner en StartingIndices (pas de
// décalage +1/-1 : les données LFFA elles-mêmes commencent exactement à
// l'index N annoncé par le Count du monde avant intégration).
//
// N'a besoin de comprendre que la STRUCTURE du fichier (des en-têtes
// [SECTION] suivis d'un "# Count: N"), jamais son contenu détaillé — les
// lignes de données de chaque section sont parcourues sans être analysées.
// Dès que les 15 sections utiles à StartingIndices sont toutes trouvées, la
// lecture s'arrête : les sections AIRWAY*/ROUTES* qui suivent dans le
// fichier (et qui en constituent l'essentiel du volume, ex. ~183 000 lignes
// pour AIRWAYSEGMENTLEGS) ne sont jamais atteintes.
// ============================================================================

#include "UserProject.h" // StartingIndices

#include <QString>
#include <QStringList>

namespace navstud::worldindex {

struct WorldIndexResult
{
    bool                    success = false; // false si le fichier n'a pas pu être ouvert
    QString                 errorMessage;
    userdata::StartingIndices indices;       // valeurs par défaut (1) pour toute section non trouvée
    QStringList             missingSections; // sections attendues jamais rencontrées (fichier tronqué/incomplet)
};

class WorldIndexReader
{
public:
    WorldIndexResult readStartingIndices(const QString& filePath) const;
};

} // namespace navstud::worldindex
