#ifndef POINTEDITORWIDGET_H
#define POINTEDITORWIDGET_H

#pragma once

// ============================================================================
// PointEditorWidget.h
// Masque de saisie d'un Point, en deux parties empilées comme demandé :
// les champs éditables en haut, l'aperçu de la ligne _Point.txt telle
// qu'elle sera écrite en bas — recalculé à chaque frappe via convertPoint()
// + NavDataWriter::formatPointLine() (même code que le Writer réel, cf.
// NavDataWriter.h : jamais deux implémentations du format qui pourraient
// diverger).
// ============================================================================

#include "Entities.h"
#include "UserEntities.h"

#include <QWidget>

class QComboBox;
class QLineEdit;
class QPlainTextEdit;

class PointEditorWidget : public QWidget
{
    Q_OBJECT

    public:
        explicit PointEditorWidget(QWidget* parent = nullptr);

        // id sert à afficher un aperçu fidèle (le numéro fait partie de la
        // ligne texte) — passé séparément du contenu car il est attribué à la
        // création de la ligne côté UserProject, pas par ce widget.
        void setValue(navstud::model::PointId id, const navstud::userdata::UserPoint& point);
        navstud::userdata::UserPoint value() const;

        // Place le focus clavier sur le champ Ident et sélectionne son contenu
        // — confort après la création d'une nouvelle ligne.
        void focusIdent();

    signals:
        // Émis à chaque frappe/changement dans un champ. Le dataset visé (au
        // plus quelques centaines de lignes par projet) rend ce coût
        // négligeable — pas de throttling nécessaire à ce stade.
        void valueEdited();

    private slots:
        void onFieldChanged();

    private:
        void updatePreview();

        navstud::model::PointId mCurrentId = navstud::model::PointId::invalid();

        QLineEdit*      mIdentEdit;
        QLineEdit*      mLatitudeEdit;
        QLineEdit*      mLongitudeEdit;
        QLineEdit*      mMagVarEdit;
        QLineEdit*      mHoldCourseEdit;
        QLineEdit*      mHoldDistEdit;
        QLineEdit*      mHoldTimeEdit;
        QComboBox*      mHoldSideCombo;
        QPlainTextEdit* mPreview;

};


#endif //POINTEDITORWIDGET_H


