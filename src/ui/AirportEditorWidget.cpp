#include "AirportEditorWidget.h"
#include "EditorFieldHelpers.h"
#include "TextFormat.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QSignalBlocker>
#include <QVBoxLayout>

using namespace navstud::model;
using namespace navstud::userdata;
using namespace navstud::ui;

// -----------------------------------------------------------------------------------------------------------
// Construit l'éditeur d'aéroport : crée les champs de saisie, le
// formulaire, la zone d'aperçu et connecte les signaux de modification.
AirportEditorWidget::AirportEditorWidget(QWidget* parent)
    : QWidget(parent)
{
    mPointIdentEdit    = makeIdentField(this, 6);
    mElevationEdit     = makeDoubleField(this, -9999.0, 99999.0);
    mLimitSpeedEdit    = makeDoubleField(this, -1.0, 999.0);
    mLimitAltitudeEdit = makeDoubleField(this, -1.0, 99999.0);
    mTransAltitudeEdit = makeDoubleField(this, -1.0, 99999.0);
    mTransLevelEdit    = makeDoubleField(this, -1.0, 99999.0);

    auto* form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->addRow(QStringLiteral("Ident du Point"), mPointIdentEdit);
    form->addRow(QStringLiteral("Élévation (m)"), mElevationEdit);
    form->addRow(QStringLiteral("Limite vitesse (m/s)"), mLimitSpeedEdit);
    form->addRow(QStringLiteral("Limite altitude (m)"), mLimitAltitudeEdit);
    form->addRow(QStringLiteral("Altitude transition (m)"), mTransAltitudeEdit);
    form->addRow(QStringLiteral("Niveau transition (m)"), mTransLevelEdit);

    auto* formGroup = new QGroupBox(QStringLiteral("Saisie — AIRPORT"), this);
    formGroup->setLayout(form);

    mPreview = new QPlainTextEdit(this);
    mPreview->setReadOnly(true);
    mPreview->setMaximumHeight(64);
    mPreview->setLineWrapMode(QPlainTextEdit::NoWrap);

    auto* previewLayout = new QVBoxLayout;
    previewLayout->addWidget(mPreview);
    auto* previewGroup = new QGroupBox(QStringLiteral("Aperçu — ligne _Airport.txt"), this);
    previewGroup->setLayout(previewLayout);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(formGroup, 0, Qt::AlignLeft);
    mainLayout->addWidget(previewGroup);
    mainLayout->addStretch(1);

    connect(mPointIdentEdit, &QLineEdit::textChanged, this, &AirportEditorWidget::valueEdited);
    connect(mElevationEdit, &QLineEdit::textChanged, this, &AirportEditorWidget::valueEdited);
    connect(mLimitSpeedEdit, &QLineEdit::textChanged, this, &AirportEditorWidget::valueEdited);
    connect(mLimitAltitudeEdit, &QLineEdit::textChanged, this, &AirportEditorWidget::valueEdited);
    connect(mTransAltitudeEdit, &QLineEdit::textChanged, this, &AirportEditorWidget::valueEdited);
    connect(mTransLevelEdit, &QLineEdit::textChanged, this, &AirportEditorWidget::valueEdited);
}

// -----------------------------------------------------------------------------------------------------------
// Charge les valeurs de l'aéroport dans les champs de saisie, signaux
// bloqués pour éviter de redéclencher l'émission de valueEdited().
void AirportEditorWidget::setValue(AirportId id, const UserAirport& airport)
{
    mCurrentId = id;
    const QSignalBlocker b1(mPointIdentEdit);
    const QSignalBlocker b2(mElevationEdit);
    const QSignalBlocker b3(mLimitSpeedEdit);
    const QSignalBlocker b4(mLimitAltitudeEdit);
    const QSignalBlocker b5(mTransAltitudeEdit);
    const QSignalBlocker b6(mTransLevelEdit);

    mPointIdentEdit->setText(airport.pointIdent);
    mElevationEdit->setText(navstud::writer::format::fixed(airport.elevationInMeters, 6));
    mLimitSpeedEdit->setText(navstud::writer::format::fixed(airport.limitSpeedInMetersPerSec, 6));
    mLimitAltitudeEdit->setText(navstud::writer::format::fixed(airport.limitAltitudeInMeters, 6));
    mTransAltitudeEdit->setText(navstud::writer::format::fixed(airport.transitionAltitudeInMeters, 6));
    mTransLevelEdit->setText(navstud::writer::format::fixed(airport.transitionLevelInMeters, 6));
}

// -----------------------------------------------------------------------------------------------------------
// Lit les valeurs saisies et retourne l'entité UserAirport correspondante.
UserAirport AirportEditorWidget::value() const
{
    UserAirport a;
    a.pointIdent                    = mPointIdentEdit->text();
    a.elevationInMeters             = mElevationEdit->text().toDouble();
    a.limitSpeedInMetersPerSec      = mLimitSpeedEdit->text().toDouble();
    a.limitAltitudeInMeters         = mLimitAltitudeEdit->text().toDouble();
    a.transitionAltitudeInMeters    = mTransAltitudeEdit->text().toDouble();
    a.transitionLevelInMeters       = mTransLevelEdit->text().toDouble();
    return a;
}

// -----------------------------------------------------------------------------------------------------------
// Donne le focus au champ d'ident du point et sélectionne son contenu.
void AirportEditorWidget::focusFirstField()
{
    mPointIdentEdit->setFocus();
    mPointIdentEdit->selectAll();
}

// -----------------------------------------------------------------------------------------------------------
// Affiche le texte donné dans la zone d'aperçu.
void AirportEditorWidget::setPreviewLine(const QString& text)
{
    mPreview->setPlainText(text);
}
