#include <QFormLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include "ApproachEditorWidget.h"
#include "EditorFieldHelpers.h"
#include "TextFormat.h"

using namespace navstud::model;
using namespace navstud::userdata;
using namespace navstud::ui;

// -----------------------------------------------------------------------------------------------------------
// Construit l'éditeur d'approche : crée les champs de saisie, le
// formulaire, la zone d'aperçu et connecte les signaux de modification.
ApproachEditorWidget::ApproachEditorWidget(QWidget* parent)
    : QWidget(parent)
{
    mRunwayIdentEdit      = makeIdentField(this, 6);
    mLegSequenceIdentEdit = makeIdentField(this, 6);
    // Bornes larges — DH/MDA sont de vraies saisies pieds, pas figées
    // (cf. UserEntities.h : la DH n'est pas constante par aéroport).
    mDecisionHeightEdit = makeDoubleField(this, 0.0, 9999.0);
    mMinimumDescentEdit = makeDoubleField(this, 0.0, 9999.0);

    auto* form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->addRow(QStringLiteral("Ident Runway"), mRunwayIdentEdit);
    form->addRow(QStringLiteral("Ident séquence"), mLegSequenceIdentEdit);
    form->addRow(QStringLiteral("Decision Height (ft)"), mDecisionHeightEdit);
    form->addRow(QStringLiteral("Minimum Descent (ft)"), mMinimumDescentEdit);

    auto* formGroup = new QGroupBox(QStringLiteral("Saisie — APPROACH"), this);
    formGroup->setLayout(form);

    mPreview = new QPlainTextEdit(this);
    mPreview->setReadOnly(true);
    mPreview->setMaximumHeight(64);
    mPreview->setLineWrapMode(QPlainTextEdit::NoWrap);

    auto* previewLayout = new QVBoxLayout;
    previewLayout->addWidget(mPreview);
    auto* previewGroup = new QGroupBox(QStringLiteral("Aperçu — ligne _Approach.txt"), this);
    previewGroup->setLayout(previewLayout);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(formGroup, 0, Qt::AlignLeft);
    mainLayout->addWidget(previewGroup);
    mainLayout->addStretch(1);

    connect(mRunwayIdentEdit, &QLineEdit::textChanged, this, &ApproachEditorWidget::valueEdited);
    connect(mLegSequenceIdentEdit, &QLineEdit::textChanged, this, &ApproachEditorWidget::valueEdited);
    connect(mDecisionHeightEdit, &QLineEdit::textChanged, this, &ApproachEditorWidget::valueEdited);
    connect(mMinimumDescentEdit, &QLineEdit::textChanged, this, &ApproachEditorWidget::valueEdited);
}

// -----------------------------------------------------------------------------------------------------------
// Charge les valeurs de l'approche dans les champs de saisie, signaux
// bloqués pour éviter de redéclencher l'émission de valueEdited().
void ApproachEditorWidget::setValue(ApproachId id, const UserApproach& approach)
{
    mCurrentId = id;
    const QSignalBlocker b1(mRunwayIdentEdit);
    const QSignalBlocker b2(mLegSequenceIdentEdit);
    const QSignalBlocker b3(mDecisionHeightEdit);
    const QSignalBlocker b4(mMinimumDescentEdit);

    mRunwayIdentEdit->setText(approach.runwayIdent);
    mLegSequenceIdentEdit->setText(approach.legSequenceIdent);
    mDecisionHeightEdit->setText(navstud::writer::format::fixed(approach.decisionHeightInFeet, 6));
    mMinimumDescentEdit->setText(navstud::writer::format::fixed(approach.minimumDescentInFeet, 6));
}

// -----------------------------------------------------------------------------------------------------------
// Lit les valeurs saisies et retourne l'entité UserApproach correspondante.
UserApproach ApproachEditorWidget::value() const
{
    UserApproach a;
    a.runwayIdent             = mRunwayIdentEdit->text();
    a.legSequenceIdent        = mLegSequenceIdentEdit->text();
    a.decisionHeightInFeet    = mDecisionHeightEdit->text().toDouble();
    a.minimumDescentInFeet    = mMinimumDescentEdit->text().toDouble();
    return a;
}

// -----------------------------------------------------------------------------------------------------------
// Donne le focus au champ d'ident runway et sélectionne son contenu.
void ApproachEditorWidget::focusFirstField()
{
    mRunwayIdentEdit->setFocus();
    mRunwayIdentEdit->selectAll();
}

// -----------------------------------------------------------------------------------------------------------
// Affiche le texte donné dans la zone d'aperçu.
void ApproachEditorWidget::setPreviewLine(const QString& text)
{
    mPreview->setPlainText(text);
}
