#ifndef AIRPORTEDITORWIDGET_H
#define AIRPORTEDITORWIDGET_H

#pragma once

#include "Entities.h"
#include "UserEntities.h"

#include <QWidget>

class QLineEdit;
class QPlainTextEdit;

class AirportEditorWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AirportEditorWidget(QWidget* parent = nullptr);

    void setValue(navstud::model::AirportId id, const navstud::userdata::UserAirport& airport);
    navstud::userdata::UserAirport value() const;

    void focusFirstField();
    void setPreviewLine(const QString& text);

signals:
    void valueEdited();

private:
    navstud::model::AirportId mCurrentId = navstud::model::AirportId::invalid();

    QLineEdit*      mPointIdentEdit;
    QLineEdit*      mElevationEdit;
    QLineEdit*      mLimitSpeedEdit;
    QLineEdit*      mLimitAltitudeEdit;
    QLineEdit*      mTransAltitudeEdit;
    QLineEdit*      mTransLevelEdit;
    QPlainTextEdit* mPreview;
};


#endif //AIRPORTEDITORWIDGET_H
