// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

#include "DeviceIdentity.hpp"
#include "3rdparty/throne/include/global/Configs.hpp"
#include "3rdparty/throne/include/database/DatabaseManager.h"
#include "3rdparty/throne/include/database/SettingsRepo.h"

#include <QCryptographicHash>
#include <QFile>
#include <QSysInfo>
#include <QDebug>
#include <QGuiApplication>
#include <QClipboard>

DeviceIdentity *DeviceIdentity::s_instance = nullptr;

DeviceIdentity::DeviceIdentity(QObject *parent) : QObject(parent) {
    s_instance = this;
    m_hwidHash = computeSha256Hwid();

    // The default-ON decision belongs to first-run seeding in ThroneEngine::initialize();
    // here we only keep the transmitted parameter in step with the current HWID so a
    // machine-id change does not leave a stale value behind.
    if (Configs::dataManager && Configs::dataManager->settingsRepo) {
        auto *settings = Configs::dataManager->settingsRepo.get();
        QString expected = QStringLiteral("hwid=%1").arg(m_hwidHash);
        if (settings->sub_send_hwid && settings->sub_custom_hwid_params != expected) {
            settings->sub_custom_hwid_params = expected;
            settings->Save();
        }
    }
}

DeviceIdentity *DeviceIdentity::instance() {
    return s_instance;
}

QString DeviceIdentity::computeSha256Hwid() {
    QString rawId;

#ifdef Q_OS_LINUX
    QFile f1(QStringLiteral("/etc/machine-id"));
    if (f1.exists() && f1.open(QIODevice::ReadOnly | QIODevice::Text)) {
        rawId = QString::fromUtf8(f1.readAll()).trimmed();
        f1.close();
    }
    if (rawId.isEmpty()) {
        QFile f2(QStringLiteral("/var/lib/dbus/machine-id"));
        if (f2.exists() && f2.open(QIODevice::ReadOnly | QIODevice::Text)) {
            rawId = QString::fromUtf8(f2.readAll()).trimmed();
            f2.close();
        }
    }
#endif

    if (rawId.isEmpty()) {
        rawId = QString::fromLatin1(QSysInfo::machineUniqueId());
    }
    if (rawId.isEmpty()) {
        rawId = QStringLiteral("%1-%2").arg(QSysInfo::machineHostName(), QSysInfo::productType());
    }

    QByteArray hash = QCryptographicHash::hash(rawId.toUtf8(), QCryptographicHash::Sha256);
    return QString::fromLatin1(hash.toHex());
}

bool DeviceIdentity::isHwidEnabled() const {
    if (Configs::dataManager && Configs::dataManager->settingsRepo) {
        return Configs::dataManager->settingsRepo->sub_send_hwid;
    }
    return true;
}

void DeviceIdentity::setHwidEnabled(bool enabled) {
    auto *settings = Configs::dataManager ? Configs::dataManager->settingsRepo.get() : nullptr;
    if (!settings || settings->sub_send_hwid == enabled) return;

    settings->sub_send_hwid = enabled;
    if (enabled) {
        settings->sub_custom_hwid_params = QString("hwid=%1").arg(m_hwidHash);
    } else {
        // Leaving the parameter behind would keep leaking the HWID on every
        // subscription refresh after the user opted out.
        settings->sub_custom_hwid_params.clear();
    }
    settings->Save();
    emit hwidEnabledChanged(enabled);
}

QString DeviceIdentity::rawHwid() const {
    return m_hwidHash;
}

QString DeviceIdentity::sanitizedHwid() const {
    if (m_hwidHash.length() > 16) {
        return m_hwidHash.left(8) + QStringLiteral("...") + m_hwidHash.right(8);
    }
    return m_hwidHash;
}

void DeviceIdentity::copyHwidToClipboard() {
    QClipboard *clipboard = QGuiApplication::clipboard();
    if (clipboard) {
        clipboard->setText(m_hwidHash);
    }
}
