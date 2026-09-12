#include "Nav1DbPipeline.h"

#include "FileConverter.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStringList>
#include <QTextStream>

namespace navstud::tools {

namespace {

using FileConverterReal = ::FileConverter;

// Table de correspondance fichier de projet -> balise de section DEVANT
// laquelle le contenu est inséré. Chaque insertion complète donc la section
// placée juste avant la balise (nav1.txt a un ordre de sections fixe).
struct Insertion
{
    QString generatedFile;   // fichier produit par FF777NavStudio (dossier travail)
    QString beforeSection;   // section `[BEFORE]` avant laquelle insérer
    QString section;         // section réellement complétée (pour # Count)
};

// ----------------------------------------------------------------------------
// Table associant chaque fichier de projet à la section nav1.txt à compléter.
const QVector<Insertion>& insertionTable()
{
    static const QVector<Insertion> table = {
        { QStringLiteral("_Point.txt"),              QStringLiteral("WAYPOINTS"),                  QStringLiteral("POINTS") },
        { QStringLiteral("_Waypoint.txt"),           QStringLiteral("NAVAIDS"),                    QStringLiteral("WAYPOINTS") },
        { QStringLiteral("_Navaid.txt"),            QStringLiteral("AIRPORTS"),                   QStringLiteral("NAVAIDS") },
        { QStringLiteral("_Airport.txt"),           QStringLiteral("RUNWAYS"),                    QStringLiteral("AIRPORTS") },
        { QStringLiteral("_Runway.txt"),             QStringLiteral("LEGSEQUENCES"),               QStringLiteral("RUNWAYS") },
        { QStringLiteral("_LegSequence.txt"),        QStringLiteral("LEGS"),                       QStringLiteral("LEGSEQUENCES") },
        { QStringLiteral("_Leg.txt"),                QStringLiteral("DEPARTURES"),                 QStringLiteral("LEGS") },
        { QStringLiteral("_ProcedureSID.txt"),       QStringLiteral("ARRIVALS"),                   QStringLiteral("DEPARTURES") },
        { QStringLiteral("_ProcedureSTAR.txt"),      QStringLiteral("APPROACHES"),                 QStringLiteral("ARRIVALS") },
        { QStringLiteral("_Approach.txt"),           QStringLiteral("DEPARTURETRANSITIONS"),       QStringLiteral("APPROACHES") },
        { QStringLiteral("_ProcTransSID.txt"),       QStringLiteral("ARRIVALTRANSITIONS"),         QStringLiteral("DEPARTURETRANSITIONS") },
        { QStringLiteral("_ProcTransSTAR.txt"),      QStringLiteral("APPROACHTRANSITIONS"),        QStringLiteral("ARRIVALTRANSITIONS") },
        { QStringLiteral("_ApproachTransition.txt"), QStringLiteral("RUNWAYDEPARTURETRANSITIONS"), QStringLiteral("APPROACHTRANSITIONS") },
        { QStringLiteral("_RunProcTransSID.txt"),    QStringLiteral("RUNWAYARRIVALTRANSITIONS"),   QStringLiteral("RUNWAYDEPARTURETRANSITIONS") },
        { QStringLiteral("_RunProcTransSTAR.txt"),   QStringLiteral("AIRWAYS"),                    QStringLiteral("RUNWAYARRIVALTRANSITIONS") },
    };
    return table;
}

// Convertit un QString vers std::string (interface FileConverter).
// ----------------------------------------------------------------------------
// Convertit un QString vers std::string pour l'interface FileConverter.
std::string stdStr(const QString& s)
{
    return s.toUtf8().toStdString();
}

// ----------------------------------------------------------------------------
// Copie un fichier en écrasant la destination, avec message d'erreur.
bool copyFile(const QString& source, const QString& destination, QString* errorMessage)
{
    QFile::remove(destination);
    if (QFile::copy(source, destination))
        return true;
    if (errorMessage)
        *errorMessage = QStringLiteral("Copie impossible de %1 vers %2").arg(source, destination);
    return false;
}

// Recalcule et remplace la ligne "# Count:" de la section donnée d'après le
// nombre réel de lignes de données qu'elle contient (lignes non vides, hors
// commentaires, entre le compteur et l'en-tête de section suivant).
// ----------------------------------------------------------------------------
// Recalcule et met à jour la ligne "# Count:" d'une section donnée.
void recountSection(QStringList& lines, const QString& section)
{
    const QString header = QStringLiteral("[%1]").arg(section);
    for (int i = 0; i < lines.size(); ++i) {
        if (lines.at(i).trimmed() != header)
            continue;

        // La ligne de compteur suit immédiatement l'en-tête.
        int countLine = -1;
        for (int j = i + 1; j < lines.size(); ++j) {
            if (lines.at(j).trimmed().startsWith(QStringLiteral("# Count:"))) {
                countLine = j;
                break;
            }
            if (lines.at(j).trimmed().startsWith(QLatin1Char('[')))
                break;
        }
        if (countLine == -1)
            return;

        int dataCount = 0;
        for (int k = countLine + 1; k < lines.size(); ++k) {
            const QString t = lines.at(k).trimmed();
            if (t.startsWith(QLatin1Char('[')) && t.endsWith(QLatin1Char(']')))
                break;
            if (!t.isEmpty() && !t.startsWith(QLatin1Char('#')))
                ++dataCount;
        }

        lines[countLine] = QStringLiteral("# Count: %1").arg(dataCount);
        return;
    }
}

} // namespace

// ============================================================================
// Chemins
// ============================================================================

// ----------------------------------------------------------------------------
// Retourne le répertoire de travail.
//
// Par défaut on veut TOUJOURS travailler dans le dossier qui CONTIENT le
// bundle .app (macOS) et non dans le dossier de l'exécutable
// (…/FF777NavStudio.app/Contents/MacOS). Ainsi les fichiers lus/écrits
// (nav1.db, nav1.txt, extraits…) sont rangés juste à côté de l'application,
// là où l'utilisateur les attend. Les boîtes de dialogue proposent ce chemin
// par défaut.
QString Nav1DbPipeline::workingDir()
{
#ifdef Q_OS_MACOS
    // applicationDirPath() == /chemin/…/FF777NavStudio.app/Contents/MacOS
    // On remonte jusqu'au dossier qui contient le bundle .app.
    QDir dir(QCoreApplication::applicationDirPath());
    if (dir.dirName() == QLatin1String("MacOS")) {
        dir.cdUp(); // Contents
        dir.cdUp(); // FF777NavStudio.app
        dir.cdUp(); // dossier contenant le bundle
        return dir.absolutePath();
    }
#endif
    // Linux / Windows : pas de bundle, on garde le dossier de l'exécutable.
    return QCoreApplication::applicationDirPath();
}

// ----------------------------------------------------------------------------
// Retourne le chemin du fichier nav1.db dans le répertoire de travail.
QString Nav1DbPipeline::nav1DbPath()
{
    return workingDir() + QStringLiteral("/nav1.db");
}

// ----------------------------------------------------------------------------
// Retourne le chemin du fichier nav1.txt dans le répertoire de travail.
QString Nav1DbPipeline::nav1TxtPath()
{
    return workingDir() + QStringLiteral("/nav1.txt");
}

// ----------------------------------------------------------------------------
// Retourne le chemin du fichier réencodé nav1-2.db.
QString Nav1DbPipeline::nav1EncodedPath()
{
    return workingDir() + QStringLiteral("/nav1-2.db");
}

// ----------------------------------------------------------------------------
// Retourne le chemin du fichier de contrôle nav1-K.txt.
QString Nav1DbPipeline::nav1ControlPath()
{
    return workingDir() + QStringLiteral("/nav1-K.txt");
}

// ----------------------------------------------------------------------------
// Détermine le n° d'AIRAC à partir du 4ème champ de la ligne CONFIG.
// La ligne a la forme : CONFIG 0 "JEP" "2608" "01" "06AUG26" "03SEP26"
// (champs séparés par des espaces, identifiants entre guillemets). On découpe
// en respectant les guillemets, puis on prend le champ d'index 3 ("2608").
QString Nav1DbPipeline::readAirac(const QString& nav1TxtPath, QString* error)
{
    QFile file(nav1TxtPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error)
            *error = QStringLiteral("Impossible d'ouvrir %1 : %2").arg(nav1TxtPath, file.errorString());
        return QString();
    }

    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (!line.startsWith(QStringLiteral("CONFIG")))
            continue;

