#ifndef LEGEDITORWIDGET_H
#define LEGEDITORWIDGET_H

#pragma once

#include "Entities.h"
#include "UserEntities.h"

#include <QWidget>

class QComboBox;
class QLineEdit;
class QPlainTextEdit;

class LegEditorWidget : public QWidget
{
    Q_OBJECT

public:
    explicit LegEditorWidget(QWidget* parent = nullptr);

    void setValue(navstud::model::LegId id, const navstud::userdata::UserLeg& leg);
    navstud::userdata::UserLeg value() const;

    void focusFirstField();
    void setPreviewLine(const QString& text);

signals:
    void valueEdited();

private:
    navstud::model::LegId mCurrentId = navstud::model::LegId::invalid();

    QComboBox*      mCodePathCombo;
    QLineEdit*      mLegSequenceIdentEdit;
    QLineEdit*      mPointIdentEdit;
    QComboBox*      mWpDescriptionCombo;
    QLineEdit*      mCourseEdit;
    QLineEdit*      mDistanceEdit;
    QLineEdit*      mNavaidIdentEdit;
    QLineEdit*      mNavaidCourseEdit;
    QLineEdit*      mNavaidDistanceEdit;
    QLineEdit*      mAltitudeLimitMinEdit;
    QLineEdit*      mAltitudeLimitMaxEdit;
    QLineEdit*      mAirSpeedLimitEdit;
    QLineEdit*      mPathEdit;
    QComboBox*      mTurnDirCombo;
    QLineEdit*      mRnpEdit;
    QPlainTextEdit* mPreview;
};

#endif //LEGEDITORWIDGET_H

