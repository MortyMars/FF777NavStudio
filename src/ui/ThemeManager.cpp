#include "ThemeManager.h"

#include <QApplication>
#include <QColor>
#include <QPalette>
#include <QSettings>
#include <QStyle>
#include <QStyleFactory>

namespace navstud::ui {

namespace {

// QSettings : on fournit explicitement organisation/application pour ne PAS
// toucher à QCoreApplication::organizationName(), qui détermine l'emplacement
// de projects.sqlite via QStandardPaths.
const char* kSettingsOrg = "FF777NavStudio";
const char* kSettingsApp = "FF777NavStudio";
const char* kSettingsKey = "appearance/theme";

// Style et palette natifs, mémorisés au premier appel pour pouvoir y revenir.
QString& nativeStyleName()
{
    static QString name;
    return name;
}

QPalette& nativePalette()
{
    static QPalette palette;
    return palette;
}

bool& nativeCaptured()
{
    static bool captured = false;
    return captured;
}

void captureNative()
{
    if (nativeCaptured())
        return;
    if (QApplication::style())
        nativeStyleName() = QApplication::style()->objectName();
    nativePalette() = QApplication::palette();
    nativeCaptured() = true;
}

// Renseigne les rôles 3D (bordures) absents d'une palette construite à la main.
void setFrameColors(QPalette& p, const QColor& light, const QColor& midlight,
                    const QColor& dark, const QColor& mid, const QColor& shadow)
{
    p.setColor(QPalette::Light, light);
    p.setColor(QPalette::Midlight, midlight);
    p.setColor(QPalette::Dark, dark);
    p.setColor(QPalette::Mid, mid);
    p.setColor(QPalette::Shadow, shadow);
}

QPalette lightPalette()
{
    QPalette p;
    p.setColor(QPalette::Window,          QColor(0xF0, 0xF0, 0xF0));
    p.setColor(QPalette::WindowText,      QColor(0x20, 0x20, 0x20));
    p.setColor(QPalette::Base,            QColor(0xFF, 0xFF, 0xFF));
    p.setColor(QPalette::AlternateBase,   QColor(0xF5, 0xF5, 0xF5));
    p.setColor(QPalette::ToolTipBase,     QColor(0xFF, 0xFF, 0xDC));
    p.setColor(QPalette::ToolTipText,     QColor(0x20, 0x20, 0x20));
    p.setColor(QPalette::Text,            QColor(0x20, 0x20, 0x20));
    p.setColor(QPalette::Button,          QColor(0xF0, 0xF0, 0xF0));
    p.setColor(QPalette::ButtonText,      QColor(0x20, 0x20, 0x20));
    p.setColor(QPalette::BrightText,      QColor(0xFF, 0x00, 0x00));
    p.setColor(QPalette::Link,            QColor(0x00, 0x60, 0xC0));
    p.setColor(QPalette::Highlight,       QColor(0x30, 0x7F, 0xD8));
    p.setColor(QPalette::HighlightedText, QColor(0xFF, 0xFF, 0xFF));
    p.setColor(QPalette::PlaceholderText, QColor(0x80, 0x80, 0x80));
    setFrameColors(p, QColor(0xFF, 0xFF, 0xFF), QColor(0xE8, 0xE8, 0xE8),
                   QColor(0xA0, 0xA0, 0xA0), QColor(0xC0, 0xC0, 0xC0),
                   QColor(0x70, 0x70, 0x70));

    p.setColor(QPalette::Disabled, QPalette::Text,       QColor(0x9A, 0x9A, 0x9A));
    p.setColor(QPalette::Disabled, QPalette::WindowText, QColor(0x9A, 0x9A, 0x9A));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0x9A, 0x9A, 0x9A));
    p.setColor(QPalette::Disabled, QPalette::Highlight,  QColor(0xE0, 0xE0, 0xE0));
    p.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(0x9A, 0x9A, 0x9A));
    return p;
}

