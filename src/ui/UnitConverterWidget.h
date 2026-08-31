#ifndef UNITCONVERTERWIDGET_H
#define UNITCONVERTERWIDGET_H


#pragma once

// ============================================================================
// UnitConverterWidget.h
// Convertisseur bidirectionnel simple, ratio pur (value2 = value1 * factor)
// — aide à la saisie dans le bandeau principal, aucune donnée persistée.
// Deux champs éditables synchronisés en temps réel : taper dans l'un met à
// jour l'autre, et réciproquement.
// ============================================================================

#include <QWidget>

class QLineEdit;

class UnitConverterWidget : public QWidget
{
    Q_OBJECT

public:
    // labelFrom/labelTo : libellés courts affichés de part et d'autre (ex.
    // "ft"/"m"). factor : value(labelTo) = value(labelFrom) * factor.
    UnitConverterWidget(const QString& labelFrom, const QString& labelTo, double factor, QWidget* parent = nullptr);

private:
    QLineEdit* mFromEdit;
    QLineEdit* mToEdit;
    double     mFactor;
};


#endif //UNITCONVERTERWIDGET_H
