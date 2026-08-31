#ifndef PROCEDURETRANSITIONEDITORWIDGET_H
#define PROCEDURETRANSITIONEDITORWIDGET_H

#pragma once

#include "Entities.h"
#include "UserEntities.h"

#include <QWidget>

class QLineEdit;
class QPlainTextEdit;

// Partagé entre les onglets SID et STAR, comme ProcedureEditorWidget.
class ProcedureTransitionEditorWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ProcedureTransitionEditorWidget(QWidget* parent = nullptr);

    void setValue(navstud::model::ProcedureTransitionId id, const navstud::userdata::UserProcedureTransition& transition);
    navstud::userdata::UserProcedureTransition value() const;

    void focusFirstField();
    void setPreviewLine(const QString& text);

signals:
    void valueEdited();

private:
    navstud::model::ProcedureTransitionId mCurrentId = navstud::model::ProcedureTransitionId::invalid();

    QLineEdit*      mProcedureIdentEdit;
    QLineEdit*      mLegSequenceIdentEdit;
    QPlainTextEdit* mPreview;
};


#endif //PROCEDURETRANSITIONEDITORWIDGET_H