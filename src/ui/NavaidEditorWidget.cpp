#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include "NavaidEditorWidget.h"
#include "EditorFieldHelpers.h"
#include "TextFormat.h"

using namespace navstud::model;
using namespace navstud::userdata;
using namespace navstud::ui;

namespace {

// Les 9 libellés de la table Flags_Navaid.xlsx — texte exact attendu par
// conversion::resolveNavaidType (UserToModelConverter.cpp).
void fillTypeCombo(QComboBox* combo)
{
    for (const QString& label : {
             QStringLiteral("DME"), QStringLiteral("GS"), QStringLiteral("GS+DME non coloc"),
             QStringLiteral("LOC"), QStringLiteral("LOC+GS+DME"), QStringLiteral("Nav coloc"),
             QStringLiteral("Nav non coloc"), QStringLiteral("TAC"), QStringLiteral("VOR") })
        combo->addItem(label, label);
}

// Les 9 codes de catégorie (table ARINC 5.80, cf. Entities.h NavaidCategory)
// — la donnée de chaque item (userData) est le code exact attendu par
// reader::parse::navaidCategory.
void fillCategoryCombo(QComboBox* combo)
{
    combo->addItem(QStringLiteral("0 — ILS Localizer Only, No Glideslope"), QStringLiteral("0"));
    combo->addItem(QStringLiteral("1 — ILS Localizer/MLS/GLS Category I"), QStringLiteral("1"));
    combo->addItem(QStringLiteral("2 — ILS Localizer/MLS/GLS Category II"), QStringLiteral("2"));
    combo->addItem(QStringLiteral("3 — ILS Localizer/MLS/GLS Category III"), QStringLiteral("3"));
    combo->addItem(QStringLiteral("I — IGS Facility"), QStringLiteral("I"));
    combo->addItem(QStringLiteral("L — LDA Facility with Glideslope"), QStringLiteral("L"));
    combo->addItem(QStringLiteral("A — LDA Facility, no Glideslope"), QStringLiteral("A"));
    combo->addItem(QStringLiteral("S — SDF Facility with Glideslope"), QStringLiteral("S"));
    combo->addItem(QStringLiteral("F — SDF Facility, no Glideslope"), QStringLiteral("F"));
}

} // namespace

// -----------------------------------------------------------------------------------------------------------
// Construit l'éditeur de navaid : crée les champs de saisie et combos, le
// formulaire, la zone d'aperçu et connecte les signaux de modification.
NavaidEditorWidget::NavaidEditorWidget(QWidget* parent)
    : QWidget(parent)
{
    mTypeCombo = new QComboBox(this);
    fillTypeCombo(mTypeCombo);

    mPointIdentEdit       = makeIdentField(this, 6);
    mAssociatedNavaidEdit = makeIdentField(this, 6); // "-1" accepté tel quel (texte, pas résolu comme ident si == "-1")
    mElevationEdit        = makeDoubleField(this, -9999.0, 99999.0);
    mDeclinationEdit      = makeDoubleField(this, -360.0, 360.0);
    mFrequencyEdit        = makeIntField(this, 0, 999999);

    mCategoryCombo = new QComboBox(this);
    fillCategoryCombo(mCategoryCombo);

    mCourseEdit      = makeDoubleField(this, 0.0, 360.0);
    mAngleEdit       = makeDoubleField(this, -90.0, 90.0);
    mRunwayIdentEdit = makeIdentField(this, 6);

    auto* form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->addRow(QStringLiteral("Type"), mTypeCombo);
    form->addRow(QStringLiteral("Ident Point"), mPointIdentEdit);
    form->addRow(QStringLiteral("GS/DME associé"), mAssociatedNavaidEdit);
    form->addRow(QStringLiteral("Élévation (m)"), mElevationEdit);
    form->addRow(QStringLiteral("Déc. magn."), mDeclinationEdit);
    form->addRow(QStringLiteral("Fréquence"), mFrequencyEdit);
    form->addRow(QStringLiteral("Cat"), mCategoryCombo);
    form->addRow(QStringLiteral("Cap"), mCourseEdit);
    form->addRow(QStringLiteral("Angle"), mAngleEdit);
    form->addRow(QStringLiteral("Runway"), mRunwayIdentEdit);

    auto* formGroup = new QGroupBox(QStringLiteral("Saisie — NAVAID"), this);
    formGroup->setLayout(form);

    mPreview = new QPlainTextEdit(this);
    mPreview->setReadOnly(true);
    mPreview->setMaximumHeight(64);
    mPreview->setLineWrapMode(QPlainTextEdit::NoWrap);

    auto* previewLayout = new QVBoxLayout;
    previewLayout->addWidget(mPreview);
    auto* previewGroup = new QGroupBox(QStringLiteral("Aperçu — ligne _Navaid.txt"), this);
    previewGroup->setLayout(previewLayout);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(formGroup, 0, Qt::AlignLeft);
    mainLayout->addWidget(previewGroup);
    mainLayout->addStretch(1);

    connect(mTypeCombo, &QComboBox::currentIndexChanged, this, &NavaidEditorWidget::valueEdited);
    connect(mPointIdentEdit, &QLineEdit::textChanged, this, &NavaidEditorWidget::valueEdited);
    connect(mAssociatedNavaidEdit, &QLineEdit::textChanged, this, &NavaidEditorWidget::valueEdited);
    connect(mElevationEdit, &QLineEdit::textChanged, this, &NavaidEditorWidget::valueEdited);
    connect(mDeclinationEdit, &QLineEdit::textChanged, this, &NavaidEditorWidget::valueEdited);
    connect(mFrequencyEdit, &QLineEdit::textChanged, this, &NavaidEditorWidget::valueEdited);
    connect(mCategoryCombo, &QComboBox::currentIndexChanged, this, &NavaidEditorWidget::valueEdited);
    connect(mCourseEdit, &QLineEdit::textChanged, this, &NavaidEditorWidget::valueEdited);
    connect(mAngleEdit, &QLineEdit::textChanged, this, &NavaidEditorWidget::valueEdited);
    connect(mRunwayIdentEdit, &QLineEdit::textChanged, this, &NavaidEditorWidget::valueEdited);
}

