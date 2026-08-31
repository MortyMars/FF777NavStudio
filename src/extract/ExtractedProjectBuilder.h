#pragma once

// ============================================================================
// ExtractedProjectBuilder.h
// Transforme le fichier texte produit par NavDataBase::extractAirport() (un
// sous-ensemble d'un nav1.txt réel, copies VERBATIM des lignes source, dans
// les sections [POINTS]... [RUNWAYARRIVALTRANSITIONS]) en un userdata::UserProject
// prêt à être créé/enregistré par ProjectStore puis affiché par la GUI.
//
// Fidélité (compromis assumés, cf. aussi les warnings en sortie) :
//   * NavaidType et PointUsage non décrits par les tables de libellés sont
//     conservés tels quels en TEXTE NUMÉRIQUE ("4101", "8388640", ...) — le
//     convertisseur (resolveNavaidType/resolveWpDescription) accepte ces
//     valeurs et le round-trip vers nav1.txt est alors EXACT.
//   * Leg.code est ré-encodé en 2 caractères depuis l'uint16 brut
//     (reader::parse::unpackLegCode).
//   * LegSequence.sequenceTypeRaw (0/2/5/12 dans le monde, mais aussi 1/3/4/6)
//     est résolu vers les libellés ILS/RNAV×APP/APP TRANS/SID/STAR : exact
//     pour 0/2/5/12 selon le contexte (séquence cœur d'approche, SID, STAR,
//     transition d'approche), APPROXIMÉ (4 -> "RNAV|SID", 1/3 -> "ILS|SID", ...)
//     pour 1/3/4/6 qui n'ont pas de libellé — ces valeurs ne sont pas
//     représentables dans le couple libellés, leur régénération tombera sur
//     0/2/5/12. Des warnings listent les types approximés.
//   * Les références traversent les identifiants en clair vus du POINT de vue
//     de l'extraction (ident de point, ident de séquence) ; une collision
//     d'ident dans une table (deux points de même ident) se résoudra au mieux
//     par le hash last-wins du IdentResolver — cellule d'aéroport, rare.
//   * engineOutProcedureId des RUNWAYPROCEDURETRANSITION n'ayant pas de champ
//     de saisie, il est ignoré (-1 en sortie) : le modèle ne le porte pas.
// ============================================================================

#include "UserProject.h" // userdata::UserProject (+ model::EntityTable via ProjectRepository.h)

#include <QString>
#include <QStringList>

namespace navstud::extract {

struct ExtractBuildResult
{
    bool success = false;
    QString errorMessage;    // erreur fatale (fichier illisible...)
    QStringList warnings;    // approximations et sauvetages de lignes

    userdata::UserProject project;

    // totaux reconstruits par section (diagnostic / QDialog)
    int points           = 0;
    int waypoints        = 0;
    int navaids          = 0;
    int airports         = 0;
    int runways          = 0;
    int legSequences     = 0;
    int legs             = 0;
    int departures       = 0;
    int arrivals         = 0;
    int approaches       = 0;
    int appTransitions   = 0;
    int depTransitions   = 0;
    int arrTransitions   = 0;
    int rwyDepTransitions = 0;
    int rwyArrTransitions = 0;
};

class ExtractedProjectBuilder
{
public:
    // Lit le fichier extrait (sortie exacte de NavDataBase::extractAirport)
    // et construit le projet utilisateur. Ne déplace aucune donnée hors du
    // fichier : les id conservent les valeurs du monde d'origine.
    ExtractBuildResult build(const QString& extractedFilePath) const;
};

} // namespace navstud::extract