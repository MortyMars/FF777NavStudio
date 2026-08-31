#include "WaypointEditorWidget.h"
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
// Construit l'éditeur de waypoint : crée le champ d'ident, le formulaire,
// la zone d'aperçu et connecte le signal de modification.
WaypointEditorWidget::WaypointEditorWidget(QWidget* parent)
    : QWidget(parent)
{
    mPointIdentEdit = makeIdentField(this, 6);

    auto* form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->addRow(QStringLiteral("Ident du Point"), mPointIdentEdit);

    auto* formGroup = new QGroupBox(QStringLiteral("Saisie — WAYPOINT"), this);
    formGroup->setLayout(form);

    mPreview = new QPlainTextEdit(this);
    mPreview->setReadOnly(true);
    mPreview->setMaximumHeight(64);
    mPreview->setLineWrapMode(QPlainTextEdit::NoWrap);

    auto* previewLayout = new QVBoxLayout;
    previewLayout->addWidget(mPreview);
    auto* previewGroup = new QGroupBox(QStringLiteral("Aperçu — ligne _Waypoint.txt"), this);
    previewGroup->setLayout(previewLayout);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(formGroup, 0, Qt::AlignLeft);
    mainLayout->addWidget(previewGroup);
    mainLayout->addStretch(1);

    connect(mPointIdentEdit, &QLineEdit::textChanged, this, &WaypointEditorWidget::valueEdited);
}

// -----------------------------------------------------------------------------------------------------------
// Charge l'ident du waypoint dans le champ de saisie, signaux bloqués pour
// éviter de redéclencher l'émission de valueEdited().
void WaypointEditorWidget::setValue(WaypointId id, const UserWaypoint& waypoint)
{
    mCurrentId = id;
    const QSignalBlocker blocker(mPointIdentEdit);
    mPointIdentEdit->setText(waypoint.pointIdent);
}

// -----------------------------------------------------------------------------------------------------------
// Lit l'ident saisi et retourne l'entité UserWaypoint correspondante.
UserWaypoint WaypointEditorWidget::value() const
{
    return UserWaypoint{ mPointIdentEdit->text() };
}

// -----------------------------------------------------------------------------------------------------------
// Donne le focus au champ d'ident du point et sélectionne son contenu.
void WaypointEditorWidget::focusFirstField()
{
    mPointIdentEdit->setFocus();
    mPointIdentEdit->selectAll();
}

// -----------------------------------------------------------------------------------------------------------
// Affiche le texte donné dans la zone d'aperçu.
void WaypointEditorWidget::setPreviewLine(const QString& text)
{
    mPreview->setPlainText(text);
}
