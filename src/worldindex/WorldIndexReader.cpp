#include "WorldIndexReader.h"

#include <QFile>
#include <QHash>
#include <QIODevice>
#include <QSet>
#include <QTextStream>

namespace navstud::worldindex {

using namespace navstud::userdata;

namespace {

// -----------------------------------------------------------------------------------------------------------
// Retourne la correspondance entre chaque section du fichier mondial et le
// champ de StartingIndices qu'elle alimente.
// Correspondance [SECTION] du fichier mondial -> champ de StartingIndices.
// Les 15 sections ci-dessous couvrent exactement les 15 champs de
// StartingIndices — les autres sections du fichier mondial (CONFIG, AIRWAYS,
// AIRWAYSEGMENTS, AIRWAYSEGMENTLEGS, ROUTES, ROUTESEGMENTS) ne concernent
// aucune structure de ce projet et sont ignorées.
const QHash<QString, qint32 StartingIndices::*>& sectionFieldMap()
{
    static const QHash<QString, qint32 StartingIndices::*> map = {
        { QStringLiteral("POINTS"),                       &StartingIndices::point },
        { QStringLiteral("WAYPOINTS"),                     &StartingIndices::waypoint },
        { QStringLiteral("NAVAIDS"),                       &StartingIndices::navaid },
        { QStringLiteral("AIRPORTS"),                      &StartingIndices::airport },
        { QStringLiteral("RUNWAYS"),                       &StartingIndices::runway },
        { QStringLiteral("LEGSEQUENCES"),                  &StartingIndices::legSequence },
        { QStringLiteral("LEGS"),                          &StartingIndices::leg },
        { QStringLiteral("DEPARTURES"),                    &StartingIndices::sidProcedure },
        { QStringLiteral("ARRIVALS"),                      &StartingIndices::starProcedure },
        { QStringLiteral("APPROACHES"),                    &StartingIndices::approach },
        { QStringLiteral("DEPARTURETRANSITIONS"),          &StartingIndices::sidProcedureTransition },
        { QStringLiteral("ARRIVALTRANSITIONS"),            &StartingIndices::starProcedureTransition },
        { QStringLiteral("APPROACHTRANSITIONS"),           &StartingIndices::approachTransition },
        { QStringLiteral("RUNWAYDEPARTURETRANSITIONS"),    &StartingIndices::sidRunwayProcedureTransition },
        { QStringLiteral("RUNWAYARRIVALTRANSITIONS"),      &StartingIndices::starRunwayProcedureTransition },
    };
    return map;
}

} // namespace

// -----------------------------------------------------------------------------------------------------------
// Lit un fichier mondial et extrait les 15 index de départ en le parcourant
// par section, en notant les sections éventuellement manquantes.
WorldIndexResult WorldIndexReader::readStartingIndices(const QString& filePath) const
{
    WorldIndexResult result;

    QFile file(filePath);
    if (!file.exists()) {
        result.errorMessage = QStringLiteral("Fichier introuvable : %1").arg(filePath);
        return result;
    }
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result.errorMessage = file.errorString();
        return result;
    }

    const auto& fieldMap = sectionFieldMap();
    QSet<QString> stillMissing;
    for (auto it = fieldMap.constBegin(); it != fieldMap.constEnd(); ++it)
        stillMissing.insert(it.key());

    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);

    QString currentSection;
    while (!in.atEnd() && !stillMissing.isEmpty()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty())
            continue;

        if (line.startsWith(QLatin1Char('[')) && line.endsWith(QLatin1Char(']'))) {
            currentSection = line.mid(1, line.size() - 2);
            continue;
        }

        if (!currentSection.isEmpty() && line.startsWith(QStringLiteral("# Count:"))) {
            const auto fieldIt = fieldMap.constFind(currentSection);
            if (fieldIt != fieldMap.constEnd()) {
                bool ok = false;
                const qint32 count = line.mid(8).trimmed().toInt(&ok); // après "# Count:"
                if (ok) {
                    result.indices.*(fieldIt.value()) = count;
                    stillMissing.remove(currentSection);
                }
            }
            currentSection.clear(); // un seul Count par section, inutile de continuer à la surveiller
        }
    }

    result.success = true; // le fichier s'est ouvert et a pu être lu — des sections manquantes restent possibles
    result.missingSections = QStringList(stillMissing.constBegin(), stillMissing.constEnd());
    return result;
}

} // namespace navstud::worldindex
