#include "PointTableModel.h"
#include "TextFormat.h" // writer::format::fixed — même précision que la ligne texte réelle, décimales par champ

using namespace navstud::model;
using namespace navstud::userdata;

namespace {
constexpr int kColumnCount = 9;
}

// -----------------------------------------------------------------------------------------------------------
// Construit le modèle de table des points à partir de la table d'entités
// donnée.
PointTableModel::PointTableModel(EntityTable<PointTag, UserPoint>* table, QObject* parent)
    : QAbstractTableModel(parent)
    , mTable(table)
{
}

// -----------------------------------------------------------------------------------------------------------
// Retourne le nombre de lignes : 0 si l'index parent est valide, sinon le
// nombre de points de la table.
int PointTableModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : mTable->count();
}

// -----------------------------------------------------------------------------------------------------------
// Retourne le nombre de colonnes fixes de la table.
int PointTableModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : kColumnCount;
}

// -----------------------------------------------------------------------------------------------------------
// Retourne l'identifiant du point à la ligne donnée, ou un identifiant
// invalide si la ligne est hors bornes.
PointId PointTableModel::idAt(int row) const
{
    const QVector<PointId>& order = mTable->order();
    if (row < 0 || row >= order.size())
        return PointId::invalid();
    return order.at(row);
}

// -----------------------------------------------------------------------------------------------------------
// Retourne la valeur d'affichage de la cellule, les colonnes décimales
// étant formatées comme la ligne texte réelle.
QVariant PointTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || role != Qt::DisplayRole)
        return {};

    const PointId id = idAt(index.row());
    const UserPoint* p = mTable->find(id);
    if (!p)
        return {};

    // Colonnes décimales formatées explicitement (writer::format::fixed,
    // mêmes décimales que la ligne _Point.txt réelle) — un double brut
    // renvoyé tel quel serait affiché par Qt avec sa propre conversion par
    // défaut (précision limitée, donnant l'illusion d'un arrondi alors que
    // la valeur stockée ne l'est pas).
    switch (index.column()) {
    case 0: return id.value();
    case 1: return p->ident;
    case 2: return navstud::writer::format::fixed(p->latitude, 12);
    case 3: return navstud::writer::format::fixed(p->longitude, 12);
    case 4: return navstud::writer::format::fixed(p->magVar, 6);
    case 5: return navstud::writer::format::fixed(p->holdCourse, 6);
    case 6: return navstud::writer::format::fixed(p->holdDistInMeters, 3);
    case 7: return navstud::writer::format::fixed(p->holdTime, 3);
    case 8: return p->holdSide;
    default: return {};
    }
}

// -----------------------------------------------------------------------------------------------------------
// Retourne le libellé d'en-tête horizontal des colonnes du point.
QVariant PointTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
        return QAbstractTableModel::headerData(section, orientation, role);

    static const QStringList headers = {
        QStringLiteral("Id"), QStringLiteral("Ident"), QStringLiteral("Latitude"), QStringLiteral("Longitude"),
        QStringLiteral("Var. magn."), QStringLiteral("Cap hold"), QStringLiteral("Dist hold (m)"),
        QStringLiteral("Temps hold"), QStringLiteral("Sens hold"),
    };
    return (section >= 0 && section < headers.size()) ? headers.at(section) : QVariant();
}

// -----------------------------------------------------------------------------------------------------------
// Déclenche une remise à zéro du modèle pour rafraîchir la vue.
void PointTableModel::reload()
{
    beginResetModel();
    endResetModel();
}

// -----------------------------------------------------------------------------------------------------------
// Émet dataChanged pour la ligne correspondant à l'identifiant du point
// modifié, si celui-ci existe dans l'ordre.
void PointTableModel::notifyRowChanged(PointId id)
{
    const int row = mTable->order().indexOf(id);
    if (row < 0)
        return;
    emit dataChanged(index(row, 0), index(row, kColumnCount - 1), { Qt::DisplayRole });
}
