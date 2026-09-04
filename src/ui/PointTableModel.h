#ifndef POINTTABLEMODEL_H
#define POINTTABLEMODEL_H


#pragma once

/* ------------------------------------------------------------------------------------------------
PointTableModel.h
QAbstractTableModel au-dessus de 'model::EntityTable<PointTag, UserPoint>'
Ne possède aucune donnée, seulement un pointeur vers la table qui vit dans le 'UserProject' du
MainWindow.
Premier jalon de l'UI : les 10 autres structures suivent le même patron (XxxTableModel).
------------------------------------------------------------------------------------------------ */

#include "ProjectRepository.h"
#include "UserEntities.h"

#include <QAbstractTableModel>

class PointTableModel : public QAbstractTableModel
{
    Q_OBJECT

    public:
        explicit PointTableModel(
            navstud::model::EntityTable<navstud::model::PointTag,navstud::userdata::UserPoint>* table,
            QObject* parent = nullptr
        );

        int rowCount(const QModelIndex& parent = QModelIndex()) const override;
        int columnCount(const QModelIndex& parent = QModelIndex()) const override;

        QVariant data(const QModelIndex& index, int role) const override;
        QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

        // Id<PointTag> de la ligne row — pour que MainWindow sache quelle
        // entité éditer après un clic dans la vue.
        navstud::model::PointId idAt(int row) const;

    public slots:
        // Reset complet — à utiliser seulement quand le NOMBRE de lignes change
        // (ajout). Pour une édition en place, préférer notifyRowChanged() :
        // moins coûteux, et ne perturbe ni la sélection ni le défilement en
        // cours (important puisque valueEdited() se déclenche à chaque frappe).
        void reload();

        // Notifie une modification EN PLACE d'une ligne déjà existante (édition
        // d'un champ) — dataChanged() ciblé, sans reset du modèle.
        void notifyRowChanged(navstud::model::PointId id);

    private:
        navstud::model::EntityTable<navstud::model::PointTag, navstud::userdata::UserPoint>* mTable;

};


#endif //POINTTABLEMODEL_H