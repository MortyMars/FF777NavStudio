#include "ProcedureEditorWidget.h"
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
// Construit l'éditeur de procédure : crée les champs de saisie, le
// formulaire, la zone d'aperçu et connecte les signaux de modification.
ProcedureEditorWidget::ProcedureEditorWidget(QWidget* parent)
    : QWidget(parent)
{
    mAirportIdentEdit     = makeIdentField(this, 6);
    mLegSequenceIdentEdit = makeIdentField(this, 6);

    auto* form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->addRow(QStringLiteral("Ident Airport"), mAirportIdentEdit);
    form->addRow(QStringLiteral("Ident séquence"), mLegSequenceIdentEdit);

    auto* formGroup = new QGroupBox(QStringLiteral("Saisie — PROCEDURE"), this);
    formGroup->setLayout(form);

    mPreview = new QPlainTextEdit(this);
    mPreview->setReadOnly(true);
    mPreview->setMaximumHeight(64);
    mPreview->setLineWrapMode(QPlainTextEdit::NoWrap);

    auto* previewLayout = new QVBoxLayout;
    previewLayout->addWidget(mPreview);
    auto* previewGroup = new QGroupBox(
        QStringLiteral("Aperçu — ligne PROCEDURE (_ProcedureSID.txt / _ProcedureSTAR.txt selon l'onglet)"), this);
    previewGroup->setLayout(previewLayout);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(formGroup, 0, Qt::AlignLeft);
    mainLayout->addWidget(previewGroup);
    mainLayout->addStretch(1);

    connect(mAirportIdentEdit, &QLineEdit::textChanged, this, &ProcedureEditorWidget::valueEdited);
    connect(mLegSequenceIdentEdit, &QLineEdit::textChanged, this, &ProcedureEditorWidget::valueEdited);
}

// -----------------------------------------------------------------------------------------------------------
// Charge les valeurs de la procédure dans les champs de saisie, signaux
// bloqués pour éviter de redéclencher l'émission de valueEdited().
void ProcedureEditorWidget::setValue(ProcedureId id, const UserProcedure& procedure)
{
    mCurrentId = id;
    const QSignalBlocker b1(mAirportIdentEdit);
    const QSignalBlocker b2(mLegSequenceIdentEdit);

    mAirportIdentEdit->setText(procedure.airportIdent);
    mLegSequenceIdentEdit->setText(procedure.legSequenceIdent);
}

// -----------------------------------------------------------------------------------------------------------
// Lit les valeurs saisies et retourne l'entité UserProcedure correspondante.
UserProcedure ProcedureEditorWidget::value() const
{
    UserProcedure p;
    p.airportIdent     = mAirportIdentEdit->text();
    p.legSequenceIdent = mLegSequenceIdentEdit->text();
    return p;
}

// -----------------------------------------------------------------------------------------------------------
// Donne le focus au champ d'ident airport et sélectionne son contenu.
void ProcedureEditorWidget::focusFirstField()
{
    mAirportIdentEdit->setFocus();
    mAirportIdentEdit->selectAll();
}

// -----------------------------------------------------------------------------------------------------------
// Affiche le texte donné dans la zone d'aperçu.
void ProcedureEditorWidget::setPreviewLine(const QString& text)
{
    mPreview->setPlainText(text);
}
