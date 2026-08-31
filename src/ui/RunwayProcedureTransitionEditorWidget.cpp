#include "RunwayProcedureTransitionEditorWidget.h"
#include "EditorFieldHelpers.h"

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
// Construit l'éditeur de transition de procédure de piste : crée les
// champs de saisie, le formulaire, la zone d'aperçu et connecte les
// signaux.
RunwayProcedureTransitionEditorWidget::RunwayProcedureTransitionEditorWidget(QWidget* parent)
    : QWidget(parent)
{
    mRunwayIdentEdit      = makeIdentField(this, 6);
    mProcedureIdentEdit   = makeIdentField(this, 6);
    mLegSequenceIdentEdit = makeIdentField(this, 6);

    auto* form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->addRow(QStringLiteral("Ident Runway"), mRunwayIdentEdit);
    form->addRow(QStringLiteral("Ident Procedure"), mProcedureIdentEdit);
    form->addRow(QStringLiteral("Ident séquence"), mLegSequenceIdentEdit);

    auto* formGroup = new QGroupBox(QStringLiteral("Saisie — RUNWAYPROCEDURETRANSITION"), this);
    formGroup->setLayout(form);

    mPreview = new QPlainTextEdit(this);
    mPreview->setReadOnly(true);
    mPreview->setMaximumHeight(64);
    mPreview->setLineWrapMode(QPlainTextEdit::NoWrap);

    auto* previewLayout = new QVBoxLayout;
    previewLayout->addWidget(mPreview);
    auto* previewGroup = new QGroupBox(
        QStringLiteral("Aperçu — ligne RUNWAYPROCEDURETRANSITION (_RunProcTransSID.txt / _RunProcTransSTAR.txt selon l'onglet)"),
        this);
    previewGroup->setLayout(previewLayout);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(formGroup, 0, Qt::AlignLeft);
    mainLayout->addWidget(previewGroup);
    mainLayout->addStretch(1);

    connect(mRunwayIdentEdit, &QLineEdit::textChanged, this, &RunwayProcedureTransitionEditorWidget::valueEdited);
    connect(mProcedureIdentEdit, &QLineEdit::textChanged, this, &RunwayProcedureTransitionEditorWidget::valueEdited);
    connect(mLegSequenceIdentEdit, &QLineEdit::textChanged, this, &RunwayProcedureTransitionEditorWidget::valueEdited);
}

// -----------------------------------------------------------------------------------------------------------
// Charge les valeurs de la transition dans les champs de saisie, signaux
// bloqués pour éviter de redéclencher l'émission de valueEdited().
void RunwayProcedureTransitionEditorWidget::setValue(RunwayProcedureTransitionId id,
                                                       const UserRunwayProcedureTransition& transition)
{
    mCurrentId = id;
    const QSignalBlocker b1(mRunwayIdentEdit);
    const QSignalBlocker b2(mProcedureIdentEdit);
    const QSignalBlocker b3(mLegSequenceIdentEdit);

    mRunwayIdentEdit->setText(transition.runwayIdent);
    mProcedureIdentEdit->setText(transition.procedureIdent);
    mLegSequenceIdentEdit->setText(transition.legSequenceIdent);
}

// -----------------------------------------------------------------------------------------------------------
// Lit les valeurs saisies et retourne l'entité UserRunwayProcedureTransition.
UserRunwayProcedureTransition RunwayProcedureTransitionEditorWidget::value() const
{
    UserRunwayProcedureTransition t;
    t.runwayIdent      = mRunwayIdentEdit->text();
    t.procedureIdent   = mProcedureIdentEdit->text();
    t.legSequenceIdent = mLegSequenceIdentEdit->text();
    return t;
}

// -----------------------------------------------------------------------------------------------------------
// Donne le focus au champ d'ident runway et sélectionne son contenu.
void RunwayProcedureTransitionEditorWidget::focusFirstField()
{
    mRunwayIdentEdit->setFocus();
    mRunwayIdentEdit->selectAll();
}

// -----------------------------------------------------------------------------------------------------------
// Affiche le texte donné dans la zone d'aperçu.
void RunwayProcedureTransitionEditorWidget::setPreviewLine(const QString& text)
{
    mPreview->setPlainText(text);
}
