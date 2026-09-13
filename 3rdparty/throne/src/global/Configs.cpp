#include "include/global/Configs.hpp"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequence>
#include <QNetworkAccessManager>
#include <QStandardPaths>
#include <utility>
#include <include/api/RPC.h>

#include "include/database/GroupsRepo.h"
#include "include/database/RoutesRepo.h"


#ifdef Q_OS_WIN
#include "include/sys/windows/guihelper.h"
#else
#ifdef Q_OS_LINUX
#include <include/sys/linux/LinuxCap.h>
#endif
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif

namespace Configs {
    void initDB(const std::string& dbPath) {
        dataManager = new DatabaseManager(dbPath);
#ifndef _WIN32
        ::chmod(dbPath.c_str(), S_IRUSR | S_IWUSR);
        std::string walPath = dbPath + "-wal";
        if (access(walPath.c_str(), F_OK) == 0) ::chmod(walPath.c_str(), S_IRUSR | S_IWUSR);
        std::string shmPath = dbPath + "-shm";
        if (access(shmPath.c_str(), F_OK) == 0) ::chmod(shmPath.c_str(), S_IRUSR | S_IWUSR);
#endif

        if (dataManager->groupsRepo->GetAllGroupIds().empty()) {
            auto defaultGroup = GroupsRepo::NewGroup();
            defaultGroup->name = QObject::tr("Default");
            dataManager->groupsRepo->AddGroup(defaultGroup);
            dataManager->settingsRepo->current_group = defaultGroup->id;
        } else {
            auto allGids = dataManager->groupsRepo->GetAllGroupIds();
            if (dataManager->groupsRepo->GetGroup(dataManager->settingsRepo->current_group) == nullptr && !allGids.isEmpty()) {
                dataManager->settingsRepo->current_group = allGids.first();
            }
        }
        if (dataManager->routesRepo->GetAllRouteProfileIds().empty()) {
            auto defaultRoute = RouteProfile::GetDefaultChain();
            dataManager->routesRepo->AddRouteProfile(defaultRoute);
        }
    }

    QString FindCoreRealPath() {
        QString appDir = QApplication::applicationDirPath();
        QStringList candidates = {
            appDir + "/beaxty-core",
            appDir + "/../bin/beaxty-core",
            QDir::currentPath() + "/bin/beaxty-core",
            appDir + "/ThroneCore"
        };
#ifdef Q_OS_WIN
        for (auto &cand : candidates) cand += ".exe";
#endif
        for (const auto &cand : candidates) {
            if (QFile::exists(cand)) {
                QFileInfo fi(cand);
                QString path = fi.isSymLink() ? fi.symLinkTarget() : fi.canonicalFilePath();
#ifdef Q_OS_WIN
                path.replace("/", "\\");
#endif
                return path;
            }
        }
        auto fn = appDir + "/beaxty-core";
#ifdef Q_OS_WIN
        fn += ".exe";
        fn.replace("/", "\\");
#endif
        return fn;
    }

    short isAdminCache = -1;

    bool isSetuidSet(const std::string& path) {
#if defined(Q_OS_MACOS) || defined(Q_OS_LINUX)
        struct stat fileInfo;

        if (stat(path.c_str(), &fileInfo) != 0) {
            return false;
        }

        if ((fileInfo.st_mode & S_ISUID) && (fileInfo.st_uid == 0)) {
            return true;
        } else {
            return false;
        }
#else
        return false;
#endif
    }

    // IsAdmin 主要判断：有无权限启动 Tun
    bool IsAdmin(bool forceRenew) {
        if (isAdminCache >= 0 && !forceRenew) return isAdminCache;

        bool admin = false;
#ifdef Q_OS_WIN
        admin = Windows_IsInAdmin();
        Configs::dataManager->settingsRepo->windows_set_admin = admin;
#else
        // Unknown until the core answers; caching that would pin "not elevated" for the session.
        if (API::defaultClient == nullptr) return false;
        bool ok;
        const auto isPrivileged = API::defaultClient->IsPrivileged(&ok);
        if (!ok) return false;
        admin = isPrivileged;
#endif
        isAdminCache = admin;
        return admin;
    }

    QString GetBasePath() {
        if (Configs::dataManager->settingsRepo->flag_use_appdata) return QStandardPaths::writableLocation(
              QStandardPaths::AppConfigLocation);
        return qApp->applicationDirPath();
    }
} // namespace Configs