        // Découpage en champs en préservant les portions entre guillemets.
        QStringList fields;
        const int n = line.size();
        int i = 0;
        while (i < n) {
            while (i < n && line.at(i).isSpace())
                ++i;
            if (i >= n)
                break;
            if (line.at(i) == QLatin1Char('"')) {
                int j = i + 1;
                while (j < n && line.at(j) != QLatin1Char('"'))
                    ++j;
                fields << line.mid(i + 1, j - i - 1);
                i = (j < n) ? j + 1 : j;
            } else {
                int j = i;
                while (j < n && !line.at(j).isSpace())
                    ++j;
                fields << line.mid(i, j - i);
                i = j;
            }
        }

        // fields[0] = "CONFIG", [1] = index, [2] = source, [3] = cycle AIRAC.
        if (fields.size() > 3)
            return fields.at(3);
        break;
    }

    if (error)
        *error = QStringLiteral("Ligne CONFIG absente de %1.").arg(nav1TxtPath);
    return QString();
}

// ----------------------------------------------------------------------------
// Retourne le répertoire de destination X-Plane des données NavData.
QString Nav1DbPipeline::destinationDir()
{
    return QDir::homePath() + QStringLiteral("/X-Plane 12/Custom Data/STSFF/nav-data/ndbl/data");
}

