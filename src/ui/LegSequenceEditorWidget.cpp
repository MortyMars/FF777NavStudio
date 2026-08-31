#include "LegSequenceEditorWidget.h"
#include "EditorFieldHelpers.h"
#include "TextFormat.h"

#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QSignalBlocker>
#include <QVBoxLayout>

using namespace navstud::model;
using namespace navstud::userdata;
using namespace navstud::ui;

namespace {

// Les 2 libellés attendus tels quels par
// conversion::resolveSequenceType (UserToModelConverter.cpp).
void fillIlsOrRnavCombo(QComboBox* combo)
{
    for (const QString& label : { QStringLiteral("ILS"), QStringLiteral("RNAV") })
        combo->addItem(label, label);
}

// Les 4 libellés attendus tels quels par conversion::resolveSequenceType —
// combinés avec ilsOrRnav, seules 8 des 8 combinaisons possibles sont
// valides (cf. table dans UserToModelConverter.cpp) ; une combinaison
// invalide reste sélectionnable ici (l'éditeur ne pré-filtre pas les
// combos l'un par rapport à l'autre) mais sera rejetée à la conversion,
// avec message d'erreur affiché dans l'aperçu comme pour toute autre
// structure.
void fillProcedureKindCombo(QComboBox* combo)
{
    for (const QString& label :
         { QStringLiteral("APP"), QStringLiteral("APP TRANS"), QStringLiteral("SID"), QStringLiteral("STAR") })
        combo->addItem(label, label);
}

} // namespace

// -----------------------------------------------------------------------------------------------------------
// Construit l'éditeur de séquence de legs : crée les champs de saisie et
// combos, le formulaire, la zone d'aperçu et connecte les signaux.
LegSequenceEditorWidget::LegSequenceEditorWidget(QWidget* parent)
    : QWidget(parent)
{
    mIdentEdit = makeIdentField(this, 6);

    mIlsOrRnavCombo = new QComboBox(this);
    fillIlsOrRnavCombo(mIlsOrRnavCombo);

    mProcedureKindCombo = new QComboBox(this);
    fillProcedureKindCombo(mProcedureKindCombo);

    mAltitudeLevelTransEdit = makeDoubleField(this, 0.0, 99999.0);

    auto* form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->addRow(QStringLiteral("Ident"), mIdentEdit);
    form->addRow(QStringLiteral("ILS / RNAV"), mIlsOrRnavCombo);
    form->addRow(QStringLiteral("Type procédure"), mProcedureKindCombo);
    form->addRow(QStringLiteral("Alt. transition (ft)"), mAltitudeLevelTransEdit);

    auto* formGroup = new QGroupBox(QStringLiteral("Saisie — LEG SEQUENCE"), this);
    formGroup->setLayout(form);

    mPreview = new QPlainTextEdit(this);
    mPreview->setReadOnly(true);
    mPreview->setMaximumHeight(64);
    mPreview->setLineWrapMode(QPlainTextEdit::NoWrap);

    auto* previewLayout = new QVBoxLayout;
    previewLayout->addWidget(mPreview);
    auto* previewGroup = new QGroupBox(QStringLiteral("Aperçu — ligne _LegSequence.txt"), this);
    previewGroup->setLayout(previewLayout);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(formGroup, 0, Qt::AlignLeft);
    mainLayout->addWidget(previewGroup);
    mainLayout->addStretch(1);

    connect(mIdentEdit, &QLineEdit::textChanged, this, &LegSequenceEditorWidget::valueEdited);
    connect(mIlsOrRnavCombo, &QComboBox::currentIndexChanged, this, &LegSequenceEditorWidget::valueEdited);
    connect(mProcedureKindCombo, &QComboBox::currentIndexChanged, this, &LegSequenceEditorWidget::valueEdited);
    connect(mAltitudeLevelTransEdit, &QLineEdit::textChanged, this, &LegSequenceEditorWidget::valueEdited);
}

// -----------------------------------------------------------------------------------------------------------
// Charge les valeurs de la séquence dans les champs de saisie, signaux
// bloqués pour éviter de redéclencher l'émission de valueEdited().
void LegSequenceEditorWidget::setValue(LegSequenceId id, const UserLegSequence& legSequence)
{
    mCurrentId = id;
    const QSignalBlocker b1(mIdentEdit);
    const QSignalBlocker b2(mIlsOrRnavCombo);
    const QSignalBlocker b3(mProcedureKindCombo);
    const QSignalBlocker b4(mAltitudeLevelTransEdit);

    mIdentEdit->setText(legSequence.ident);
    mIlsOrRnavCombo->setCurrentIndex(mIlsOrRnavCombo->findData(legSequence.ilsOrRnav));
    mProcedureKindCombo->setCurrentIndex(mProcedureKindCombo->findData(legSequence.procedureKind));
    mAltitudeLevelTransEdit->setText(navstud::writer::format::fixed(legSequence.altitudeLevelTransInFeet, 6));
}

// -----------------------------------------------------------------------------------------------------------
// Lit les valeurs saisies et retourne l'entité UserLegSequence.
UserLegSequence LegSequenceEditorWidget::value() const
{
    UserLegSequence ls;
    ls.ident                     = mIdentEdit->text();
    ls.ilsOrRnav                 = mIlsOrRnavCombo->currentData().toString();
    ls.procedureKind              = mProcedureKindCombo->currentData().toString();
    ls.altitudeLevelTransInFeet   = mAltitudeLevelTransEdit->text().toDouble();
    return ls;
}

// -----------------------------------------------------------------------------------------------------------
// Donne le focus au champ d'ident et sélectionne son contenu.
void LegSequenceEditorWidget::focusFirstField()
{
    mIdentEdit->setFocus();
    mIdentEdit->selectAll();
}

// -----------------------------------------------------------------------------------------------------------
// Affiche le texte donné dans la zone d'aperçu.
void LegSequenceEditorWidget::setPreviewLine(const QString& text)
{
    mPreview->setPlainText(text);
}
