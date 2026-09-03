#include <QComboBox>
#include <QDoubleValidator>
#include <QFormLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QLocale>
#include <QPlainTextEdit>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include "PointEditorWidget.h"
#include "NavDataWriter.h"
#include "TextFormat.h" // writer::format::fixed — même formatage que la ligne texte réelle, décimales par champ
#include "UserToModelConverter.h"

using namespace navstud::model;
using namespace navstud::userdata;
using namespace navstud::conversion;
using namespace navstud::writer;

namespace {
// Nombre de décimales PAR CHAMP, identique à NavDataWriter::formatPointLine
// (12 pour lat/lon, 6 pour magVar/holdCourse, 3 pour holdDist/holdTime) —
// afficher moins de décimales que ce que la ligne texte réserve donnerait
// l'illusion d'une valeur arrondie alors que la donnée stockée, elle, ne
// l'est pas.
QString formatFixed(double v, int decimals) { return format::fixed(v, decimals); }
} // namespace

// -----------------------------------------------------------------------------------------------------------
// Construit l'éditeur de point : crée les champs de saisie avec leurs
// validateurs, le formulaire, la zone d'aperçu, connecte les signaux et
// met à jour l'aperçu.
PointEditorWidget::PointEditorWidget(QWidget* parent)
    : QWidget(parent)
{
    mIdentEdit = new QLineEdit(this);
    mIdentEdit->setMaxLength(6);
    mLatitudeEdit   = new QLineEdit(this);
    mLongitudeEdit  = new QLineEdit(this);
    mMagVarEdit     = new QLineEdit(this);
    mHoldCourseEdit = new QLineEdit(this);
    mHoldDistEdit   = new QLineEdit(this);
    mHoldTimeEdit   = new QLineEdit(this);

    mHoldSideCombo = new QComboBox(this);
    mHoldSideCombo->addItem(QStringLiteral("Aucun (0)"), 0);
    mHoldSideCombo->addItem(QStringLiteral("Gauche (-1)"), -1);
    mHoldSideCombo->addItem(QStringLiteral("Droite (+1)"), 1);

    // Validateurs numériques — empêchent la saisie alphanumérique à la
    // source plutôt que de la tolérer silencieusement (l'ancien value()
    // convertissait tout texte invalide en 0.0 sans prévenir). Les bornes
    // basses tiennent compte des sentinelles -1 ("non spécifié") propres
    // à chaque champ. setLocale(QLocale::c()) est indispensable : sans ça,
    // QDoubleValidator utilise le séparateur décimal de la locale système
    // (la virgule en français) et refuse le point.
    //
    // Le paramètre "decimals" est volontairement généreux (15) plutôt que
    // calé sur la précision réelle du champ (12 pour lat/lon, 6 pour
    // magVar/holdCourse, 3 pour holdDist/holdTime) : QDoubleValidator
    // l'applique comme un PLAFOND STRICT même en cours d'édition — modifier
    // un chiffre au milieu d'un nombre déjà à sa précision cible dépasserait
    // ce plafond et la frappe serait refusée. La précision réellement
    // écrite dans le fichier reste imposée par format::fixed() à la
    // génération, jamais par ce que ce plafond de saisie autorise.
    const QLocale cLocale = QLocale::c();
    constexpr int kEditDecimalsCeiling = 15;

    auto* latValidator = new QDoubleValidator(-90.0, 90.0, kEditDecimalsCeiling, mLatitudeEdit);
    latValidator->setNotation(QDoubleValidator::StandardNotation);
    latValidator->setLocale(cLocale);
    mLatitudeEdit->setValidator(latValidator);

    auto* lonValidator = new QDoubleValidator(-180.0, 180.0, kEditDecimalsCeiling, mLongitudeEdit);
    lonValidator->setNotation(QDoubleValidator::StandardNotation);
    lonValidator->setLocale(cLocale);
    mLongitudeEdit->setValidator(lonValidator);

    auto* magVarValidator = new QDoubleValidator(-360.0, 360.0, kEditDecimalsCeiling, mMagVarEdit);
    magVarValidator->setNotation(QDoubleValidator::StandardNotation);
    magVarValidator->setLocale(cLocale);
    mMagVarEdit->setValidator(magVarValidator);

    auto* holdCourseValidator = new QDoubleValidator(-1.0, 360.0, kEditDecimalsCeiling, mHoldCourseEdit);
    holdCourseValidator->setNotation(QDoubleValidator::StandardNotation);
    holdCourseValidator->setLocale(cLocale);
    mHoldCourseEdit->setValidator(holdCourseValidator);

    auto* holdDistValidator = new QDoubleValidator(-1.0, 99999.0, kEditDecimalsCeiling, mHoldDistEdit);
    holdDistValidator->setNotation(QDoubleValidator::StandardNotation);
    holdDistValidator->setLocale(cLocale);
    mHoldDistEdit->setValidator(holdDistValidator);

    auto* holdTimeValidator = new QDoubleValidator(-1.0, 99999.0, kEditDecimalsCeiling, mHoldTimeEdit);
    holdTimeValidator->setNotation(QDoubleValidator::StandardNotation);
    holdTimeValidator->setLocale(cLocale);
    mHoldTimeEdit->setValidator(holdTimeValidator);

    auto* form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->addRow(QStringLiteral("Ident"), mIdentEdit);
    form->addRow(QStringLiteral("Latitude"), mLatitudeEdit);
    form->addRow(QStringLiteral("Longitude"), mLongitudeEdit);
    form->addRow(QStringLiteral("Variation magnétique"), mMagVarEdit);
    form->addRow(QStringLiteral("Cap hold"), mHoldCourseEdit);
    form->addRow(QStringLiteral("Distance hold (m)"), mHoldDistEdit);
    form->addRow(QStringLiteral("Temps hold"), mHoldTimeEdit);
    form->addRow(QStringLiteral("Sens hold"), mHoldSideCombo);

    auto* formGroup = new QGroupBox(QStringLiteral("Saisie — POINT"), this);
    formGroup->setLayout(form);

    mPreview = new QPlainTextEdit(this);
    mPreview->setReadOnly(true);
    mPreview->setMaximumHeight(64);
    mPreview->setLineWrapMode(QPlainTextEdit::NoWrap);

    auto* previewLayout = new QVBoxLayout;
    previewLayout->addWidget(mPreview);
    auto* previewGroup = new QGroupBox(QStringLiteral("Aperçu — ligne _Point.txt"), this);
    previewGroup->setLayout(previewLayout);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(formGroup, 0, Qt::AlignLeft);
    mainLayout->addWidget(previewGroup);
    mainLayout->addStretch(1);

    connect(mIdentEdit, &QLineEdit::textChanged, this, &PointEditorWidget::onFieldChanged);
    connect(mLatitudeEdit, &QLineEdit::textChanged, this, &PointEditorWidget::onFieldChanged);
    connect(mLongitudeEdit, &QLineEdit::textChanged, this, &PointEditorWidget::onFieldChanged);
    connect(mMagVarEdit, &QLineEdit::textChanged, this, &PointEditorWidget::onFieldChanged);
    connect(mHoldCourseEdit, &QLineEdit::textChanged, this, &PointEditorWidget::onFieldChanged);
    connect(mHoldDistEdit, &QLineEdit::textChanged, this, &PointEditorWidget::onFieldChanged);
    connect(mHoldTimeEdit, &QLineEdit::textChanged, this, &PointEditorWidget::onFieldChanged);
    connect(mHoldSideCombo, &QComboBox::currentIndexChanged, this, &PointEditorWidget::onFieldChanged);

    updatePreview();
}

