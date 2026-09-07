// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

#include "AppPrefs.hpp"

#include "3rdparty/throne/include/global/Configs.hpp"
#include "3rdparty/throne/include/database/DatabaseManager.h"

#include <QDebug>

namespace {
    bool ensureTable() {
        if (!Configs::dataManager) return false;
        static bool created = false;
        if (created) return true;
        try {
            Configs::dataManager->getDatabase().execThrow(
                "CREATE TABLE IF NOT EXISTS beaxty_prefs (key TEXT PRIMARY KEY, value TEXT)");
            created = true;
        } catch (const std::exception &e) {
            qWarning() << "[AppPrefs] Failed to create prefs table:" << e.what();
            return false;
        }
        return true;
    }
}

namespace AppPrefs {

bool has(const QString &key) {
    if (!ensureTable()) return false;
    try {
        auto q = Configs::dataManager->getDatabase().queryThrow(
            "SELECT value FROM beaxty_prefs WHERE key = ?", key.toStdString());
        return q && q->executeStep();
    } catch (const std::exception &e) {
        qWarning() << "[AppPrefs] has(" << key << ") failed:" << e.what();
        return false;
    }
}

QString getString(const QString &key, const QString &fallback) {
    if (!ensureTable()) return fallback;
    try {
        auto q = Configs::dataManager->getDatabase().queryThrow(
            "SELECT value FROM beaxty_prefs WHERE key = ?", key.toStdString());
        if (q && q->executeStep()) {
            return QString::fromStdString(q->getColumn(0).getString());
        }
    } catch (const std::exception &e) {
        qWarning() << "[AppPrefs] getString(" << key << ") failed:" << e.what();
    }
    return fallback;
}

void setString(const QString &key, const QString &value) {
    if (!ensureTable()) return;
    try {
        Configs::dataManager->getDatabase().execThrow(
            "INSERT INTO beaxty_prefs (key, value) VALUES (?, ?) "
            "ON CONFLICT(key) DO UPDATE SET value = excluded.value",
            key.toStdString(), value.toStdString());
    } catch (const std::exception &e) {
        qWarning() << "[AppPrefs] setString(" << key << ") failed:" << e.what();
    }
}

bool getBool(const QString &key, bool fallback) {
    QString v = getString(key);
    if (v.isEmpty()) return fallback;
    return v == QStringLiteral("1") || v.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0;
}

void setBool(const QString &key, bool value) {
    setString(key, value ? QStringLiteral("1") : QStringLiteral("0"));
}

int getInt(const QString &key, int fallback) {
    QString v = getString(key);
    if (v.isEmpty()) return fallback;
    bool ok = false;
    int parsed = v.toInt(&ok);
    return ok ? parsed : fallback;
}

void setInt(const QString &key, int value) {
    setString(key, QString::number(value));
}

} // namespace AppPrefs

AppPrefsService *AppPrefsService::s_instance = nullptr;

AppPrefsService::AppPrefsService(QObject *parent)
    : QObject(parent)
{
    s_instance = this;
    m_sidebarCollapsed = AppPrefs::getBool(QStringLiteral("sidebar_collapsed"), false);
}

AppPrefsService *AppPrefsService::instance() {
    return s_instance;
}

bool AppPrefsService::sidebarCollapsed() const {
    return m_sidebarCollapsed;
}

void AppPrefsService::setSidebarCollapsed(bool collapsed) {
    if (m_sidebarCollapsed != collapsed) {
        m_sidebarCollapsed = collapsed;
        AppPrefs::setBool(QStringLiteral("sidebar_collapsed"), collapsed);
        emit sidebarCollapsedChanged();
        emit prefChanged(QStringLiteral("sidebar_collapsed"));
    }
}

bool AppPrefsService::has(const QString &key) const {
    return AppPrefs::has(key);
}

QString AppPrefsService::getString(const QString &key, const QString &fallback) const {
    return AppPrefs::getString(key, fallback);
}

void AppPrefsService::setString(const QString &key, const QString &value) {
    AppPrefs::setString(key, value);
    emit prefChanged(key);
}

bool AppPrefsService::getBool(const QString &key, bool fallback) const {
    if (key == QStringLiteral("sidebar_collapsed")) {
        return m_sidebarCollapsed;
    }
    return AppPrefs::getBool(key, fallback);
}

void AppPrefsService::setBool(const QString &key, bool value) {
    if (key == QStringLiteral("sidebar_collapsed")) {
        setSidebarCollapsed(value);
        return;
    }
    AppPrefs::setBool(key, value);
    emit prefChanged(key);
}

int AppPrefsService::getInt(const QString &key, int fallback) const {
    return AppPrefs::getInt(key, fallback);
}

void AppPrefsService::setInt(const QString &key, int value) {
    AppPrefs::setInt(key, value);
    emit prefChanged(key);
}