// -----------------------------------------------------------------------------------------------------------
// Charge les valeurs du navaid dans les champs et combos de saisie,
// signaux bloqués pour éviter de redéclencher l'émission de valueEdited().
void NavaidEditorWidget::setValue(NavaidId id, const UserNavaid& navaid)
{
    mCurrentId = id;
    const QSignalBlocker b1(mTypeCombo);
    const QSignalBlocker b2(mPointIdentEdit);
    const QSignalBlocker b3(mAssociatedNavaidEdit);
    const QSignalBlocker b4(mElevationEdit);
    const QSignalBlocker b5(mDeclinationEdit);
    const QSignalBlocker b6(mFrequencyEdit);
    const QSignalBlocker b7(mCategoryCombo);
    const QSignalBlocker b8(mCourseEdit);
    const QSignalBlocker b9(mAngleEdit);
    const QSignalBlocker b10(mRunwayIdentEdit);

    mTypeCombo->setCurrentIndex(mTypeCombo->findData(navaid.type));
    mPointIdentEdit->setText(navaid.pointIdent);
    mAssociatedNavaidEdit->setText(navaid.associatedNavaidIdent);
    mElevationEdit->setText(navstud::writer::format::fixed(navaid.elevationInMeters, 6));
    mDeclinationEdit->setText(navstud::writer::format::fixed(navaid.declination, 6));
    mFrequencyEdit->setText(QString::number(navaid.frequencyMHzTimes100));
    mCategoryCombo->setCurrentIndex(mCategoryCombo->findData(navaid.category));
    mCourseEdit->setText(navstud::writer::format::fixed(navaid.course, 6));
    mAngleEdit->setText(navstud::writer::format::fixed(navaid.angle, 6));
    mRunwayIdentEdit->setText(navaid.runwayIdent);
}

// -----------------------------------------------------------------------------------------------------------
// Lit les valeurs saisies et retourne l'entité UserNavaid correspondante.
UserNavaid NavaidEditorWidget::value() const
{
    UserNavaid n;
    n.type                  = mTypeCombo->currentData().toString();
    n.pointIdent             = mPointIdentEdit->text();
    n.associatedNavaidIdent  = mAssociatedNavaidEdit->text();
    n.elevationInMeters      = mElevationEdit->text().toDouble();
    n.declination            = mDeclinationEdit->text().toDouble();
    n.frequencyMHzTimes100   = mFrequencyEdit->text().toUInt();
    n.category                = mCategoryCombo->currentData().toString();
    n.course                  = mCourseEdit->text().toDouble();
    n.angle                   = mAngleEdit->text().toDouble();
    n.runwayIdent             = mRunwayIdentEdit->text();
    return n;
}

// -----------------------------------------------------------------------------------------------------------
// Donne le focus au champ d'ident du point et sélectionne son contenu.
void NavaidEditorWidget::focusFirstField()
{
    mPointIdentEdit->setFocus();
    mPointIdentEdit->selectAll();
}

// -----------------------------------------------------------------------------------------------------------
// Affiche le texte donné dans la zone d'aperçu.
void NavaidEditorWidget::setPreviewLine(const QString& text)
{
    mPreview->setPlainText(text);
}
