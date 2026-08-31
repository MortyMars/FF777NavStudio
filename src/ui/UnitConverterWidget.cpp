#include "UnitConverterWidget.h"
#include "EditorFieldHelpers.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSizePolicy>

using namespace navstud::ui;

// -----------------------------------------------------------------------------------------------------------
// Construit le convertisseur d'unités : crée les deux champs numériques,
// les libellés et connecte la conversion dans les deux sens au fil de la
// saisie.
UnitConverterWidget::UnitConverterWidget(const QString& labelFrom, const QString& labelTo, double factor, QWidget* parent)
    : QWidget(parent)
    , mFactor(factor)
{
    // Bornes larges plutôt que calées sur un usage précis — même principe
    // que les champs de saisie (cf. EditorFieldHelpers.h) : un convertisseur
    // générique n'a pas de plage "naturelle".
    mFromEdit = makeDoubleField(this, -1000000.0, 1000000.0);
    mToEdit   = makeDoubleField(this, -1000000.0, 1000000.0);
    mFromEdit->setFixedWidth(87); // 58 * 1.5 — assez pour plusieurs décimales sans coupure
    mToEdit->setFixedWidth(87);
    mFromEdit->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    mToEdit->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    // Style explicite plutôt que la palette héritée du bandeau (sombre sur
    // certains styles système, rendant un champ sans fond propre illisible
    // — texte noir sur fond noir) : fond clair fixe, bordure visible, quel
    // que soit le thème de l'appli.
    const QString fieldStyle = QStringLiteral(
        "QLineEdit { background-color: #f0f0f0; color: #202020; "
        "border: 1px solid #888888; border-radius: 3px; padding: 1px 3px; }");
    mFromEdit->setStyleSheet(fieldStyle);
    mToEdit->setStyleSheet(fieldStyle);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(3, 0, 3, 0);
    layout->setSpacing(3);
    layout->addWidget(mFromEdit);
    layout->addWidget(new QLabel(labelFrom, this));
    layout->addWidget(new QLabel(QStringLiteral("="), this));
    layout->addWidget(mToEdit);
    layout->addWidget(new QLabel(labelTo, this));

    // Empêche le QToolBar d'étirer ce widget pour occuper l'espace
    // disponible (comportement par défaut de QToolBar::addWidget avec un
    // widget "expanding" — c'est ce qui créait le grand vide entre les deux
    // convertisseurs) : taille fixée à son contenu minimal.
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);

    // textEdited (jamais déclenché par setText) plutôt que textChanged :
    // évite toute boucle champ->champ, pas besoin de QSignalBlocker.
    connect(mFromEdit, &QLineEdit::textEdited, this, [this](const QString& text) {
        bool ok = false;
        const double value = text.toDouble(&ok);
        mToEdit->setText(ok ? QString::number(value * mFactor, 'f', 4) : QString());
    });
    connect(mToEdit, &QLineEdit::textEdited, this, [this](const QString& text) {
        bool ok = false;
        const double value = text.toDouble(&ok);
        mFromEdit->setText(ok ? QString::number(value / mFactor, 'f', 4) : QString());
    });
}
