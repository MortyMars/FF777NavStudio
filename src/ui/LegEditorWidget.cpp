#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include "LegEditorWidget.h"
#include "EditorFieldHelpers.h"
#include "TextFormat.h"

using namespace navstud::model;
using namespace navstud::userdata;
using namespace navstud::ui;

namespace {

// Les 23 codes ARINC 424 Path & Termination connus de
// conversion::validLegCodes() (UserToModelConverter.cpp) — texte déjà la
// valeur cible, aucune conversion.
void fillCodePathCombo(QComboBox* combo)
{
    for (const QString& code :
         { QStringLiteral("AF"), QStringLiteral("CA"), QStringLiteral("CD"), QStringLiteral("CF"),
           QStringLiteral("CI"), QStringLiteral("CR"), QStringLiteral("DF"), QStringLiteral("FA"),
           QStringLiteral("FC"), QStringLiteral("FD"), QStringLiteral("FM"), QStringLiteral("HA"),
           QStringLiteral("HF"), QStringLiteral("HM"), QStringLiteral("IF"), QStringLiteral("PI"),
           QStringLiteral("RF"), QStringLiteral("TF"), QStringLiteral("VA"), QStringLiteral("VD"),
           QStringLiteral("VI"), QStringLiteral("VM"), QStringLiteral("VR") })
        combo->addItem(code, code);
}

// Les 25 codes de la table WP_descript_code.xlsx connus de
// conversion::resolveWpDescription() — texte brut, converti en
// PointUsageFlags à la conversion, pas ici.
void fillWpDescriptionCombo(QComboBox* combo)
{
    combo->addItem(QStringLiteral("(aucune)"), QString());
    for (const QString& code :
         { QStringLiteral("_U__"), QStringLiteral("A___"), QStringLiteral("E___"), QStringLiteral("E__A"),
           QStringLiteral("E__F"), QStringLiteral("E__I"), QStringLiteral("E__M"), QStringLiteral("E_C_"),
           QStringLiteral("E_CA"), QStringLiteral("E_CH"), QStringLiteral("EE__"), QStringLiteral("EE_B"),
           QStringLiteral("EE_H"), QStringLiteral("EEC_"), QStringLiteral("EECH"), QStringLiteral("EY_M"),
           QStringLiteral("EYC_"), QStringLiteral("G___"), QStringLiteral("G__M"), QStringLiteral("GY_M"),
           QStringLiteral("N___"), QStringLiteral("P___"), QStringLiteral("R___"), QStringLiteral("T___"),
           QStringLiteral("V___") })
        combo->addItem(code, code);
}

void fillTurnDirCombo(QComboBox* combo)
{
    combo->addItem(QStringLiteral("Aucun (E)"), 0);
    combo->addItem(QStringLiteral("Gauche (L)"), -1);
    combo->addItem(QStringLiteral("Droite (R)"), 1);
}

} // namespace

