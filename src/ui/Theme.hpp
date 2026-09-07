// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

#pragma once

#include <QObject>
#include <QColor>
#include <QString>

class Theme : public QObject {
    Q_OBJECT

    Q_PROPERTY(int themeMode READ themeMode WRITE setThemeMode NOTIFY themeModeChanged)
    Q_PROPERTY(bool isDark READ isDark NOTIFY themeChanged)

    // Surfaces, darkest to lightest.
    Q_PROPERTY(QColor bgDark READ bgDark NOTIFY themeChanged)
    Q_PROPERTY(QColor bgElevated READ bgElevated NOTIFY themeChanged)
    Q_PROPERTY(QColor cardBg READ cardBg NOTIFY themeChanged)
    Q_PROPERTY(QColor cardHover READ cardHover NOTIFY themeChanged)
    Q_PROPERTY(QColor cardBorder READ cardBorder NOTIFY themeChanged)
    Q_PROPERTY(QColor separator READ separator NOTIFY themeChanged)

    // Interactive surfaces (buttons, the big connect dial).
    Q_PROPERTY(QColor controlBg READ controlBg NOTIFY themeChanged)
    Q_PROPERTY(QColor controlBgHover READ controlBgHover NOTIFY themeChanged)
    Q_PROPERTY(QColor controlBorder READ controlBorder NOTIFY themeChanged)
    Q_PROPERTY(QColor controlBorderHover READ controlBorderHover NOTIFY themeChanged)
    Q_PROPERTY(QColor rimHighlight READ rimHighlight NOTIFY themeChanged)
    Q_PROPERTY(QColor rimHighlightHover READ rimHighlightHover NOTIFY themeChanged)

    // Text.
    Q_PROPERTY(QColor textPrimary READ textPrimary NOTIFY themeChanged)
    Q_PROPERTY(QColor textSecondary READ textSecondary NOTIFY themeChanged)
    Q_PROPERTY(QColor textMuted READ textMuted NOTIFY themeChanged)
    Q_PROPERTY(QColor textInverted READ textInverted NOTIFY themeChanged)

    Q_PROPERTY(QColor accentWhite READ accentWhite NOTIFY themeChanged)
    Q_PROPERTY(QColor accentDim READ accentDim NOTIFY themeChanged)

    // Status, encoded as lightness only.
    Q_PROPERTY(QColor statusStrong READ statusStrong NOTIFY themeChanged)
    Q_PROPERTY(QColor statusMedium READ statusMedium NOTIFY themeChanged)
    Q_PROPERTY(QColor statusWeak READ statusWeak NOTIFY themeChanged)

    Q_PROPERTY(int radiusSmall READ radiusSmall CONSTANT)
    Q_PROPERTY(int radiusMedium READ radiusMedium CONSTANT)
    Q_PROPERTY(int radiusLarge READ radiusLarge CONSTANT)
    Q_PROPERTY(int radiusPill READ radiusPill CONSTANT)

    // 4pt spacing scale.
    Q_PROPERTY(int spacingXs READ spacingXs CONSTANT)
    Q_PROPERTY(int spacingSm READ spacingSm CONSTANT)
    Q_PROPERTY(int spacingMd READ spacingMd CONSTANT)
    Q_PROPERTY(int spacingLg READ spacingLg CONSTANT)
    Q_PROPERTY(int spacingXl READ spacingXl CONSTANT)

    // Animation durations, so motion stays consistent across views.
    Q_PROPERTY(int durationFast READ durationFast CONSTANT)
    Q_PROPERTY(int durationNormal READ durationNormal CONSTANT)
    Q_PROPERTY(int durationSlow READ durationSlow CONSTANT)

    Q_PROPERTY(QString fontMono READ fontMono CONSTANT)
    Q_PROPERTY(QString fontSans READ fontSans CONSTANT)

    // Latency tiers, kept next to the colours that render them.
    Q_PROPERTY(int pingFastMs READ pingFastMs CONSTANT)
    Q_PROPERTY(int pingMediumMs READ pingMediumMs CONSTANT)

public:
    enum ThemeMode {
        Dark = 0,
        Light = 1,
        System = 2
    };
    Q_ENUM(ThemeMode)

    explicit Theme(QObject *parent = nullptr);
    static Theme *instance();

    int themeMode() const;
    Q_INVOKABLE void setThemeMode(int mode);
    bool isDark() const;
    Q_INVOKABLE QString icon(const QString &path, bool isDark) const;
    Q_INVOKABLE QString icon(const QString &path) const;

    QColor bgDark() const;
    QColor bgElevated() const;
    QColor cardBg() const;
    QColor cardHover() const;
    QColor cardBorder() const;
    QColor separator() const;

    QColor controlBg() const;
    QColor controlBgHover() const;
    QColor controlBorder() const;
    QColor controlBorderHover() const;
    QColor rimHighlight() const;
    QColor rimHighlightHover() const;

    QColor textPrimary() const;
    QColor textSecondary() const;
    QColor textMuted() const;
    QColor textInverted() const;

    QColor accentWhite() const;
    QColor accentDim() const;

    QColor statusStrong() const;
    QColor statusMedium() const;
    QColor statusWeak() const;

    int radiusSmall() const { return 8; }
    int radiusMedium() const { return 16; }
    int radiusLarge() const { return 20; }
    int radiusPill() const { return 999; }

    int spacingXs() const { return 4; }
    int spacingSm() const { return 8; }
    int spacingMd() const { return 16; }
    int spacingLg() const { return 24; }
    int spacingXl() const { return 32; }

    int durationFast() const { return 120; }
    int durationNormal() const { return 200; }
    int durationSlow() const { return 400; }

    QString fontMono() const { return QStringLiteral("monospace"); }
    QString fontSans() const { return QStringLiteral("sans-serif"); }

    int pingFastMs() const { return 100; }
    int pingMediumMs() const { return 250; }

signals:
    void themeChanged();
    void themeModeChanged(int mode);

private:
    static Theme *s_instance;
    int m_themeMode = Dark;
};
