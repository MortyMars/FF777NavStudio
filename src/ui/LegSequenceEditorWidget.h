#ifndef LEGSEQUENCEEDITORWIDGET_H
#define LEGSEQUENCEEDITORWIDGET_H

#pragma once

#include "Entities.h"
#include "UserEntities.h"

#include <QWidget>

class QComboBox;
class QLineEdit;
class QPlainTextEdit;

class LegSequenceEditorWidget : public QWidget
{
    Q_OBJECT

public:
    explicit LegSequenceEditorWidget(QWidget* parent = nullptr);

    void setValue(navstud::model::LegSequenceId id, const navstud::userdata::UserLegSequence& legSequence);
    navstud::userdata::UserLegSequence value() const;

    void focusFirstField();
    void setPreviewLine(const QString& text);

signals:
    void valueEdited();

private:
    navstud::model::LegSequenceId mCurrentId = navstud::model::LegSequenceId::invalid();

    QLineEdit*      mIdentEdit;
    QComboBox*      mIlsOrRnavCombo;
    QComboBox*      mProcedureKindCombo;
    QLineEdit*      mAltitudeLevelTransEdit;
    QPlainTextEdit* mPreview;
};


#endif  //LEGSEQUENCEEDITORWIDGET_H