QPalette darkPalette()
{
    QPalette p;
    p.setColor(QPalette::Window,          QColor(0x35, 0x35, 0x35));
    p.setColor(QPalette::WindowText,      QColor(0xE6, 0xE6, 0xE6));
    p.setColor(QPalette::Base,            QColor(0x27, 0x27, 0x27));
    p.setColor(QPalette::AlternateBase,   QColor(0x32, 0x32, 0x32));
    p.setColor(QPalette::ToolTipBase,     QColor(0x2B, 0x2B, 0x2B));
    p.setColor(QPalette::ToolTipText,     QColor(0xE6, 0xE6, 0xE6));
    p.setColor(QPalette::Text,            QColor(0xE6, 0xE6, 0xE6));
    p.setColor(QPalette::Button,          QColor(0x3A, 0x3A, 0x3A));
    p.setColor(QPalette::ButtonText,      QColor(0xE6, 0xE6, 0xE6));
    p.setColor(QPalette::BrightText,      QColor(0xFF, 0x5C, 0x5C));
    p.setColor(QPalette::Link,            QColor(0x4D, 0xA6, 0xFF));
    p.setColor(QPalette::Highlight,       QColor(0x2A, 0x82, 0xDA));
    p.setColor(QPalette::HighlightedText, QColor(0xFF, 0xFF, 0xFF));
    p.setColor(QPalette::PlaceholderText, QColor(0x9A, 0x9A, 0x9A));
    setFrameColors(p, QColor(0x4A, 0x4A, 0x4A), QColor(0x40, 0x40, 0x40),
                   QColor(0x23, 0x23, 0x23), QColor(0x2E, 0x2E, 0x2E),
                   QColor(0x18, 0x18, 0x18));

    p.setColor(QPalette::Disabled, QPalette::Text,       QColor(0x7F, 0x7F, 0x7F));
    p.setColor(QPalette::Disabled, QPalette::WindowText, QColor(0x7F, 0x7F, 0x7F));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0x7F, 0x7F, 0x7F));
    p.setColor(QPalette::Disabled, QPalette::Highlight,  QColor(0x4A, 0x4A, 0x4A));
    p.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(0x9A, 0x9A, 0x9A));
    return p;
}

} // namespace

void ThemeManager::apply(Theme theme)
{
    captureNative();

    if (theme == Theme::System) {
        // Retour au style et à la palette natifs (look macOS d'origine).
        if (!nativeStyleName().isEmpty()) {
            if (QStyle* native = QStyleFactory::create(nativeStyleName()))
                QApplication::setStyle(native);
        }
        QApplication::setPalette(nativePalette());
        return;
    }

    // Thèmes explicites : style Fusion + palette dédiée, pour un rendu
    // homogène indépendamment de l'OS.
    if (QStyle* fusion = QStyleFactory::create(QStringLiteral("Fusion")))
        QApplication::setStyle(fusion);

    QApplication::setPalette(theme == Theme::Dark ? darkPalette() : lightPalette());
}

ThemeManager::Theme ThemeManager::loadSavedTheme()
{
    QSettings settings(QString::fromLatin1(kSettingsOrg), QString::fromLatin1(kSettingsApp));
    const QString value = settings.value(QString::fromLatin1(kSettingsKey),
                                         QStringLiteral("system")).toString().toLower();
    if (value == QLatin1String("light") || value == QLatin1String("clair"))
        return Theme::Light;
    if (value == QLatin1String("dark") || value == QLatin1String("sombre"))
        return Theme::Dark;
    return Theme::System;
}

void ThemeManager::saveTheme(Theme theme)
{
    QSettings settings(QString::fromLatin1(kSettingsOrg), QString::fromLatin1(kSettingsApp));
    QString value = QStringLiteral("system");
    if (theme == Theme::Light)
        value = QStringLiteral("light");
    else if (theme == Theme::Dark)
        value = QStringLiteral("dark");
    settings.setValue(QString::fromLatin1(kSettingsKey), value);
}

QString ThemeManager::themeName(Theme theme)
{
    switch (theme) {
    case Theme::Light:  return QStringLiteral("Clair");
    case Theme::Dark:   return QStringLiteral("Sombre");
    case Theme::System:
    default:            return QStringLiteral("Système");
    }
}

} // namespace navstud::ui
