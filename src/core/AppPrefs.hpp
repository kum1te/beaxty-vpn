// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

#pragma once

#include <QString>

// Small key/value store for wrapper-only preferences that have no counterpart in
// Throne's SettingsRepo (auto-connect, kill switch, first-run marker, ...).
//
// Throne's own settings must keep going through SettingsRepo so that upstream
// stays the single owner of its schema; this table is purely ours and is created
// lazily inside the same SQLite file.
#include <QObject>

class AppPrefsService : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool sidebarCollapsed READ sidebarCollapsed WRITE setSidebarCollapsed NOTIFY sidebarCollapsedChanged)

public:
    explicit AppPrefsService(QObject *parent = nullptr);
    static AppPrefsService *instance();

    bool sidebarCollapsed() const;
    void setSidebarCollapsed(bool collapsed);

    Q_INVOKABLE bool has(const QString &key) const;
    Q_INVOKABLE QString getString(const QString &key, const QString &fallback = QString()) const;
    Q_INVOKABLE void setString(const QString &key, const QString &value);
    Q_INVOKABLE bool getBool(const QString &key, bool fallback = false) const;
    Q_INVOKABLE void setBool(const QString &key, bool value);
    Q_INVOKABLE int getInt(const QString &key, int fallback = 0) const;
    Q_INVOKABLE void setInt(const QString &key, int value);

signals:
    void sidebarCollapsedChanged();
    void prefChanged(const QString &key);

private:
    static AppPrefsService *s_instance;
    bool m_sidebarCollapsed{false};
};

namespace AppPrefs {
    bool has(const QString &key);

    QString getString(const QString &key, const QString &fallback = QString());
    void setString(const QString &key, const QString &value);

    bool getBool(const QString &key, bool fallback);
    void setBool(const QString &key, bool value);

    int getInt(const QString &key, int fallback);
    void setInt(const QString &key, int value);
}
