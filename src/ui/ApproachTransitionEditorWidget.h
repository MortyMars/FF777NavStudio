#ifndef APPROACHTRANSITIONEDITORWIDGET_H
#define APPROACHTRANSITIONEDITORWIDGET_H

#pragma once

#include "Entities.h"
#include "UserEntities.h"

#include <QWidget>

class QLineEdit;
class QPlainTextEdit;

class ApproachTransitionEditorWidget : public QWidget
{
    Q_OBJECT

    public:
        explicit ApproachTransitionEditorWidget(QWidget* parent = nullptr);

        void setValue(navstud::model::ApproachTransitionId id, const navstud::userdata::UserApproachTransition& transition);
        navstud::userdata::UserApproachTransition value() const;

        void focusFirstField();
        void setPreviewLine(const QString& text);

    signals:
        void valueEdited();

    private:
        navstud::model::ApproachTransitionId mCurrentId = navstud::model::ApproachTransitionId::invalid();

        QLineEdit*      mApproachIdentEdit;
        QLineEdit*      mLegSequenceIdentEdit;
        QPlainTextEdit* mPreview;
};

#endif //APPROACHTRANSITIONEDITORWIDGET_H