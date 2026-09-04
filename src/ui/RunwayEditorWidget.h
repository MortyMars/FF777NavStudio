#ifndef RUNWAYEDITORWIDGET_H
#define RUNWAYEDITORWIDGET_H

#pragma once

#include "Entities.h"
#include "UserEntities.h"

#include <QWidget>

class QLineEdit;
class QPlainTextEdit;

class RunwayEditorWidget : public QWidget
{
    Q_OBJECT

    public:
        explicit RunwayEditorWidget(QWidget* parent = nullptr);

        void setValue(
            navstud::model::RunwayId id,
            const navstud::userdata::UserRunway& runway
        );

        navstud::userdata::UserRunway value() const;

        void focusFirstField();
        void setPreviewLine(const QString& text);

    signals:
        void valueEdited();

    private:
        navstud::model::RunwayId mCurrentId = navstud::model::RunwayId::invalid();

        QLineEdit*      mAirportIdentEdit;
        QLineEdit*      mThresholdIdentEdit;
        QLineEdit*      mElevationEdit;
        QLineEdit*      mGradientEdit;
        QLineEdit*      mCourseEdit;
        QLineEdit*      mLengthEdit;
        QLineEdit*      mDisplacedEdit;
        QLineEdit*      mStopwayEdit;
        QLineEdit*      mCrossEdit;
        QPlainTextEdit* mPreview;

};


#endif //RUNWAYEDITORWIDGET_H