#include <QFormLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include "ApproachTransitionEditorWidget.h"
#include "EditorFieldHelpers.h"

using namespace navstud::model;
using namespace navstud::userdata;
using namespace navstud::ui;

// -----------------------------------------------------------------------------------------------------------
// Construit l'éditeur de transition d'approche : crée les champs de
// saisie, le formulaire, la zone d'aperçu et connecte les signaux.
ApproachTransitionEditorWidget::ApproachTransitionEditorWidget(QWidget* parent)
    : QWidget(parent)
{
    mApproachIdentEdit    = makeIdentField(this, 6);
    mLegSequenceIdentEdit = makeIdentField(this, 6);

    auto* form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->addRow(QStringLiteral("Ident Approach"), mApproachIdentEdit);
    form->addRow(QStringLiteral("Ident séquence"), mLegSequenceIdentEdit);

    auto* formGroup = new QGroupBox(QStringLiteral("Saisie — APPROACHTRANSITION"), this);
    formGroup->setLayout(form);

    mPreview = new QPlainTextEdit(this);
    mPreview->setReadOnly(true);
    mPreview->setMaximumHeight(64);
    mPreview->setLineWrapMode(QPlainTextEdit::NoWrap);

    auto* previewLayout = new QVBoxLayout;
    previewLayout->addWidget(mPreview);
    auto* previewGroup = new QGroupBox(QStringLiteral("Aperçu — ligne _ApproachTransition.txt"), this);
    previewGroup->setLayout(previewLayout);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(formGroup, 0, Qt::AlignLeft);
    mainLayout->addWidget(previewGroup);
    mainLayout->addStretch(1);

    connect(mApproachIdentEdit, &QLineEdit::textChanged, this, &ApproachTransitionEditorWidget::valueEdited);
    connect(mLegSequenceIdentEdit, &QLineEdit::textChanged, this, &ApproachTransitionEditorWidget::valueEdited);
}

// -----------------------------------------------------------------------------------------------------------
// Charge les valeurs de la transition dans les champs de saisie, signaux
// bloqués pour éviter de redéclencher l'émission de valueEdited().
void ApproachTransitionEditorWidget::setValue(ApproachTransitionId id, const UserApproachTransition& transition)
{
    mCurrentId = id;
    const QSignalBlocker b1(mApproachIdentEdit);
    const QSignalBlocker b2(mLegSequenceIdentEdit);

    mApproachIdentEdit->setText(transition.approachIdent);
    mLegSequenceIdentEdit->setText(transition.legSequenceIdent);
}

// -----------------------------------------------------------------------------------------------------------
// Lit les valeurs saisies et retourne l'entité UserApproachTransition.
UserApproachTransition ApproachTransitionEditorWidget::value() const
{
    UserApproachTransition t;
    t.approachIdent    = mApproachIdentEdit->text();
    t.legSequenceIdent = mLegSequenceIdentEdit->text();
    return t;
}

// -----------------------------------------------------------------------------------------------------------
// Donne le focus au champ d'ident approach et sélectionne son contenu.
void ApproachTransitionEditorWidget::focusFirstField()
{
    mApproachIdentEdit->setFocus();
    mApproachIdentEdit->selectAll();
}

// -----------------------------------------------------------------------------------------------------------
// Affiche le texte donné dans la zone d'aperçu.
void ApproachTransitionEditorWidget::setPreviewLine(const QString& text)
{
    mPreview->setPlainText(text);
}
