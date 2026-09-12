#pragma once

// ============================================================================
// ThemeManager.h
// Gestion des thèmes d'affichage : Système / Clair / Sombre.
//
// Stratégie retenue (garantit un rendu cohérent sur macOS ET Windows) :
//   - « Système » : restaure le style et la palette NATIFS capturés au premier
//     appel (on conserve ainsi le look natif macOS que l'on apprécie).
//   - « Clair » / « Sombre » : force le style « Fusion » accompagné d'une
//     palette dédiée. Fusion respecte intégralement les palettes, ce qui évite
//     le rendu « tout blanc » des styles natifs Windows.
//
// Le choix est persisté dans les QSettings de l'application.
// ============================================================================

#include <QString>

namespace navstud::ui {

class ThemeManager
{
public:
    enum class Theme {
        System,
        Light,
        Dark
    };

    // Applique le thème à l'application. À appeler idéalement avant la
    // création des fenêtres (depuis main()).
    static void apply(Theme theme);

    // Persistance du choix (QSettings).
    static Theme loadSavedTheme();
    static void  saveTheme(Theme theme);

    // Libellé humain du thème (« Système », « Clair », « Sombre »).
    static QString themeName(Theme theme);
};

} // namespace navstud::ui
