// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

#include "EmojiFont.hpp"

#include <QDebug>
#include <QFont>
#include <QFontDatabase>
#include <QFontInfo>

void registerEmojiFallback() {
#if defined(Q_OS_LINUX)
    const int fontId = QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/NotoColorEmoji.ttf"));
    if (fontId < 0) {
        qWarning() << "[Fonts] Could not load the bundled Noto Color Emoji font";
        return;
    }

    const QStringList families = QFontDatabase::applicationFontFamilies(fontId);
    if (families.isEmpty()) {
        qWarning() << "[Fonts] Bundled Noto Color Emoji font has no registered family";
        return;
    }
    // Resolve the system UI alias before registering the emoji font. Otherwise
    // QFontInfo may report the emoji fallback itself as the resolved family.
    QString uiFamily = QFontInfo(QFontDatabase::systemFont(QFontDatabase::GeneralFont)).family();
    if (uiFamily.isEmpty() || uiFamily == families.first()) {
        uiFamily = QFontInfo(QFont(QStringLiteral("sans-serif"))).family();
    }

    // Common includes ASCII digits and punctuation as well as flag sequences.
    // Prefer the actual system UI font for those glyphs, then use the bundled
    // emoji font for flags and symbols that the UI font cannot render.
    QFontDatabase::addApplicationFallbackFontFamily(QChar::Script_Common, families.first());
    if (!uiFamily.isEmpty() && uiFamily != families.first()) {
        // Application fallback families are checked in reverse insertion order.
        QFontDatabase::addApplicationFallbackFontFamily(QChar::Script_Common, uiFamily);
    }
#endif
}
