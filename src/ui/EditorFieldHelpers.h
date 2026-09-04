#ifndef EDITORFIELDHELPERS_H
#define EDITORFIELDHELPERS_H


#pragma once

/* ---------------------------------------------------------------------------------
EditorFieldHelpers.h
Diverses fonctions 'inline' de création de champs partagés par tous les
XxxEditorWidget, évite de répéter la construction des validateurs (locale C,
plafond de décimales desserré) sur chacune des 11 structures différentes.
--------------------------------------------------------------------------------- */

#include <QDoubleValidator>
#include <QIntValidator>
#include <QLineEdit>
#include <QLocale>

namespace navstud::ui {

    // Crée un champs 'Ident'
    inline QLineEdit* makeIdentField(QWidget* parent, int maxLength = 6)
    {
        auto* edit = new QLineEdit(parent);
        edit->setMaxLength(maxLength);
        return edit;
    }

    // Crée un champs 'Int'
    inline QLineEdit* makeIntField(QWidget* parent, int min, int max)
    {
        auto* edit = new QLineEdit(parent);
        edit->setValidator(new QIntValidator(min, max, edit));
        return edit;
    }


    // Crée un champs 'Double'
    // decimals volontairement absent : plafond d'édition fixé à 15 pour tous les champs
    // (cf. PointEditorWidget — [bug] un plafond calé sur la précision réelle bloque
    // l'édition d'un chiffre au milieu d'un nombre déjà à sa précision cible)
    // La précision réellement écrite reste imposée par 'writer::format::fixed()' à la
    // génération, jamais par ce plafond de saisie.
    inline QLineEdit* makeDoubleField(QWidget* parent, double min, double max)
    {
        auto* edit = new QLineEdit(parent);
        auto* validator = new QDoubleValidator(min, max, 15, edit);
        validator->setNotation(QDoubleValidator::StandardNotation);
        validator->setLocale(QLocale::c());
        edit->setValidator(validator);
        return edit;
    }

} // namespace navstud::ui


#endif //EDITORFIELDHELPERS_H