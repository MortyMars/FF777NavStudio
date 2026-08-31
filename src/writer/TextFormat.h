#pragma once

// ============================================================================
// TextFormat.h
// Fonctions de formatage élémentaires pour le format texte cible, calées
// champ par champ sur les fichiers réels LFFA fournis en référence.
// ============================================================================

#include "Entities.h"

#include <QFlags>
#include <QString>
#include <QtGlobal>

namespace navstud::writer::format {

// Flottant à décimales fixes. Le nombre de décimales VARIE selon le champ
// (12 pour Point::latitude/longitude, 3 pour Point::holdDistInMeters/
// holdTime, 6 partout ailleurs) — c'est donc à l'appelant de préciser
// `decimals`, il n'y a pas de constante globale unique à utiliser.
inline QString fixed(double value, int decimals)
{
    return QString::number(value, 'f', decimals);
}

// Chaîne encadrée par des guillemets doubles littéraux — uniquement pour
// Point::ident et LegSequence::ident, les deux seuls champs QString du
// format cible.
inline QString quoted(const QString& s)
{
    return QStringLiteral("\"%1\"").arg(s);
}

// Décimal brut d'un Id<Tag>, y compris invalid() (-1) — jamais entre
// guillemets, jamais formaté différemment qu'un entier classique.
template <typename Tag>
inline QString id(model::Id<Tag> value)
{
    return QString::number(value.value());
}

// Somme décimale d'un QFlags<Enum> (PointUsageFlags, NavaidTypeFlags) — le
// format texte ne connaît pas les flags eux-mêmes, seulement leur somme.
template <typename Enum>
inline QString flagsToInt(QFlags<Enum> value)
{
    return QString::number(static_cast<quint32>(value.toInt()));
}

// NavaidCategory : les catégories numériques (0-3) s'écrivent en chiffre,
// les catégories IGS/LDA/SDF s'écrivent comme LA LETTRE LITTÉRALE (confirmé
// sur LFFA) — pas son code ASCII décimal, malgré le stockage interne en
// quint32 (cf. Entities.h, NavaidCategory).
inline QString navaidCategory(model::NavaidCategory category)
{
    const quint32 raw = static_cast<quint32>(category);
    if (raw <= 3)
        return QString::number(raw);
    return QString(QChar::fromLatin1(static_cast<char>(raw)));
}

// Encodage 2 caractères ASCII -> uint16, tel qu'observé dans le format cible
// (ex. "IF" -> 0x49 0x46 -> 18758). code doit faire exactement 2 caractères ;
// violé uniquement par une erreur de saisie en amont (le validateur devra
// s'en assurer avant d'atteindre le writer).
inline quint16 packLegCode(const QString& code)
{
    Q_ASSERT(code.size() == 2);
    const quint16 hi = static_cast<quint16>(static_cast<uchar>(code.at(0).toLatin1()));
    const quint16 lo = static_cast<quint16>(static_cast<uchar>(code.at(1).toLatin1()));
    return static_cast<quint16>((hi << 8) | lo);
}

} // namespace navstud::writer::format
