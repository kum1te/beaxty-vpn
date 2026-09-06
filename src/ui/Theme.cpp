// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

#include "Theme.hpp"
#include "src/core/AppPrefs.hpp"
#include <QGuiApplication>
#include <QStyleHints>
#include <QDebug>

Theme *Theme::s_instance = nullptr;

Theme::Theme(QObject *parent) : QObject(parent) {
    s_instance = this;
    m_themeMode = AppPrefs::getInt(QStringLiteral("theme_mode"), Dark);

    if (QGuiApplication::styleHints()) {
        connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, [this]() {
            if (m_themeMode == System) {
                emit themeChanged();
            }
        });
    }
}

Theme *Theme::instance() {
    return s_instance;
}

int Theme::themeMode() const {
    return m_themeMode;
}

void Theme::setThemeMode(int mode) {
    if (m_themeMode != mode) {
        m_themeMode = mode;
        AppPrefs::setInt(QStringLiteral("theme_mode"), mode);
        emit themeModeChanged(mode);
        emit themeChanged();
        qDebug() << "[Theme] Switched theme mode to:" << mode << "isDark:" << isDark();
    }
}

bool Theme::isDark() const {
    if (m_themeMode == Dark) return true;
    if (m_themeMode == Light) return false;
    // System theme:
    if (QGuiApplication::styleHints()) {
        return QGuiApplication::styleHints()->colorScheme() != Qt::ColorScheme::Light;
    }
    return true;
}

QColor Theme::bgDark() const {
    return isDark() ? QColor(0x08, 0x08, 0x08) : QColor(0xF8, 0xF9, 0xFA);
}

QColor Theme::bgElevated() const {
    return isDark() ? QColor(0x12, 0x12, 0x14) : QColor(0xFF, 0xFF, 0xFF);
}

QColor Theme::cardBg() const {
    return isDark() ? QColor(0x14, 0x14, 0x16) : QColor(0xFF, 0xFF, 0xFF);
}

QColor Theme::cardHover() const {
    return isDark() ? QColor(0x1B, 0x1B, 0x1E) : QColor(0xF3, 0xF4, 0xF6);
}

QColor Theme::cardBorder() const {
    return isDark() ? QColor(0x23, 0x23, 0x26) : QColor(0xE5, 0xE7, 0xEB);
}

QColor Theme::separator() const {
    return isDark() ? QColor(0x1E, 0x1E, 0x22) : QColor(0xE5, 0xE7, 0xEB);
}

QColor Theme::controlBg() const {
    return isDark() ? QColor(0x13, 0x13, 0x16) : QColor(0xF3, 0xF4, 0xF6);
}

QColor Theme::controlBgHover() const {
    return isDark() ? QColor(0x1D, 0x1D, 0x21) : QColor(0xE5, 0xE7, 0xEB);
}

QColor Theme::controlBorder() const {
    return isDark() ? QColor(0x26, 0x26, 0x2B) : QColor(0xD1, 0xD5, 0xDB);
}

QColor Theme::controlBorderHover() const {
    return isDark() ? QColor(0x45, 0x45, 0x4C) : QColor(0x9C, 0xA3, 0xAF);
}

QColor Theme::rimHighlight() const {
    return isDark() ? QColor(0x32, 0x32, 0x38) : QColor(0xFF, 0xFF, 0xFF);
}

QColor Theme::rimHighlightHover() const {
    return isDark() ? QColor(0x5E, 0x5E, 0x66) : QColor(0xF9, 0xFA, 0xFB);
}

QColor Theme::textPrimary() const {
    return isDark() ? QColor(0xFF, 0xFF, 0xFF) : QColor(0x11, 0x18, 0x27);
}

QColor Theme::textSecondary() const {
    return isDark() ? QColor(0x7E, 0x7E, 0x87) : QColor(0x6B, 0x72, 0x80);
}

QColor Theme::textMuted() const {
    return isDark() ? QColor(0x48, 0x48, 0x4E) : QColor(0x9C, 0xA3, 0xAF);
}

QColor Theme::textInverted() const {
    return isDark() ? QColor(0x08, 0x08, 0x0A) : QColor(0xFF, 0xFF, 0xFF);
}

QColor Theme::accentWhite() const {
    return isDark() ? QColor(0xFF, 0xFF, 0xFF) : QColor(0x11, 0x18, 0x27);
}

QColor Theme::accentDim() const {
    return isDark() ? QColor(0x2C, 0x2C, 0x30) : QColor(0xE5, 0xE7, 0xEB);
}

QColor Theme::statusStrong() const {
    return isDark() ? QColor(0xFF, 0xFF, 0xFF) : QColor(0x11, 0x18, 0x27);
}

QColor Theme::statusMedium() const {
    return isDark() ? QColor(0x9A, 0x9A, 0xA3) : QColor(0x6B, 0x72, 0x80);
}

QColor Theme::statusWeak() const {
    return isDark() ? QColor(0x55, 0x55, 0x5C) : QColor(0x9C, 0xA3, 0xAF);
}
