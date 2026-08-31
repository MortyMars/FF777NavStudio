#ifndef TABLECOLUMNHELPERS_H
#define TABLECOLUMNHELPERS_H


#pragma once

// ============================================================================
// TableColumnHelpers.h
// Fabriques de GenericTableModel::Column, génériques sur (Tag, Entity) —
// affichent toujours les champs UserXxx BRUTS (tels que saisis), jamais une
// valeur résolue/convertie : la table liste ce qui a été tapé, l'aperçu de
// l'éditeur (lui, résolu via régénération) montre ce que ça donnera.
// ============================================================================

#include "GenericTableModel.h"
#include "ProjectRepository.h"
#include "TextFormat.h"

#include <QVector>

namespace navstud::ui {

template <typename Tag, typename Entity>
std::function<QVector<qint32>()> orderFnFor(model::EntityTable<Tag, Entity>* table)
{
    return [table] {
        QVector<qint32> ids;
        for (const auto id : table->order())
            ids << id.value();
        return ids;
    };
}

inline GenericTableModel::Column idColumn()
{
    return GenericTableModel::Column{ QStringLiteral("Id"), [](qint32 rawId) -> QVariant { return rawId; } };
}

template <typename Tag, typename Entity>
GenericTableModel::Column textColumn(const QString& header, model::EntityTable<Tag, Entity>* table, QString Entity::* field)
{
    return GenericTableModel::Column{ header, [table, field](qint32 rawId) -> QVariant {
        const Entity* e = table->find(model::Id<Tag>(rawId));
        return e ? QVariant(e->*field) : QVariant();
    } };
}

template <typename Tag, typename Entity>
GenericTableModel::Column doubleColumn(const QString& header, model::EntityTable<Tag, Entity>* table,
                                        double Entity::* field, int decimals)
{
    return GenericTableModel::Column{ header, [table, field, decimals](qint32 rawId) -> QVariant {
        const Entity* e = table->find(model::Id<Tag>(rawId));
        return e ? QVariant(writer::format::fixed(e->*field, decimals)) : QVariant();
    } };
}

template <typename Tag, typename Entity>
GenericTableModel::Column intColumn(const QString& header, model::EntityTable<Tag, Entity>* table, qint8 Entity::* field)
{
    return GenericTableModel::Column{ header, [table, field](qint32 rawId) -> QVariant {
        const Entity* e = table->find(model::Id<Tag>(rawId));
        return e ? QVariant(e->*field) : QVariant();
    } };
}

template <typename Tag, typename Entity>
GenericTableModel::Column uintColumn(const QString& header, model::EntityTable<Tag, Entity>* table, quint32 Entity::* field)
{
    return GenericTableModel::Column{ header, [table, field](qint32 rawId) -> QVariant {
        const Entity* e = table->find(model::Id<Tag>(rawId));
        return e ? QVariant(e->*field) : QVariant();
    } };
}

} // namespace navstud::ui


#endif //TABLECOLUMNHELPERS_H
