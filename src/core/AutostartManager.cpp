// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

#include "src/core/AutostartManager.hpp"

#include <QCoreApplication>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QProcessEnvironment>
#include <QDebug>

#if defined(Q_OS_WIN)
#include <QSettings>
#endif

AutostartManager::AutostartManager(QObject *parent)
    : QObject(parent)
{
}

#if defined(Q_OS_LINUX)
QString AutostartManager::desktopFilePath() const
{
    QString config = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    return config + QStringLiteral("/autostart/beaxty-vpn.desktop");
}
#endif

bool AutostartManager::autostartEnabled() const
{
#if defined(Q_OS_LINUX)
    return QFile::exists(desktopFilePath());
#elif defined(Q_OS_WIN)
    QSettings settings(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"), QSettings::NativeFormat);
    return settings.contains(QStringLiteral("BeaxtyVPN"));
#else
    return false;
#endif
}

void AutostartManager::setAutostartEnabled(bool enabled)
{
    if (autostartEnabled() == enabled) {
        return;
    }

#if defined(Q_OS_LINUX)
    QString filePath = desktopFilePath();
    if (enabled) {
        QFileInfo info(filePath);
        QDir dir = info.dir();
        if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
            qWarning() << "[AutostartManager] Failed to create directory:" << dir.absolutePath();
            return;
        }

        QString execPath;
        if (QProcessEnvironment::systemEnvironment().contains(QStringLiteral("APPIMAGE"))) {
            execPath = QProcessEnvironment::systemEnvironment().value(QStringLiteral("APPIMAGE"));
        } else {
            execPath = QCoreApplication::applicationFilePath();
        }

        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            qWarning() << "[AutostartManager] Failed to write autostart file:" << filePath;
            return;
        }

        QTextStream out(&file);
        out << QStringLiteral("[Desktop Entry]\n")
            << QStringLiteral("Type=Application\n")
            << QStringLiteral("Name=Beaxty VPN\n")
            << QStringLiteral("Comment=Beaxty VPN Client\n")
            << QStringLiteral("Exec=\"%1\" -tray\n").arg(execPath)
            << QStringLiteral("Icon=beaxty-vpn\n")
            << QStringLiteral("Terminal=false\n")
            << QStringLiteral("Categories=Network;VPN;\n")
            << QStringLiteral("StartupNotify=false\n")
            << QStringLiteral("X-GNOME-Autostart-enabled=true\n");
        out.flush();
        file.close();
        qInfo() << "[AutostartManager] Autostart enabled at:" << filePath;
    } else {
        if (QFile::exists(filePath)) {
            QFile::remove(filePath);
            qInfo() << "[AutostartManager] Autostart disabled (removed):" << filePath;
        }
    }
#elif defined(Q_OS_WIN)
    QSettings settings(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"), QSettings::NativeFormat);
    if (enabled) {
        QString appPath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
        settings.setValue(QStringLiteral("BeaxtyVPN"), QStringLiteral("\"%1\" -tray").arg(appPath));
    } else {
        settings.remove(QStringLiteral("BeaxtyVPN"));
    }
#endif

    emit autostartEnabledChanged();
}
