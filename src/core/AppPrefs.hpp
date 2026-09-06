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
namespace AppPrefs {
    bool has(const QString &key);

    QString getString(const QString &key, const QString &fallback = QString());
    void setString(const QString &key, const QString &value);

    bool getBool(const QString &key, bool fallback);
    void setBool(const QString &key, bool value);

    int getInt(const QString &key, int fallback);
    void setInt(const QString &key, int value);
}
