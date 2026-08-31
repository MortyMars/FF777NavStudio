#ifndef GENERICTABLEMODEL_H
#define GENERICTABLEMODEL_H


#pragma once

// ============================================================================
// GenericTableModel.h
// QAbstractTableModel générique, piloté par une liste de colonnes (libellé +
// fonction d'accès par id). Une seule classe pour les 11 structures — chaque
// XxxEditorWidget/onglet ne fournit que sa liste de colonnes, pas une classe
// de modèle dédiée.
//
// L'id est un qint32 "nu" ici (pas un model::Id<Tag> typé) : le modèle n'a
// pas besoin de connaître le Tag pour afficher une table, seulement de
// pouvoir le restituer à l'appelant (MainWindow), qui le re-typera lui-même
// via model::Id<Tag>(rawId) pour interroger la bonne EntityTable.
// ============================================================================

#include <QAbstractTableModel>
#include <QVector>

#include <functional>

class GenericTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    struct Column
    {
        QString header;
        std::function<QVariant(qint32 id)> getter;
    };

    // orderFn : renvoie les id de la table wrappée, dans l'ordre
    // d'insertion (typiquement une lambda capturant l'EntityTable* et
    // convertissant order() en QVector<qint32>).
    GenericTableModel(std::function<QVector<qint32>()> orderFn, QVector<Column> columns, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    qint32 idAt(int row) const;

public slots:
    // Reset complet — à utiliser seulement quand le NOMBRE de lignes change
    // (ajout). Pour une édition en place, préférer notifyRowChanged().
    void reload();

    // dataChanged() ciblé, sans reset — ne perturbe ni la sélection ni le
    // défilement en cours (important : se déclenche à chaque frappe).
    void notifyRowChanged(qint32 id);

private:
    std::function<QVector<qint32>()> mOrderFn;
    QVector<Column> mColumns;
};


#endif //GENERICTABLEMODEL_H