// -----------------------------------------------------------------------------------------------------------
// Charge les valeurs du point dans les champs de saisie, signaux bloqués
// pour éviter de redéclencher onFieldChanged(), puis met à jour l'aperçu.
void PointEditorWidget::setValue(PointId id, const UserPoint& point)
{
    mCurrentId = id;

    // Signaux bloqués pendant le chargement programmatique : setValue() ne
    // doit pas déclencher valueEdited() (cf. onFieldChanged, qui répercute
    // dans le UserProject — on ne veut pas ré-écrire une valeur qu'on vient
    // tout juste de lire).
    const QSignalBlocker b1(mIdentEdit);
    const QSignalBlocker b2(mLatitudeEdit);
    const QSignalBlocker b3(mLongitudeEdit);
    const QSignalBlocker b4(mMagVarEdit);
    const QSignalBlocker b5(mHoldCourseEdit);
    const QSignalBlocker b6(mHoldDistEdit);
    const QSignalBlocker b7(mHoldTimeEdit);
    const QSignalBlocker b8(mHoldSideCombo);

    mIdentEdit->setText(point.ident);
    mLatitudeEdit->setText(formatFixed(point.latitude, 12));
    mLongitudeEdit->setText(formatFixed(point.longitude, 12));
    mMagVarEdit->setText(formatFixed(point.magVar, 6));
    mHoldCourseEdit->setText(formatFixed(point.holdCourse, 6));
    mHoldDistEdit->setText(formatFixed(point.holdDistInMeters, 3));
    mHoldTimeEdit->setText(formatFixed(point.holdTime, 3));
    mHoldSideCombo->setCurrentIndex(mHoldSideCombo->findData(point.holdSide));

    updatePreview();
}

// -----------------------------------------------------------------------------------------------------------
// Lit les valeurs saisies et retourne l'entité UserPoint correspondante.
UserPoint PointEditorWidget::value() const
{
    UserPoint p;
    p.ident             = mIdentEdit->text();
    p.latitude           = mLatitudeEdit->text().toDouble();
    p.longitude          = mLongitudeEdit->text().toDouble();
    p.magVar             = mMagVarEdit->text().toDouble();
    p.holdCourse         = mHoldCourseEdit->text().toDouble();
    p.holdDistInMeters   = mHoldDistEdit->text().toDouble();
    p.holdTime           = mHoldTimeEdit->text().toDouble();
    p.holdSide           = static_cast<qint8>(mHoldSideCombo->currentData().toInt());
    return p;
}

// -----------------------------------------------------------------------------------------------------------
// Donne le focus au champ d'ident et sélectionne son contenu.
void PointEditorWidget::focusIdent()
{
    mIdentEdit->setFocus();
    mIdentEdit->selectAll();
}

// -----------------------------------------------------------------------------------------------------------
// Met à jour l'aperçu puis émet le signal valueEdited() à la modification
// d'un champ.
void PointEditorWidget::onFieldChanged()
{
    updatePreview();
    emit valueEdited();
}

// -----------------------------------------------------------------------------------------------------------
// Convertit la valeur saisie et affiche la ligne _Point.txt générée ou les
// erreurs de conversion dans la zone d'aperçu.
void PointEditorWidget::updatePreview()
{
    const ConversionResult<Point> result = convertPoint(value());
    if (result.isOk())
        mPreview->setPlainText(NavDataWriter::formatPointLine(mCurrentId, result.value()));
    else
        mPreview->setPlainText(QStringLiteral("⚠ ") + result.errors().join(QStringLiteral(" ; ")));
}