// ============================================================================
// Op 2 : décodage nav1.db -> nav1.txt
// ============================================================================

// ----------------------------------------------------------------------------
// Décode le nav1.db du dossier de travail (raccourci vers l'overload ci-dessous).
bool Nav1DbPipeline::decode(QString* errorMessage, QString* detail)
{
    return decode(nav1DbPath(), errorMessage, detail);
}

// ----------------------------------------------------------------------------
// Décode un fichier nav1.db donné (éventuellement choisi hors du dossier de
// travail) en nav1.txt, toujours écrit dans le dossier de travail.
bool Nav1DbPipeline::decode(const QString& sourceDbPath, QString* errorMessage, QString* detail)
{
    if (!QFile::exists(sourceDbPath)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("nav1.db introuvable (%1).").arg(sourceDbPath);
        return false;
    }

    FileConverterReal converter;
    if (!converter.convertBinaryToText(stdStr(sourceDbPath), stdStr(nav1TxtPath()))) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Échec du décodage nav1.db -> nav1.txt.");
        return false;
    }

    if (detail)
        *detail = QStringLiteral("nav1.txt créé dans %1").arg(nav1TxtPath());
    return true;
}

// ============================================================================
// Op 4 : intégration des 15 fichiers de projet dans nav1.txt
// ============================================================================

// ----------------------------------------------------------------------------
// Intègre les 15 fichiers de projet dans nav1.txt et recalcule les compteurs.
bool Nav1DbPipeline::integrate(QString* errorMessage, QString* detail)
{
    const QString nav1Txt = nav1TxtPath();
    if (!QFile::exists(nav1Txt)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("nav1.txt introuvable — décodez d'abord nav1.db.");
        return false;
    }

    // Vérification de la présence des 15 fichiers générés.
    QStringList missing;
    for (const Insertion& ins : insertionTable())
        if (!QFile::exists(workingDir() + QLatin1Char('/') + ins.generatedFile))
            missing << ins.generatedFile;
    if (!missing.isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Fichier(s) manquant(s) dans le dossier de l'application : %1")
                                .arg(missing.join(QStringLiteral(", ")));
        return false;
    }

    // Lecture du contenu actuel de nav1.txt.
    QFile src(nav1Txt);
    if (!src.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Impossible d'ouvrir %1 en lecture.").arg(nav1Txt);
        return false;
    }
    QTextStream in(&src);
    in.setEncoding(QStringConverter::Utf8);
    QStringList output = in.readAll().split(QLatin1Char('\n'));
    src.close();

    // Chaque fichier généré est inséré juste avant sa balise de section cible.
    for (const Insertion& ins : insertionTable()) {
        QFile gen(workingDir() + QLatin1Char('/') + ins.generatedFile);
        if (!gen.open(QIODevice::ReadOnly | QIODevice::Text)) {
            if (errorMessage)
                *errorMessage = QStringLiteral("Impossible d'ouvrir %1.").arg(ins.generatedFile);
            return false;
        }
        QTextStream gin(&gen);
        gin.setEncoding(QStringConverter::Utf8);
        QStringList genLines = gin.readAll().split(QLatin1Char('\n'));
        gen.close();

        // Supprime une éventuelle ligne vide finale (le split produit un
        // dernier élément vide si le fichier finit par un saut de ligne).
        if (!genLines.isEmpty() && genLines.last().isEmpty())
            genLines.removeLast();

        const QString balise = QStringLiteral("[%1]").arg(ins.beforeSection);
        int targetLine = -1;
        for (int i = 0; i < output.size(); ++i) {
            if (output.at(i).trimmed() == balise) {
                targetLine = i;
                break;
            }
        }
        if (targetLine == -1) {
            if (errorMessage)
                *errorMessage = QStringLiteral("Section [%1] absente de nav1.txt.").arg(ins.beforeSection);
            return false;
        }

        // Le bloc généré complète la section : il s'insère à la suite de ses
        // données existantes, et conserve la ligne vide de séparation unique qui
        // précède l'en-tête de section suivant (une ligne vide après le bloc,
        // aucune entre les données et le bloc).
        int insertAt = targetLine;
        const int sep = targetLine - 1;
        const bool hasSep = sep >= 0 && output.at(sep).trimmed().isEmpty();
        if (hasSep)
            insertAt = sep;

        for (const QString& l : genLines)
            output.insert(insertAt++, l);

        // Sans ligne de séparation préexistante, en ajoute une après le bloc.
        if (!hasSep)
            output.insert(insertAt, QString());

        // Le compteur de la section complétée doit refléter son nouveau total.
        recountSection(output, ins.section);
    }

    // Réécriture de nav1.txt.
    QFile dst(nav1Txt);
    if (!dst.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Impossible d'écrire %1.").arg(nav1Txt);
        return false;
    }
    QTextStream out(&dst);
    out.setEncoding(QStringConverter::Utf8);
    out << output.join(QLatin1Char('\n'));
    dst.close();

    if (detail)
        *detail = QStringLiteral("nav1.txt complété : %1 fichiers intégrés et compteurs mis à jour.")
                      .arg(insertionTable().size());
    return true;
}

