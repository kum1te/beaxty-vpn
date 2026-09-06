// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

#pragma once

#ifndef MW_INTERFACE
#define MW_INTERFACE
#endif
#include "3rdparty/throne/include/ui/mainwindow.h"

#include <functional>

namespace BridgeCallbacks {
    inline std::function<void(int)> onProfileStart;
    inline std::function<void(bool)> onProfileStop;
    inline std::function<int()> onGetProfileToStart;
    inline std::function<void(int, int, int, int)> onUpdateTraffic;
    inline std::function<void()> onRefreshProxyList;
    inline std::function<QString()> onGetRunningConfigName;
    inline std::function<void(bool)> onSetTunMode;
    inline std::function<void()> onCleanup;
    inline std::function<void(const QString &, const QString &, bool)> onShowToast;
}
