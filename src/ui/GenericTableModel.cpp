#include "GenericTableModel.h"

// -----------------------------------------------------------------------------------------------------------
// Construit le modèle de table générique avec la fonction d'ordre des
// lignes et la liste des colonnes.
GenericTableModel::GenericTableModel(std::function<QVector<qint32>()> orderFn, QVector<Column> columns, QObject* parent)
    : QAbstractTableModel(parent)
    , mOrderFn(std::move(orderFn))
    , mColumns(std::move(columns))
{
}

// -----------------------------------------------------------------------------------------------------------
// Retourne le nombre de lignes : 0 si l'index parent est valide, sinon la
// taille de la fonction d'ordre.
int GenericTableModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : mOrderFn().size();
}

// -----------------------------------------------------------------------------------------------------------
// Retourne le nombre de colonnes : 0 si l'index parent est valide, sinon
// le nombre de colonnes définies.
int GenericTableModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : mColumns.size();
}

// -----------------------------------------------------------------------------------------------------------
// Retourne l'identifiant de la ligne donnée, ou -1 si la ligne est hors
// bornes.
qint32 GenericTableModel::idAt(int row) const
{
    const QVector<qint32> ids = mOrderFn();
    if (row < 0 || row >= ids.size())
        return -1;
    return ids.at(row);
}

// -----------------------------------------------------------------------------------------------------------
// Retourne la valeur d'affichage de la cellule pour le rôle Display, via
// le getter de la colonne, si l'index est valide.
QVariant GenericTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || role != Qt::DisplayRole)
        return {};
    if (index.column() < 0 || index.column() >= mColumns.size())
        return {};
    const qint32 id = idAt(index.row());
    if (id < 0)
        return {};
    return mColumns.at(index.column()).getter(id);
}

// -----------------------------------------------------------------------------------------------------------
// Retourne le libellé d'en-tête horizontal de la section donnée, sinon
// délègue à la classe de base.
QVariant GenericTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
        return QAbstractTableModel::headerData(section, orientation, role);
    return (section >= 0 && section < mColumns.size()) ? mColumns.at(section).header : QVariant();
}

// -----------------------------------------------------------------------------------------------------------
// Déclenche une remise à zéro du modèle pour rafraîchir la vue.
void GenericTableModel::reload()
{
    beginResetModel();
    endResetModel();
}

// -----------------------------------------------------------------------------------------------------------
// Émet dataChanged pour la ligne correspondant à l'identifiant donné, si
// celle-ci existe dans l'ordre.
void GenericTableModel::notifyRowChanged(qint32 id)
{
    const QVector<qint32> ids = mOrderFn();
    const int row = ids.indexOf(id);
    if (row < 0)
        return;
    emit dataChanged(index(row, 0), index(row, mColumns.size() - 1), { Qt::DisplayRole });
}