// -----------------------------------------------------------------------------------------------------------
// Construit l'éditeur de leg : crée les champs de saisie et combos, le
// formulaire, la zone d'aperçu et connecte les signaux de modification.
LegEditorWidget::LegEditorWidget(QWidget* parent)
    : QWidget(parent)
{
    mCodePathCombo = new QComboBox(this);
    fillCodePathCombo(mCodePathCombo);

    mLegSequenceIdentEdit = makeIdentField(this, 6);
    mPointIdentEdit       = makeIdentField(this, 6); // optionnel : vide ou "-1" accepté

    mWpDescriptionCombo = new QComboBox(this);
    fillWpDescriptionCombo(mWpDescriptionCombo);

    mCourseEdit           = makeDoubleField(this, -1.0, 360.0);   // -1 = non spécifié
    mDistanceEdit         = makeDoubleField(this, 0.0, 99999.0);
    mNavaidIdentEdit      = makeIdentField(this, 6);               // optionnel : vide ou "-1" accepté
    mNavaidCourseEdit     = makeDoubleField(this, -1.0, 360.0);    // -1 = non spécifié
    mNavaidDistanceEdit   = makeDoubleField(this, -1.0, 99999.0);  // -1 = non spécifié
    mAltitudeLimitMinEdit = makeDoubleField(this, -1.0, 99999.0);  // -1 = non spécifié
    mAltitudeLimitMaxEdit = makeDoubleField(this, -1.0, 99999.0);  // -1 = non spécifié
    mAirSpeedLimitEdit    = makeDoubleField(this, -1.0, 9999.0);   // -1 = non spécifié
    mPathEdit             = makeDoubleField(this, -999.0, 999.0);

    mTurnDirCombo = new QComboBox(this);
    fillTurnDirCombo(mTurnDirCombo);

    mRnpEdit = makeDoubleField(this, 0.0, 99999.0);

    auto* form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->addRow(QStringLiteral("Code path"), mCodePathCombo);
    form->addRow(QStringLiteral("Ident séquence"), mLegSequenceIdentEdit);
    form->addRow(QStringLiteral("Ident point (FIX)"), mPointIdentEdit);
    form->addRow(QStringLiteral("Description WP"), mWpDescriptionCombo);
    form->addRow(QStringLiteral("Cap"), mCourseEdit);
    form->addRow(QStringLiteral("Distance (m)"), mDistanceEdit);
    form->addRow(QStringLiteral("Ident navaid"), mNavaidIdentEdit);
    form->addRow(QStringLiteral("Cap navaid"), mNavaidCourseEdit);
    form->addRow(QStringLiteral("Distance navaid (m)"), mNavaidDistanceEdit);
    form->addRow(QStringLiteral("Alt. min (ft)"), mAltitudeLimitMinEdit);
    form->addRow(QStringLiteral("Alt. max (ft)"), mAltitudeLimitMaxEdit);
    form->addRow(QStringLiteral("Lim. vitesse air"), mAirSpeedLimitEdit);
    form->addRow(QStringLiteral("Path"), mPathEdit);
    form->addRow(QStringLiteral("Sens virage"), mTurnDirCombo);
    form->addRow(QStringLiteral("RNP (m)"), mRnpEdit);

    auto* formGroup = new QGroupBox(QStringLiteral("Saisie — LEG"), this);
    formGroup->setLayout(form);

    mPreview = new QPlainTextEdit(this);
    mPreview->setReadOnly(true);
    mPreview->setMaximumHeight(64);
    mPreview->setLineWrapMode(QPlainTextEdit::NoWrap);

    auto* previewLayout = new QVBoxLayout;
    previewLayout->addWidget(mPreview);
    auto* previewGroup = new QGroupBox(QStringLiteral("Aperçu — ligne _Leg.txt"), this);
    previewGroup->setLayout(previewLayout);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(formGroup, 0, Qt::AlignLeft);
    mainLayout->addWidget(previewGroup);
    mainLayout->addStretch(1);

    connect(mCodePathCombo, &QComboBox::currentIndexChanged, this, &LegEditorWidget::valueEdited);
    connect(mLegSequenceIdentEdit, &QLineEdit::textChanged, this, &LegEditorWidget::valueEdited);
    connect(mPointIdentEdit, &QLineEdit::textChanged, this, &LegEditorWidget::valueEdited);
    connect(mWpDescriptionCombo, &QComboBox::currentIndexChanged, this, &LegEditorWidget::valueEdited);
    connect(mCourseEdit, &QLineEdit::textChanged, this, &LegEditorWidget::valueEdited);
    connect(mDistanceEdit, &QLineEdit::textChanged, this, &LegEditorWidget::valueEdited);
    connect(mNavaidIdentEdit, &QLineEdit::textChanged, this, &LegEditorWidget::valueEdited);
    connect(mNavaidCourseEdit, &QLineEdit::textChanged, this, &LegEditorWidget::valueEdited);
    connect(mNavaidDistanceEdit, &QLineEdit::textChanged, this, &LegEditorWidget::valueEdited);
    connect(mAltitudeLimitMinEdit, &QLineEdit::textChanged, this, &LegEditorWidget::valueEdited);
    connect(mAltitudeLimitMaxEdit, &QLineEdit::textChanged, this, &LegEditorWidget::valueEdited);
    connect(mAirSpeedLimitEdit, &QLineEdit::textChanged, this, &LegEditorWidget::valueEdited);
    connect(mPathEdit, &QLineEdit::textChanged, this, &LegEditorWidget::valueEdited);
    connect(mTurnDirCombo, &QComboBox::currentIndexChanged, this, &LegEditorWidget::valueEdited);
    connect(mRnpEdit, &QLineEdit::textChanged, this, &LegEditorWidget::valueEdited);
}

