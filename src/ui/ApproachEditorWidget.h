#ifndef APPROACHEDITORWIDGET_H
#define APPROACHEDITORWIDGET_H

#pragma once

#include "Entities.h"
#include "UserEntities.h"

#include <QWidget>

class QLineEdit;
class QPlainTextEdit;

class ApproachEditorWidget : public QWidget
{
    Q_OBJECT

    public:
        explicit ApproachEditorWidget(QWidget* parent = nullptr);

        void setValue(navstud::model::ApproachId id, const navstud::userdata::UserApproach& approach);
        navstud::userdata::UserApproach value() const;

        void focusFirstField();
        void setPreviewLine(const QString& text);

    signals:
        void valueEdited();

    private:
        navstud::model::ApproachId mCurrentId = navstud::model::ApproachId::invalid();

        QLineEdit*      mRunwayIdentEdit;
        QLineEdit*      mLegSequenceIdentEdit;
        QLineEdit*      mDecisionHeightEdit;
        QLineEdit*      mMinimumDescentEdit;
        QPlainTextEdit* mPreview;
};


#endif //APPROACHEDITORWIDGET_H