// ============================================================================
// Op 5 : réencodage nav1.txt -> nav1-2.db (+ fichier de contrôle)
// ============================================================================

// ----------------------------------------------------------------------------
// Réencode nav1.txt en nav1-2.db et génère le fichier de contrôle.
bool Nav1DbPipeline::encode(QString* errorMessage, QString* detail)
{
    if (!QFile::exists(nav1TxtPath())) {
        if (errorMessage)
            *errorMessage = QStringLiteral("nav1.txt introuvable.");
        return false;
    }

    FileConverterReal converter;
    if (!converter.convertTextToBinary(stdStr(nav1TxtPath()), stdStr(nav1EncodedPath()))) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Échec du réencodage nav1.txt -> nav1-2.db.");
        return false;
    }

    // Fichier texte de contrôle (re-décodage du binaire produit).
    FileConverterReal control;
    if (!control.convertBinaryToText(stdStr(nav1EncodedPath()), stdStr(nav1ControlPath()))) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Échec de la création du fichier de contrôle nav1-K.txt.");
        return false;
    }

    if (detail)
        *detail = QStringLiteral("nav1-2.db et nav1-K.txt créés dans %1").arg(workingDir());
    return true;
}

// ============================================================================
// Op 6 : déploiement vers la destination X-Plane
// ============================================================================

// ----------------------------------------------------------------------------
// Copie nav1.db (et le fichier de contrôle) vers la destination X-Plane.
bool Nav1DbPipeline::deploy(QString* errorMessage, QString* detail)
{
    if (!QFile::exists(nav1EncodedPath())) {
        if (errorMessage)
            *errorMessage = QStringLiteral("nav1-2.db introuvable — réencodez d'abord.");
        return false;
    }

    QDir().mkpath(destinationDir());

    if (!copyFile(nav1EncodedPath(), destinationDir() + QStringLiteral("/nav1.db"), errorMessage))
        return false;

    if (QFile::exists(nav1ControlPath()))
        copyFile(nav1ControlPath(), destinationDir() + QStringLiteral("/nav1-K.txt"), errorMessage);

    if (detail)
        *detail = QStringLiteral("nav1.db copié vers %1").arg(destinationDir());
    return true;
}

// ============================================================================
// Séquence complète ops 4 -> 5 -> 6
// ============================================================================

// ----------------------------------------------------------------------------
// Exécute la chaîne complète intégration -> encodage -> déploiement.
bool Nav1DbPipeline::integrateEncodeDeploy(QString* errorMessage, QString* detail)
{
    if (!integrate(errorMessage, detail))
        return false;
    if (!encode(errorMessage, detail))
        return false;
    return deploy(errorMessage, detail);
}

} // namespace navstud::tools