// -----------------------------------------------------------------------------------------------------------
// Charge les valeurs du leg dans les champs et combos de saisie, signaux
// bloqués pour éviter de redéclencher l'émission de valueEdited().
void LegEditorWidget::setValue(LegId id, const UserLeg& leg)
{
    mCurrentId = id;
    const QSignalBlocker b1(mCodePathCombo);
    const QSignalBlocker b2(mLegSequenceIdentEdit);
    const QSignalBlocker b3(mPointIdentEdit);
    const QSignalBlocker b4(mWpDescriptionCombo);
    const QSignalBlocker b5(mCourseEdit);
    const QSignalBlocker b6(mDistanceEdit);
    const QSignalBlocker b7(mNavaidIdentEdit);
    const QSignalBlocker b8(mNavaidCourseEdit);
    const QSignalBlocker b9(mNavaidDistanceEdit);
    const QSignalBlocker b10(mAltitudeLimitMinEdit);
    const QSignalBlocker b11(mAltitudeLimitMaxEdit);
    const QSignalBlocker b12(mAirSpeedLimitEdit);
    const QSignalBlocker b13(mPathEdit);
    const QSignalBlocker b14(mTurnDirCombo);
    const QSignalBlocker b15(mRnpEdit);

    mCodePathCombo->setCurrentIndex(mCodePathCombo->findData(leg.codePath));
    mLegSequenceIdentEdit->setText(leg.legSequenceIdent);
    mPointIdentEdit->setText(leg.pointIdent);
    mWpDescriptionCombo->setCurrentIndex(mWpDescriptionCombo->findData(leg.wpDescription));
    mCourseEdit->setText(navstud::writer::format::fixed(leg.course, 6));
    mDistanceEdit->setText(navstud::writer::format::fixed(leg.distanceInMeters, 6));
    mNavaidIdentEdit->setText(leg.navaidIdent);
    mNavaidCourseEdit->setText(navstud::writer::format::fixed(leg.navaidCourse, 6));
    mNavaidDistanceEdit->setText(navstud::writer::format::fixed(leg.navaidDistanceInMeters, 6));
    mAltitudeLimitMinEdit->setText(navstud::writer::format::fixed(leg.altitudeLimitMinInFeet, 6));
    mAltitudeLimitMaxEdit->setText(navstud::writer::format::fixed(leg.altitudeLimitMaxInFeet, 6));
    mAirSpeedLimitEdit->setText(navstud::writer::format::fixed(leg.airSpeedLimit, 6));
    mPathEdit->setText(navstud::writer::format::fixed(leg.path, 6));
    mTurnDirCombo->setCurrentIndex(mTurnDirCombo->findData(static_cast<int>(leg.turnDir)));
    mRnpEdit->setText(navstud::writer::format::fixed(leg.rnpInMeters, 6));
}

// -----------------------------------------------------------------------------------------------------------
// Lit les valeurs saisies et retourne l'entité UserLeg correspondante.
UserLeg LegEditorWidget::value() const
{
    UserLeg l;
    l.codePath                = mCodePathCombo->currentData().toString();
    l.legSequenceIdent          = mLegSequenceIdentEdit->text();
    l.pointIdent                 = mPointIdentEdit->text();
    l.wpDescription               = mWpDescriptionCombo->currentData().toString();
    l.course                      = mCourseEdit->text().toDouble();
    l.distanceInMeters            = mDistanceEdit->text().toDouble();
    l.navaidIdent                  = mNavaidIdentEdit->text();
    l.navaidCourse                  = mNavaidCourseEdit->text().toDouble();
    l.navaidDistanceInMeters         = mNavaidDistanceEdit->text().toDouble();
    l.altitudeLimitMinInFeet        = mAltitudeLimitMinEdit->text().toDouble();
    l.altitudeLimitMaxInFeet         = mAltitudeLimitMaxEdit->text().toDouble();
    l.airSpeedLimit                     = mAirSpeedLimitEdit->text().toDouble();
    l.path                               = mPathEdit->text().toDouble();
    l.turnDir                             = static_cast<qint8>(mTurnDirCombo->currentData().toInt());
    l.rnpInMeters                          = mRnpEdit->text().toDouble();
    return l;
}

// -----------------------------------------------------------------------------------------------------------
// Donne le focus au champ d'ident de séquence et sélectionne son contenu.
void LegEditorWidget::focusFirstField()
{
    mLegSequenceIdentEdit->setFocus();
    mLegSequenceIdentEdit->selectAll();
}

// -----------------------------------------------------------------------------------------------------------
// Affiche le texte donné dans la zone d'aperçu.
void LegEditorWidget::setPreviewLine(const QString& text)
{
    mPreview->setPlainText(text);
}
