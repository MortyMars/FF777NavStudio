#ifndef PROCEDUREEDITORWIDGET_H
#define PROCEDUREEDITORWIDGET_H

#pragma once

#include "Entities.h"
#include "UserEntities.h"

#include <QWidget>

class QLineEdit;
class QPlainTextEdit;

// Classe utilisée dans les onglets SID et STAR — la structure est identique, seule la table
// conteneur diffère (cf. ProcedureKind dans Entities.h, jamais stocké dans l'entité elle-même).
class ProcedureEditorWidget : public QWidget
{
    Q_OBJECT

    public:
        explicit ProcedureEditorWidget(QWidget* parent = nullptr);

        void setValue(
            navstud::model::ProcedureId id,
            const navstud::userdata::UserProcedure& procedure
        );

        navstud::userdata::UserProcedure value() const;

        void focusFirstField();
        void setPreviewLine(const QString& text);

    signals:
        void valueEdited();

    private:
        navstud::model::ProcedureId mCurrentId = navstud::model::ProcedureId::invalid();

        QLineEdit*      mAirportIdentEdit;
        QLineEdit*      mLegSequenceIdentEdit;
        QPlainTextEdit* mPreview;
};


#endif //PROCEDUREEDITORWIDGET_H