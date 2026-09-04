#ifndef WAYPOINTEDITORWIDGET_H
#define WAYPOINTEDITORWIDGET_H

#pragma once

/* ------------------------------------------------------------------------------------------------
WaypointEditorWidget.h
Masque de saisie d'un Waypoint — un seul champ (l'ident du Point visé).
Contrairement à PointEditorWidget, ce widget NE calcule PAS lui-même son aperçu : Waypoint a besoin
d'IdentResolver (résolution de pointIdent), donc c'est MainWindow qui régénère le projet et pousse
la ligne déjà calculée via setPreviewLine() — cf. le renversement de responsabilité documenté dans
mainwindow.cpp.
------------------------------------------------------------------------------------------------ */

#include "Entities.h"
#include "UserEntities.h"

#include <QWidget>

class QLineEdit;
class QPlainTextEdit;

class WaypointEditorWidget : public QWidget
{
    Q_OBJECT

    public:
        explicit WaypointEditorWidget(QWidget* parent = nullptr);

        void setValue(navstud::model::WaypointId id, const navstud::userdata::UserWaypoint& waypoint);
        navstud::userdata::UserWaypoint value() const;

        void focusFirstField();

        // Poussée par MainWindow après régénération — ligne résolue, ou message
        // d'erreur préfixé "⚠ " en cas d'échec de conversion.
        void setPreviewLine(const QString& text);

    signals:
        void valueEdited();

    private:
        navstud::model::WaypointId mCurrentId = navstud::model::WaypointId::invalid();

        QLineEdit*      mPointIdentEdit;
        QPlainTextEdit* mPreview;

};


#endif //WAYPOINTEDITORWIDGET_H