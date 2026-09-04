#ifndef RUNWAYPROCEDURETRANSITIONEDITORWIDGET_H
#define RUNWAYPROCEDURETRANSITIONEDITORWIDGET_H


#pragma once

#include "Entities.h"
#include "UserEntities.h"

#include <QWidget>

class QLineEdit;
class QPlainTextEdit;

// Partagé entre les onglets SID et STAR, comme ProcedureEditorWidget.
// engineOutProcedureId n'est pas un champ de saisie pour l'instant (cf.
// UserEntities.h) — toujours -1 (EOS), pas de champ dans ce widget.
class RunwayProcedureTransitionEditorWidget : public QWidget
{
    Q_OBJECT

    public:
        explicit RunwayProcedureTransitionEditorWidget(QWidget* parent = nullptr);

        void setValue(
            navstud::model::RunwayProcedureTransitionId id,
            const navstud::userdata::UserRunwayProcedureTransition& transition
        );

        navstud::userdata::UserRunwayProcedureTransition value() const;

        void focusFirstField();
        void setPreviewLine(const QString& text);

    signals:
        void valueEdited();

    private:
        navstud::model::RunwayProcedureTransitionId mCurrentId = navstud::model::RunwayProcedureTransitionId::invalid();

        QLineEdit*      mRunwayIdentEdit;
        QLineEdit*      mProcedureIdentEdit;
        QLineEdit*      mLegSequenceIdentEdit;
        QPlainTextEdit* mPreview;

};


#endif //RUNWAYPROCEDURETRANSITIONEDITORWIDGET_H