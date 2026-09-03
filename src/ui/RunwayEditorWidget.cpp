#include <QFormLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include "RunwayEditorWidget.h"
#include "EditorFieldHelpers.h"
#include "TextFormat.h"

using namespace navstud::model;
using namespace navstud::userdata;
using namespace navstud::ui;

// -----------------------------------------------------------------------------------------------------------
// Construit l'éditeur de piste : crée les champs de saisie, le formulaire,
// la zone d'aperçu et connecte les signaux de modification.
RunwayEditorWidget::RunwayEditorWidget(QWidget* parent)
    : QWidget(parent)
{
    mAirportIdentEdit   = makeIdentField(this, 6);
    mThresholdIdentEdit = makeIdentField(this, 6);
    mElevationEdit      = makeDoubleField(this, -9999.0, 99999.0);
    mGradientEdit       = makeDoubleField(this, -100.0, 100.0);
    mCourseEdit         = makeDoubleField(this, 0.0, 360.0);
    mLengthEdit         = makeDoubleField(this, 0.0, 99999.0);
    mDisplacedEdit      = makeDoubleField(this, 0.0, 99999.0);
    mStopwayEdit        = makeDoubleField(this, 0.0, 99999.0);
    mCrossEdit          = makeDoubleField(this, -999.0, 999.0);

    auto* form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->addRow(QStringLiteral("Ident Airport"), mAirportIdentEdit);
    form->addRow(QStringLiteral("Ident Threshold"), mThresholdIdentEdit);
    form->addRow(QStringLiteral("Élévation (m)"), mElevationEdit);
    form->addRow(QStringLiteral("Gradient (%)"), mGradientEdit);
    form->addRow(QStringLiteral("Cap magnétique"), mCourseEdit);
    form->addRow(QStringLiteral("Longueur (m)"), mLengthEdit);
    form->addRow(QStringLiteral("Seuil décalé (m)"), mDisplacedEdit);
    form->addRow(QStringLiteral("Longueur stopway (m)"), mStopwayEdit);
    form->addRow(QStringLiteral("Hauteur survol seuil (m)"), mCrossEdit);

    auto* formGroup = new QGroupBox(QStringLiteral("Saisie — RUNWAY"), this);
    formGroup->setLayout(form);

    mPreview = new QPlainTextEdit(this);
    mPreview->setReadOnly(true);
    mPreview->setMaximumHeight(64);
    mPreview->setLineWrapMode(QPlainTextEdit::NoWrap);

    auto* previewLayout = new QVBoxLayout;
    previewLayout->addWidget(mPreview);
    auto* previewGroup = new QGroupBox(QStringLiteral("Aperçu — ligne _Runway.txt"), this);
    previewGroup->setLayout(previewLayout);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(formGroup, 0, Qt::AlignLeft);
    mainLayout->addWidget(previewGroup);
    mainLayout->addStretch(1);

    connect(mAirportIdentEdit, &QLineEdit::textChanged, this, &RunwayEditorWidget::valueEdited);
    connect(mThresholdIdentEdit, &QLineEdit::textChanged, this, &RunwayEditorWidget::valueEdited);
    connect(mElevationEdit, &QLineEdit::textChanged, this, &RunwayEditorWidget::valueEdited);
    connect(mGradientEdit, &QLineEdit::textChanged, this, &RunwayEditorWidget::valueEdited);
    connect(mCourseEdit, &QLineEdit::textChanged, this, &RunwayEditorWidget::valueEdited);
    connect(mLengthEdit, &QLineEdit::textChanged, this, &RunwayEditorWidget::valueEdited);
    connect(mDisplacedEdit, &QLineEdit::textChanged, this, &RunwayEditorWidget::valueEdited);
    connect(mStopwayEdit, &QLineEdit::textChanged, this, &RunwayEditorWidget::valueEdited);
    connect(mCrossEdit, &QLineEdit::textChanged, this, &RunwayEditorWidget::valueEdited);
}

// -----------------------------------------------------------------------------------------------------------
// Charge les valeurs de la piste dans les champs de saisie, signaux
// bloqués pour éviter de redéclencher l'émission de valueEdited().
void RunwayEditorWidget::setValue(RunwayId id, const UserRunway& runway)
{
    mCurrentId = id;
    const QSignalBlocker b1(mAirportIdentEdit);
    const QSignalBlocker b2(mThresholdIdentEdit);
    const QSignalBlocker b3(mElevationEdit);
    const QSignalBlocker b4(mGradientEdit);
    const QSignalBlocker b5(mCourseEdit);
    const QSignalBlocker b6(mLengthEdit);
    const QSignalBlocker b7(mDisplacedEdit);
    const QSignalBlocker b8(mStopwayEdit);
    const QSignalBlocker b9(mCrossEdit);

    mAirportIdentEdit->setText(runway.airportIdent);
    mThresholdIdentEdit->setText(runway.thresholdIdent);
    mElevationEdit->setText(navstud::writer::format::fixed(runway.elevationInMeters, 6));
    mGradientEdit->setText(navstud::writer::format::fixed(runway.gradient, 6));
    mCourseEdit->setText(navstud::writer::format::fixed(runway.course, 6));
    mLengthEdit->setText(navstud::writer::format::fixed(runway.lengthInMeters, 6));
    mDisplacedEdit->setText(navstud::writer::format::fixed(runway.displacedInMeters, 6));
    mStopwayEdit->setText(navstud::writer::format::fixed(runway.stopwayInMeters, 6));
    mCrossEdit->setText(navstud::writer::format::fixed(runway.crossInMeters, 6));
}

// -----------------------------------------------------------------------------------------------------------
// Lit les valeurs saisies et retourne l'entité UserRunway correspondante.
UserRunway RunwayEditorWidget::value() const
{
    UserRunway r;
    r.airportIdent       = mAirportIdentEdit->text();
    r.thresholdIdent     = mThresholdIdentEdit->text();
    r.elevationInMeters  = mElevationEdit->text().toDouble();
    r.gradient           = mGradientEdit->text().toDouble();
    r.course             = mCourseEdit->text().toDouble();
    r.lengthInMeters     = mLengthEdit->text().toDouble();
    r.displacedInMeters  = mDisplacedEdit->text().toDouble();
    r.stopwayInMeters    = mStopwayEdit->text().toDouble();
    r.crossInMeters      = mCrossEdit->text().toDouble();
    return r;
}

// -----------------------------------------------------------------------------------------------------------
// Donne le focus au champ d'ident airport et sélectionne son contenu.
void RunwayEditorWidget::focusFirstField()
{
    mAirportIdentEdit->setFocus();
    mAirportIdentEdit->selectAll();
}

// -----------------------------------------------------------------------------------------------------------
// Affiche le texte donné dans la zone d'aperçu.
void RunwayEditorWidget::setPreviewLine(const QString& text)
{
    mPreview->setPlainText(text);
}
