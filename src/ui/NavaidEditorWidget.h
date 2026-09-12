#ifndef NAVAIDEDITORWIDGET_H
#define NAVAIDEDITORWIDGET_H

#pragma once

#include "Entities.h"
#include "UserEntities.h"

#include <QWidget>

class QComboBox;
class QLineEdit;
class QPlainTextEdit;

class NavaidEditorWidget : public QWidget
{
    Q_OBJECT

    public:
        explicit NavaidEditorWidget(QWidget* parent = nullptr);

        void setValue(
            navstud::model::NavaidId id,
            const navstud::userdata::UserNavaid& navaid
        );

        navstud::userdata::UserNavaid value() const;

        void focusFirstField();
        void setPreviewLine(const QString& text);

    signals:
        void valueEdited();

    private:
        navstud::model::NavaidId mCurrentId = navstud::model::NavaidId::invalid();

        QComboBox*      mTypeCombo;
        QLineEdit*      mPointIdentEdit;
        QLineEdit*      mAssociatedNavaidEdit;
        QLineEdit*      mElevationEdit;
        QLineEdit*      mDeclinationEdit;
        QLineEdit*      mFrequencyEdit;
        QComboBox*      mCategoryCombo;
        QLineEdit*      mCourseEdit;
        QLineEdit*      mAngleEdit;
        QLineEdit*      mRunwayIdentEdit;
        QPlainTextEdit* mPreview;
};


#endif //NAVAIDEDITORWIDGET_H