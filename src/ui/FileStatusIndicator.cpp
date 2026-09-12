#include "FileStatusIndicator.h"

#include <QFont>
#include <QFontMetrics>
#include <QGridLayout>
#include <QLabel>
#include <QPalette>
#include <QSizePolicy>

namespace {
// L'encadré occupe une place abondante en bas à droite : on agrandit donc
// l'incrustation par ses marges et ses espacements, en CONSERVANT la taille de
// police d'origine de l'application. Ces quatre valeurs sont les seuls réglages
// à ajuster pour changer l'encombrement.
constexpr int kMarginH   = 36; // marge horizontale (gauche/droite)
constexpr int kMarginV   = 28; // marge verticale (haut/bas)
constexpr int kSpacingH  = 60; // espace entre libellés et valeurs
constexpr int kSpacingV  = 14; // espace entre les lignes
}

FileStatusIndicator::FileStatusIndicator(QWidget* parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("FileStatusIndicator"));
    setFrameShape(QFrame::StyledPanel);
    setFrameShadow(QFrame::Raised);

    // Fond et texte tirés de la PALETTE de l'application : l'indicateur reste
    // lisible en thème clair comme en thème sombre (l'ancien fond clair codé
    // en dur donnait du texte blanc sur gris clair en thème sombre).
    setAutoFillBackground(true);
    setBackgroundRole(QPalette::Base);

    auto* layout = new QGridLayout(this);
    layout->setContentsMargins(kMarginH, kMarginV, kMarginH, kMarginV);
    layout->setHorizontalSpacing(kSpacingH);
    layout->setVerticalSpacing(kSpacingV);

    auto makeLabel = [this](const QString& text, bool bold) {
        auto* label = new QLabel(text, this);
        label->setForegroundRole(QPalette::Text);
        if (bold) {
            QFont f = label->font();
            f.setBold(true);
            label->setFont(f);
        }
        return label;
    };

    auto* title = makeLabel(QStringLiteral("État des fichiers"), true);
    layout->addWidget(title, 0, 0, 1, 2);

    layout->addWidget(makeLabel(QStringLiteral("Nav1.db"),     true), 1, 0);
    layout->addWidget(makeLabel(QStringLiteral("Nav1.txt"),    true), 2, 0);
    layout->addWidget(makeLabel(QStringLiteral("Nav1-2.db"),   true), 3, 0);
    layout->addWidget(makeLabel(QStringLiteral("Jeu de .txt"), true), 4, 0);

    mNav1DbValue  = makeLabel(QString(), false);
    mNav1TxtValue = makeLabel(QString(), false);
    mNav12DbValue = makeLabel(QString(), false);
    mTxtSetValue  = makeLabel(QString(), false);

    layout->addWidget(mNav1DbValue,  1, 1);
    layout->addWidget(mNav1TxtValue, 2, 1);
    layout->addWidget(mNav12DbValue, 3, 1);
    layout->addWidget(mTxtSetValue,  4, 1);

    // Largeurs minimales des colonnes, calculées sur la police réelle :
    //   - colonne des libellés de fichiers : x1.5 (noms plus tronqués) ;
    //   - colonne des états                : x2 (place pour « 2609 (increased) »).
    QFont boldFont = font();
    boldFont.setBold(true);
    const QFontMetrics boldFm(boldFont);
    int nameWidth = 0;
    for (const QString& name : { QStringLiteral("Nav1.db"), QStringLiteral("Nav1.txt"),
                                 QStringLiteral("Nav1-2.db"), QStringLiteral("Jeu de .txt") })
        nameWidth = qMax(nameWidth, boldFm.horizontalAdvance(name));

    const QFontMetrics valueFm(font());
    int valueWidth = 0;
    for (const QString& value : { QStringLiteral("N.C."), QStringLiteral("2609"),
                                  QStringLiteral("2609 (reduced)"), QStringLiteral("2609 (increased)") })
        valueWidth = qMax(valueWidth, valueFm.horizontalAdvance(value));

    layout->setColumnMinimumWidth(0, qRound(nameWidth  * 1.5));
    layout->setColumnMinimumWidth(1, qRound(valueWidth * 2.0));

    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    // État initial : cf. colonne « Ouverture appli » du tableau.
    notifyAppOpened();
}

QString FileStatusIndicator::airacValue(const QString& suffix) const
{
    if (mAirac.isEmpty())
        return QStringLiteral("N.C.");
    if (suffix.isEmpty())
        return mAirac;
    return QStringLiteral("%1 (%2)").arg(mAirac, suffix);
}

void FileStatusIndicator::setAirAirac(const QString& airac)
{
    mAirac = airac.trimmed();
}

void FileStatusIndicator::notifyAppOpened()
{
    mNav1Db = QStringLiteral("N.C.");
    mNav1Txt = airacValue();
    mNav12Db = QStringLiteral("N.C.");
    // mTxtSet reste inchangé (vide au démarrage).
    refresh();
}

void FileStatusIndicator::notifyAirportToProject()
{
    mNav1Db = airacValue();
    mNav1Txt = airacValue(QStringLiteral("reduced"));
    refresh();
}

void FileStatusIndicator::notifyReloadWorld()
{
    mNav1Db = airacValue();
    mNav1Txt = airacValue();
    refresh();
}

void FileStatusIndicator::notifyExportTxt(const QString& project)
{
    if (!project.trimmed().isEmpty())
        mTxtSet = project.trimmed();
    refresh();
}

void FileStatusIndicator::notifyDecodeNav1Db()
{
    mNav1Db = airacValue();
    mNav1Txt = airacValue();
    refresh();
}

void FileStatusIndicator::notifyIntegrate()
{
    mNav1Db = airacValue();
    mNav1Txt = airacValue(QStringLiteral("increased"));
    mNav12Db = airacValue(QStringLiteral("increased"));
    refresh();
}

void FileStatusIndicator::refresh()
{
    mNav1DbValue->setText(mNav1Db.isEmpty() ? QStringLiteral("") : mNav1Db);
    mNav1TxtValue->setText(mNav1Txt.isEmpty() ? QStringLiteral("") : mNav1Txt);
    mNav12DbValue->setText(mNav12Db.isEmpty() ? QStringLiteral("") : mNav12Db);
    mTxtSetValue->setText(mTxtSet.isEmpty() ? QStringLiteral("") : mTxtSet);
}
