#pragma once

// ============================================================================
// TextParse.h
// Fonctions de parsing élémentaires, exact inverse de writer/TextFormat.h.
// Chaque fonction retourne std::optional : nullopt signale un token
// invalide, à l'appelant de décider quoi en faire (ici : ligne ignorée,
// erreur consignée, lecture du fichier poursuivie).
// ============================================================================

#include "Entities.h"

#include <QChar>
#include <QFlags>
#include <QString>
#include <QStringList>
#include <QtGlobal>

#include <optional>

namespace navstud::reader::parse {

// Découpe une ligne en tokens séparés par des espaces, en respectant les
// portions entre guillemets doubles (Point::ident, LegSequence::ident)
// comme un seul token, guillemets retirés.
inline QStringList tokenize(const QString& line)
{
    QStringList tokens;
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
            tokens << line.mid(i + 1, j - i - 1);
            i = (j < n) ? j + 1 : j;
        } else {
            int j = i;
            while (j < n && !line.at(j).isSpace())
                ++j;
            tokens << line.mid(i, j - i);
            i = j;
        }
    }
    return tokens;
}

template <typename Tag>
inline std::optional<model::Id<Tag>> id(const QString& token)
{
    bool ok = false;
    const qint32 value = token.toInt(&ok);
    if (!ok)
        return std::nullopt;
    return model::Id<Tag>(value);
}

inline std::optional<double> real(const QString& token)
{
    bool ok = false;
    const double value = token.toDouble(&ok);
    if (!ok)
        return std::nullopt;
    return value;
}

inline std::optional<qint32> integer(const QString& token)
{
    bool ok = false;
    const qint32 value = token.toInt(&ok);
    if (!ok)
        return std::nullopt;
    return value;
}

inline std::optional<quint32> unsignedInteger(const QString& token)
{
    bool ok = false;
    const quint32 value = token.toUInt(&ok);
    if (!ok)
        return std::nullopt;
    return value;
}

// QFlags<Enum> à partir de la somme décimale brute — reconstruit exactement
// n'importe quelle combinaison de bits, même sans nom d'enumerator associé,
// via le constructeur QFlags(Enum) qui affecte directement la valeur interne
// (idiome Qt standard, symétrique de format::flagsToInt côté writer).
template <typename Enum>
inline std::optional<QFlags<Enum>> flags(const QString& token)
{
    const auto raw = unsignedInteger(token);
    if (!raw)
        return std::nullopt;
    return QFlags<Enum>(static_cast<Enum>(*raw));
}

// "IF" <- 18758 (inverse de format::packLegCode)
inline QString unpackLegCode(quint16 packed)
{
    const char hi = static_cast<char>((packed >> 8) & 0xFF);
    const char lo = static_cast<char>(packed & 0xFF);
    return QString(QLatin1Char(hi)) + QLatin1Char(lo);
}

// NavaidCategory : chiffre 0-3 pour les catégories numériques, lettre
// littérale (I/L/A/S/F) pour IGS/LDA/SDF — exact inverse de
// format::navaidCategory côté writer.
inline std::optional<model::NavaidCategory> navaidCategory(const QString& token)
{
    if (token.size() != 1)
        return std::nullopt;
    const QChar c = token.at(0);
    if (c.isDigit()) {
        const int digit = c.digitValue();
        if (digit >= 0 && digit <= 3)
            return static_cast<model::NavaidCategory>(digit);
        return std::nullopt;
    }
    switch (c.toLatin1()) {
    case 'I': return model::NavaidCategory::Igs;
    case 'L': return model::NavaidCategory::LdaWithGlideslope;
    case 'A': return model::NavaidCategory::LdaNoGlideslope;
    case 'S': return model::NavaidCategory::SdfWithGlideslope;
    case 'F': return model::NavaidCategory::SdfNoGlideslope;
    default:  return std::nullopt;
    }
}

} // namespace navstud::reader::parse
