#include "UserToModelConverter.h"
#include "TextParse.h" // réutilise reader::parse::navaidCategory — les codes de la liste de choix (0/1/2/3/I/L/A/S/F) SONT déjà les valeurs cible

#include <QHash>
#include <QSet>

namespace navstud::conversion {

using namespace navstud::model;
using namespace navstud::userdata;

// -----------------------------------------------------------------------------------------------------------
// Valide l'entrée utilisateur (ident obligatoire, ≤ 6 caractères) puis
// convertit le point saisie en entité modèle Point.
ConversionResult<Point> convertPoint(const UserPoint& input)
{
    QStringList errors;

    if (input.ident.isEmpty())
        errors << QStringLiteral("ident vide");
    else if (input.ident.size() > 6)
        errors << QStringLiteral("ident \"%1\" dépasse 6 caractères").arg(input.ident);

    if (!errors.isEmpty())
        return ConversionResult<Point>::failed(errors);

    Point p;
    p.ident             = input.ident;
    p.latitude           = input.latitude;
    p.longitude          = input.longitude;
    p.magVar             = input.magVar;
    p.holdCourse         = input.holdCourse;
    p.holdDistInMeters   = input.holdDistInMeters;
    p.holdTime           = input.holdTime;
    p.holdSide           = input.holdSide;
    return ConversionResult<Point>::ok(p);
}

namespace {

// -----------------------------------------------------------------------------------------------------------
// Résout le libellé de type Navaid (table Flags_Navaid) en flags NavaidType.
// Table Flags_Navaid.xlsx — les 9 libellés confirmés correspondent
// exactement, décimale pour décimale, aux flags NavaidType déjà codés
// (ex. "GS+DME non coloc" = 8257 = NonCollocated|Gs|Dme, valeur retrouvée
// telle quelle dans les données LFFA réelles).
std::optional<NavaidTypeFlags> resolveNavaidType(const QString& label)
{
    static const QHash<QString, NavaidTypeFlags> table = {
        { QStringLiteral("DME"),               NavaidTypeFlags(NavaidType::Dme) },
        { QStringLiteral("GS"),                NavaidTypeFlags(NavaidType::Gs) },
        { QStringLiteral("GS+DME non coloc"),   NavaidTypeFlags(NavaidType::NonCollocated) | NavaidType::Gs | NavaidType::Dme },
        { QStringLiteral("LOC"),               NavaidTypeFlags(NavaidType::Loc) },
        { QStringLiteral("LOC+GS+DME"),        NavaidTypeFlags(NavaidType::Gs) | NavaidType::Loc | NavaidType::Dme },
        { QStringLiteral("Nav coloc"),          NavaidTypeFlags(NavaidType::Collocated) },
        { QStringLiteral("Nav non coloc"),      NavaidTypeFlags(NavaidType::NonCollocated) },
        { QStringLiteral("TAC"),               NavaidTypeFlags(NavaidType::Tac) },
        { QStringLiteral("VOR"),               NavaidTypeFlags(NavaidType::Vor) },
    };
    const auto it = table.constFind(label);
    if (it == table.constEnd()) {
        // Passthrough numérique d'un type issu de l'extraction (ex. 4101 =
        // Collocated|Vor|Dme, 8204 = NonCollocated|Tac|Vor) pour lesquels
        // aucun libellé de table n'existe : la valeur décimale SAISIE EST
        // déjà la somme de flags cible, inverse du flagsToInt du writer.
        bool ok = false;
        const quint32 raw = label.toUInt(&ok);
        if (ok)
            return QFlags<NavaidType>(static_cast<NavaidType>(raw));
        return std::nullopt;
    }
    return it.value();
}

} // namespace

// -----------------------------------------------------------------------------------------------------------
// Valide et convertit un navaid saisi en entité modèle, en résolvant ses
// identifiants (point, piste, navaid associé) via le résolveur.
ConversionResult<Navaid> convertNavaid(const UserNavaid& input, const IdentResolver& resolver)
{
    QStringList errors;

    const auto type = resolveNavaidType(input.type);
    if (!type)
        errors << QStringLiteral("type \"%1\" inconnu (cf. table Flags_Navaid)").arg(input.type);

    const auto pointId = resolver.point(input.pointIdent);
    if (!pointId)
        errors << QStringLiteral("point \"%1\" introuvable").arg(input.pointIdent);

    // associatedNavaidIdent optionnel : "-1" (ou vide) -> aucun navaid associé,
    // légitime (cf. Navaid::distanceNavaidId, invalid() = non spécifié).
    NavaidId distanceNavaidId = NavaidId::invalid();
    if (input.associatedNavaidIdent != QStringLiteral("-1") && !input.associatedNavaidIdent.isEmpty()) {
        const auto resolved = resolver.navaidViaPointIdent(input.associatedNavaidIdent);
        if (!resolved)
            errors << QStringLiteral("navaid associé \"%1\" introuvable").arg(input.associatedNavaidIdent);
        else
            distanceNavaidId = *resolved;
    }

    const auto category = reader::parse::navaidCategory(input.category);
    if (!category)
        errors << QStringLiteral("catégorie \"%1\" invalide").arg(input.category);

    // runwayIdent optionnel : "-1"/vide -> aucune piste associée (VOR/DME/NDB
    // non rattachés à une ILS), symétrique de associatedNavaidIdent.
    RunwayId runwayId = RunwayId::invalid();
    if (input.runwayIdent != QStringLiteral("-1") && !input.runwayIdent.isEmpty()) {
        const auto resolved = resolver.runwayViaPointIdent(input.runwayIdent);
        if (!resolved)
            errors << QStringLiteral("piste \"%1\" introuvable").arg(input.runwayIdent);
        else
            runwayId = *resolved;
    }

    if (!errors.isEmpty())
        return ConversionResult<Navaid>::failed(errors);

    Navaid n;
    n.type                  = *type;
    n.pointId                = *pointId;
    n.distanceNavaidId       = distanceNavaidId;
    n.elevationInMeters      = input.elevationInMeters;
    n.declination            = input.declination;
    n.figureOfMerit          = 0; // jamais saisi, cf. UserEntities.h
    n.frequencyMHzTimes100   = input.frequencyMHzTimes100;
    n.category                = *category;
    n.course                  = input.course;
    n.angle                   = input.angle;
    n.runwayId                = runwayId;
    return ConversionResult<Navaid>::ok(n);
}

// -----------------------------------------------------------------------------------------------------------
// Valide le point de l'aéroport puis convertit l'entrée en entité Airport.
ConversionResult<Airport> convertAirport(const UserAirport& input, const IdentResolver& resolver)
{
    QStringList errors;

    const auto pointId = resolver.point(input.pointIdent);
    if (!pointId)
        errors << QStringLiteral("point \"%1\" introuvable").arg(input.pointIdent);

    if (!errors.isEmpty())
        return ConversionResult<Airport>::failed(errors);

    Airport a;
    a.pointId                    = *pointId; // 1 saut : le Point résolu EST directement Airport.pointId, pas de recherche d'un Airport existant
    a.elevationInMeters          = input.elevationInMeters;
    a.limitSpeedInMetersPerSec   = input.limitSpeedInMetersPerSec;
    a.limitAltitudeInMeters      = input.limitAltitudeInMeters;
    a.transitionAltitudeInMeters = input.transitionAltitudeInMeters;
    a.transitionLevelInMeters    = input.transitionLevelInMeters;
    return ConversionResult<Airport>::ok(a);
}

// -----------------------------------------------------------------------------------------------------------
// Résout aéroport et point de seuil puis convertit l'entrée en entité Runway.
ConversionResult<Runway> convertRunway(const UserRunway& input, const IdentResolver& resolver)
{
    QStringList errors;

    const auto airportId = resolver.airportViaPointIdent(input.airportIdent);
    if (!airportId)
        errors << QStringLiteral("aéroport \"%1\" introuvable").arg(input.airportIdent);

    const auto thresholdPointId = resolver.point(input.thresholdIdent);
    if (!thresholdPointId)
        errors << QStringLiteral("point de seuil \"%1\" introuvable").arg(input.thresholdIdent);

    if (!errors.isEmpty())
        return ConversionResult<Runway>::failed(errors);

    Runway r;
    r.airportId        = *airportId;      // 2 sauts pt : texte -> Point -> Airport dont c'est le pointId
    r.pointId           = *thresholdPointId; // 1 saut : le Point résolu EST directement Runway.pointId
    r.elevationInMeters = input.elevationInMeters;
    r.gradient          = input.gradient;
    r.course            = input.course;
    r.lengthInMeters    = input.lengthInMeters;
    r.displacedInMeters = input.displacedInMeters;
    r.stopwayInMeters   = input.stopwayInMeters;
    r.crossInMeters     = input.crossInMeters;
    return ConversionResult<Runway>::ok(r);
}

namespace {

// Facteur de conversion pieds -> mètres, confirmé sur les données réelles
// (LegSequence.transitionInMeters, Approach.decisionHeightInMeters,
// Approach.minimumDescentInMeters — et nulle part ailleurs dans le classeur).
constexpr double kFeetToMeters = 1.0 / 3.28084;

// -----------------------------------------------------------------------------------------------------------
// Résout la combinaison ILS/RNAV × type de procédure en code séquence brut.
// Double liste ILS/RNAV x APP/APP TRANS/SID/STAR -> sequenceTypeRaw,
// reconstituée à partir des données réelles LFFA (_LegSequence.txt) : les 8
// combinaisons ne donnent que 4 valeurs distinctes (0, 2, 5, 12), cohérentes
// avec les codes ARINC 424 Route Type déjà croisés côté ApproachType/
// DepartureType/ArrivalType.
std::optional<quint8> resolveSequenceType(const QString& ilsOrRnav, const QString& procedureKind)
{
    static const QHash<QString, quint8> table = {
        { QStringLiteral("ILS|APP TRANS"),  0 },
        { QStringLiteral("RNAV|APP TRANS"), 0 },
        { QStringLiteral("ILS|SID"),        2 },
        { QStringLiteral("ILS|STAR"),       2 },
        { QStringLiteral("RNAV|SID"),       5 },
        { QStringLiteral("RNAV|STAR"),      5 },
        { QStringLiteral("ILS|APP"),        5 },
        { QStringLiteral("RNAV|APP"),      12 },
    };
    const auto it = table.constFind(ilsOrRnav + QLatin1Char('|') + procedureKind);
    if (it == table.constEnd())
        return std::nullopt;
    return it.value();
}

} // namespace

// -----------------------------------------------------------------------------------------------------------
// Valide ident et code séquence puis convertit l'entrée en LegSequence
// (altitude de transition convertie de pieds en mètres).
ConversionResult<LegSequence> convertLegSequence(const UserLegSequence& input)
{
    QStringList errors;

    if (input.ident.isEmpty())
        errors << QStringLiteral("ident vide");
    else if (input.ident.size() > 6)
        errors << QStringLiteral("ident \"%1\" dépasse 6 caractères").arg(input.ident);

    const auto sequenceType = resolveSequenceType(input.ilsOrRnav, input.procedureKind);
    if (!sequenceType)
        errors << QStringLiteral("combinaison \"%1\"/\"%2\" inconnue").arg(input.ilsOrRnav, input.procedureKind);

    if (!errors.isEmpty())
        return ConversionResult<LegSequence>::failed(errors);

    LegSequence ls;
    ls.ident              = input.ident;
    ls.sequenceTypeRaw    = *sequenceType;
    ls.transitionInMeters = input.altitudeLevelTransInFeet * kFeetToMeters;
    return ConversionResult<LegSequence>::ok(ls);
}

namespace {

// -----------------------------------------------------------------------------------------------------------
// Retourne l'ensemble des codes de path ARINC connus du légende de legs.
// Codage_Path.xlsx : la colonne "Path code" cible EST identique à la
// colonne "Path" saisie (ex. "IF" -> "IF") — Leg.code reste le texte tel
// quel, packLegCode se charge de l'encodage 2-car.->uint16 au Writer. Cette
// liste ne sert donc qu'à valider que le code saisi fait partie des 18
// valeurs ARINC connues, pas à le convertir.
const QSet<QString>& validLegCodes()
{
    static const QSet<QString> codes = {
        QStringLiteral("AF"), QStringLiteral("CA"), QStringLiteral("CD"), QStringLiteral("CF"),
        QStringLiteral("CI"), QStringLiteral("CR"), QStringLiteral("DF"), QStringLiteral("FA"),
        QStringLiteral("FC"), QStringLiteral("FD"), QStringLiteral("FM"), QStringLiteral("HA"),
        QStringLiteral("HF"), QStringLiteral("HM"), QStringLiteral("IF"), QStringLiteral("PI"),
        QStringLiteral("RF"), QStringLiteral("TF"), QStringLiteral("VA"), QStringLiteral("VD"),
        QStringLiteral("VI"), QStringLiteral("VM"), QStringLiteral("VR"),
    };
    return codes;
}

// -----------------------------------------------------------------------------------------------------------
// Résout un libellé de description de point (table WP_descript_code) en
// flags PointUsage ; une chaîne vide donne l'usage nul.
// WP_descript_code.xlsx : 25 codes texte -> valeur décimale brute (déjà la
// somme de flags PointUsage attendue, reconstituée via le même idiome que
// reader::parse::flags — QFlags(Enum) accepte une valeur qui n'est pas un
// enumerator nommé, elle porte simplement les bits demandés).
std::optional<PointUsageFlags> resolveWpDescription(const QString& label)
{
    if (label.isEmpty())
        return PointUsageFlags(); // aucune description : légitime, PointUsage::None

    static const QHash<QString, quint32> table = {
        { QStringLiteral("_U__"), 2 },          { QStringLiteral("A___"), 512 },
        { QStringLiteral("E___"), 32 },         { QStringLiteral("E__A"), 32800 },
        { QStringLiteral("E__F"), 131104 },     { QStringLiteral("E__I"), 262176 },
        { QStringLiteral("E__M"), 1048608 },    { QStringLiteral("E_C_"), 8388640 },
        { QStringLiteral("E_CA"), 8421408 },    { QStringLiteral("E_CH"), 1082130464u },
        { QStringLiteral("EE__"), 16777248 },   { QStringLiteral("EE_B"), 16842784 },
        { QStringLiteral("EE_H"), 1090519072u },{ QStringLiteral("EEC_"), 25165856 },
        { QStringLiteral("EECH"), 1098907680u },{ QStringLiteral("EY_M"), 34603040 },
        { QStringLiteral("EYC_"), 41943072 },   { QStringLiteral("G___"), 1024 },
        { QStringLiteral("G__M"), 1049600 },    { QStringLiteral("GY_M"), 34604032 },
        { QStringLiteral("N___"), 2048 },       { QStringLiteral("P___"), 16 },
        { QStringLiteral("R___"), 64 },         { QStringLiteral("T___"), 128 },
        { QStringLiteral("V___"), 4096 },
    };
    const auto it = table.constFind(label);
    if (it == table.constEnd()) {
        // Passthrough numérique d'un usage issu de l'extraction (combinaison
        // de bits sans libellé WP_descript dédié) : la valeur saisie EST déjà
        // la somme de flags cible, inverse du flagsToInt du writer.
        bool ok = false;
        const quint32 raw = label.toUInt(&ok);
        if (ok)
            return QFlags<PointUsage>(static_cast<PointUsage>(raw));
        return std::nullopt;
    }
    return PointUsageFlags(static_cast<PointUsage>(it.value()));
}

} // namespace

// -----------------------------------------------------------------------------------------------------------
// Valide et convertit un leg saisi en entité modèle, en résolvant séquence,
// point, usage et navaid associé.
ConversionResult<Leg> convertLeg(const UserLeg& input, const IdentResolver& resolver)
{
    QStringList errors;

    if (!validLegCodes().contains(input.codePath))
        errors << QStringLiteral("code path \"%1\" inconnu (cf. table Codage_Path)").arg(input.codePath);

    const auto legSequenceId = resolver.legSequence(input.legSequenceIdent);
    if (!legSequenceId)
        errors << QStringLiteral("séquence \"%1\" introuvable").arg(input.legSequenceIdent);

    // pointIdent optionnel : certains legs (ex. VA/CA, cap sans fix) n'en ont pas.
    PointId pointId = PointId::invalid();
    if (input.pointIdent != QStringLiteral("-1") && !input.pointIdent.isEmpty()) {
        const auto resolved = resolver.point(input.pointIdent);
        if (!resolved)
            errors << QStringLiteral("point \"%1\" introuvable").arg(input.pointIdent);
        else
            pointId = *resolved;
    }

    const auto pointUsage = resolveWpDescription(input.wpDescription);
    if (!pointUsage)
        errors << QStringLiteral("description \"%1\" inconnue (cf. table WP_descript_code)").arg(input.wpDescription);

    // navaidIdent optionnel : 2 sauts pt, "-1"/vide -> aucun navaid.
    NavaidId navaidId = NavaidId::invalid();
    if (input.navaidIdent != QStringLiteral("-1") && !input.navaidIdent.isEmpty()) {
        const auto resolved = resolver.navaidViaPointIdent(input.navaidIdent);
        if (!resolved)
            errors << QStringLiteral("navaid \"%1\" introuvable").arg(input.navaidIdent);
        else
            navaidId = *resolved;
    }

    if (!errors.isEmpty())
        return ConversionResult<Leg>::failed(errors);

    Leg l;
    l.code                       = input.codePath;
    l.legSequenceId               = *legSequenceId;
    l.pointId                     = pointId;
    l.pointUsage                  = *pointUsage;
    l.course                      = input.course;
    l.distanceInMeters            = input.distanceInMeters;
    l.navaidId                    = navaidId;
    l.navaidCourse                = input.navaidCourse;
    l.navaidDistanceInMeters      = input.navaidDistanceInMeters;
    l.altitudeLimitMinInMeters    = (input.altitudeLimitMinInFeet == -1.0) ? -1.0 : input.altitudeLimitMinInFeet / 3.28084;
    l.altitudeLimitMaxInMeters    = (input.altitudeLimitMaxInFeet == -1.0) ? -1.0 : input.altitudeLimitMaxInFeet / 3.28084;
    l.airSpeedLimit                = input.airSpeedLimit;
    l.path                         = input.path;
    l.turnDir                      = input.turnDir;
    l.rnpInMeters                   = input.rnpInMeters;
    return ConversionResult<Leg>::ok(l);
}

// -----------------------------------------------------------------------------------------------------------
// Valide aéroport et séquence puis convertit l'entrée en entité Procedure.
ConversionResult<Procedure> convertProcedure(const UserProcedure& input, const IdentResolver& resolver)
{
    QStringList errors;

    const auto airportId = resolver.airportViaPointIdent(input.airportIdent);
    if (!airportId)
        errors << QStringLiteral("aéroport \"%1\" introuvable").arg(input.airportIdent);

    const auto legSequenceId = resolver.legSequence(input.legSequenceIdent);
    if (!legSequenceId)
        errors << QStringLiteral("séquence \"%1\" introuvable").arg(input.legSequenceIdent);

    if (!errors.isEmpty())
        return ConversionResult<Procedure>::failed(errors);

    Procedure p;
    p.airportId      = *airportId;
    p.legSequenceId = *legSequenceId;
    return ConversionResult<Procedure>::ok(p);
}

// -----------------------------------------------------------------------------------------------------------
// Valide piste et séquence puis convertit l'entrée en entité Approach
// (hauteurs décision et descente converties de pieds en mètres).
ConversionResult<Approach> convertApproach(const UserApproach& input, const IdentResolver& resolver)
{
    QStringList errors;

    const auto runwayId = resolver.runwayViaPointIdent(input.runwayIdent);
    if (!runwayId)
        errors << QStringLiteral("piste \"%1\" introuvable").arg(input.runwayIdent);

    const auto legSequenceId = resolver.legSequence(input.legSequenceIdent);
    if (!legSequenceId)
        errors << QStringLiteral("séquence \"%1\" introuvable").arg(input.legSequenceIdent);

    if (!errors.isEmpty())
        return ConversionResult<Approach>::failed(errors);

    Approach a;
    a.runwayId              = *runwayId;
    a.legSequenceId          = *legSequenceId;
    a.decisionHeightInMeters = input.decisionHeightInFeet * kFeetToMeters;
    a.minimumDescentInMeters = input.minimumDescentInFeet * kFeetToMeters;
    return ConversionResult<Approach>::ok(a);
}

// -----------------------------------------------------------------------------------------------------------
// Valide procédure et séquence d'un kind donné puis convertit l'entrée en
// entité ProcedureTransition.
ConversionResult<ProcedureTransition> convertProcedureTransition(const UserProcedureTransition& input,
                                                                  ProcedureKind kind, const IdentResolver& resolver)
{
    QStringList errors;

    const auto procedureId = resolver.procedureViaLegSequenceIdent(kind, input.procedureIdent);
    if (!procedureId)
        errors << QStringLiteral("procédure \"%1\" introuvable").arg(input.procedureIdent);

    const auto legSequenceId = resolver.legSequence(input.legSequenceIdent);
    if (!legSequenceId)
        errors << QStringLiteral("séquence \"%1\" introuvable").arg(input.legSequenceIdent);

    if (!errors.isEmpty())
        return ConversionResult<ProcedureTransition>::failed(errors);

    ProcedureTransition t;
    t.procedureId     = *procedureId;
    t.legSequenceId   = *legSequenceId;
    return ConversionResult<ProcedureTransition>::ok(t);
}

// -----------------------------------------------------------------------------------------------------------
// Valide approche et séquence puis convertit l'entrée en entité
// ApproachTransition.
ConversionResult<ApproachTransition> convertApproachTransition(const UserApproachTransition& input, const IdentResolver& resolver)
{
    QStringList errors;

    const auto approachId = resolver.approachViaLegSequenceIdent(input.approachIdent);
    if (!approachId)
        errors << QStringLiteral("approche \"%1\" introuvable").arg(input.approachIdent);

    const auto legSequenceId = resolver.legSequence(input.legSequenceIdent);
    if (!legSequenceId)
        errors << QStringLiteral("séquence \"%1\" introuvable").arg(input.legSequenceIdent);

    if (!errors.isEmpty())
        return ConversionResult<ApproachTransition>::failed(errors);

    ApproachTransition t;
    t.approachId    = *approachId;
    t.legSequenceId = *legSequenceId;
    return ConversionResult<ApproachTransition>::ok(t);
}

// -----------------------------------------------------------------------------------------------------------
// Valide piste, procédure du kind donné et séquence puis convertit l'entrée
// en entité RunwayProcedureTransition.
ConversionResult<RunwayProcedureTransition> convertRunwayProcedureTransition(
    const UserRunwayProcedureTransition& input, ProcedureKind kind, const IdentResolver& resolver)
{
    QStringList errors;

    const auto runwayId = resolver.runwayViaPointIdent(input.runwayIdent);
    if (!runwayId)
        errors << QStringLiteral("piste \"%1\" introuvable").arg(input.runwayIdent);

    const auto procedureId = resolver.procedureViaLegSequenceIdent(kind, input.procedureIdent);
    if (!procedureId)
        errors << QStringLiteral("procédure \"%1\" introuvable").arg(input.procedureIdent);

    const auto legSequenceId = resolver.legSequence(input.legSequenceIdent);
    if (!legSequenceId)
        errors << QStringLiteral("séquence \"%1\" introuvable").arg(input.legSequenceIdent);

    if (!errors.isEmpty())
        return ConversionResult<RunwayProcedureTransition>::failed(errors);

    RunwayProcedureTransition t;
    t.runwayId               = *runwayId;
    t.procedureId            = *procedureId;
    t.engineOutProcedureId  = ProcedureId::invalid(); // pas de champ de saisie pour l'instant, cf. UserEntities.h
    t.legSequenceId          = *legSequenceId;
    return ConversionResult<RunwayProcedureTransition>::ok(t);
}

} // namespace navstud::conversion
