// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

#ifndef MW_INTERFACE
#define MW_INTERFACE
#endif
#include "MainWindowBridge.hpp"
#include "3rdparty/throne/include/global/Configs.hpp"
#include "3rdparty/throne/include/global/Utils.hpp"

#include <QDebug>
#ifdef Q_OS_LINUX
#include <include/sys/linux/LinuxCap.h>
#endif

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    mainwindow = this;
}

MainWindow::~MainWindow() {
    if (mainwindow == this) {
        mainwindow = nullptr;
    }
}

qint64 MainWindow::GetCorePid() {
    return 0;
}

QString MainWindow::GetRunningConfigName() {
    if (BridgeCallbacks::onGetRunningConfigName) {
        return BridgeCallbacks::onGetRunningConfigName();
    }
    return QString();
}

QString MainWindow::liveVpnConnectOkText() {
    return QStringLiteral("Connected");
}

QString MainWindow::liveVpnStateText(bool *connected) {
    if (connected) {
        *connected = false;
    }
    return QStringLiteral("DISCONNECTED");
}

void MainWindow::prepare_exit() {
    if (BridgeCallbacks::onCleanup) {
        BridgeCallbacks::onCleanup();
    }
}

void MainWindow::refresh_proxy_list(const QList<int> &ids, bool mayNeedReset, RefreshAnchor anchor) {
    Q_UNUSED(ids)
    Q_UNUSED(mayNeedReset)
    Q_UNUSED(anchor)
    if (BridgeCallbacks::onRefreshProxyList) {
        BridgeCallbacks::onRefreshProxyList();
    }
}

void MainWindow::show_group(int gid) {
    Q_UNUSED(gid)
}

void MainWindow::show_group_tab_menu(const QPoint &tabBarPos) {
    Q_UNUSED(tabBarPos)
}

void MainWindow::refresh_groups() {
    if (BridgeCallbacks::onRefreshProxyList) {
        BridgeCallbacks::onRefreshProxyList();
    }
}

void MainWindow::refresh_status(const QString &traffic_update) {
    Q_UNUSED(traffic_update)
}

void MainWindow::update_traffic_graph(int proxyDl, int proxyUp, int directDl, int directUp) {
    if (BridgeCallbacks::onUpdateTraffic) {
        BridgeCallbacks::onUpdateTraffic(proxyDl, proxyUp, directDl, directUp);
    }
}

void MainWindow::profile_start(int _id) {
    if (BridgeCallbacks::onProfileStart) {
        BridgeCallbacks::onProfileStart(_id);
    }
}

void MainWindow::profile_stop(bool crash, bool block, bool manual) {
    Q_UNUSED(crash)
    Q_UNUSED(block)
    if (BridgeCallbacks::onProfileStop) {
        BridgeCallbacks::onProfileStop(manual);
    }
}

int MainWindow::get_profile_to_start() {
    if (BridgeCallbacks::onGetProfileToStart) {
        return BridgeCallbacks::onGetProfileToStart();
    }
    return -1;
}

void MainWindow::set_spmode_system_proxy(bool enable, bool save) {
    Q_UNUSED(enable)
    Q_UNUSED(save)
}

void MainWindow::toggle_system_proxy() {}

void MainWindow::set_spmode_vpn(bool enable, bool save) {
    Q_UNUSED(save)
    if (BridgeCallbacks::onSetTunMode) {
        BridgeCallbacks::onSetTunMode(enable);
    }
}

bool MainWindow::get_elevated_permissions(ExitReason reason) {
    Q_UNUSED(reason)
    if (Configs::dataManager && Configs::dataManager->settingsRepo && Configs::dataManager->settingsRepo->disable_privilege_req) {
        return true;
    }
    if (Configs::IsAdmin()) return true;

    QString corePath = Configs::FindCoreRealPath();
    if (Configs::isSetuidSet(corePath.toStdString())) {
        return true;
    }

#ifdef Q_OS_LINUX
    if (!Linux_HavePkexec()) {
        qWarning() << "[MainWindowBridge] pkexec is not available on this system.";
        if (BridgeCallbacks::onShowToast) {
            BridgeCallbacks::onShowToast(QStringLiteral("Permission Error"),
                                         QStringLiteral("Please install 'pkexec' to elevate TUN permissions"),
                                         true);
        }
        return false;
    }

    auto ret = Linux_Run_Command(QStringLiteral("chown"), {QStringLiteral("root:root"), corePath});
    if (ret != 0) {
        qWarning() << "[MainWindowBridge] Failed to run pkexec chown:" << ret;
        return false;
    }
    ret = Linux_Run_Command(QStringLiteral("chmod"), {QStringLiteral("4755"), corePath});
    if (ret != 0) {
        qWarning() << "[MainWindowBridge] Failed to run pkexec chmod:" << ret;
        return false;
    }
    qDebug() << "[MainWindowBridge] Successfully elevated core permissions (SUID root):" << corePath;
    StopVPNProcess();
    return true;
#else
    return true;
#endif
}

void MainWindow::start_select_mode(QObject *context, const std::function<void(int)> &callback) {
    Q_UNUSED(context)
    Q_UNUSED(callback)
}

void MainWindow::RegisterHotkey(bool unregister) {
    Q_UNUSED(unregister)
}

bool MainWindow::StopVPNProcess() {
    if (BridgeCallbacks::onProfileStop) {
        BridgeCallbacks::onProfileStop(true);
    }
    return true;
}

void MainWindow::RestartCore() {
    if (BridgeCallbacks::onProfileStop) {
        BridgeCallbacks::onProfileStop(false);
    }
    if (BridgeCallbacks::onProfileStart) {
        BridgeCallbacks::onProfileStart(-1);
    }
}

void MainWindow::UpdateConnectionList(const QList<Stats::ConnectionMetadata>& connections) {
    Q_UNUSED(connections)
}

void MainWindow::UpdateDataView(bool force) {
    Q_UNUSED(force)
}

void MainWindow::refresh_auto_selector_view() {}

void MainWindow::setDownloadReport(const DownloadProgressReport& report, bool show) {
    Q_UNUSED(report)
    Q_UNUSED(show)
}

void MainWindow::on_commitDataRequest() {}

void MainWindow::on_menu_exit_triggered() {
    prepare_exit();
}

void UI_InitMainWindow() {
    if (!mainwindow) {
        mainwindow = new MainWindow();
    }
}

#ifdef Q_OS_LINUX
OrgFreedesktopPortalRequestInterface::OrgFreedesktopPortalRequestInterface(
    const QString& service,
    const QString& path,
    const QDBusConnection& connection,
    QObject* parent)
    : QDBusAbstractInterface(service,
                             path,
                             "org.freedesktop.portal.Request",
                             connection,
                             parent)
{}

OrgFreedesktopPortalRequestInterface::~OrgFreedesktopPortalRequestInterface() {}
#endif
