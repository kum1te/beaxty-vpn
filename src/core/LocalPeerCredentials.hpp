// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

#pragma once

#include <QtGlobal>

#if defined(Q_OS_LINUX)
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace LocalPeerCredentials {

inline bool isSameUserPeer(qintptr socketDescriptor) {
    if (socketDescriptor < 0) return false;

    struct PeerCredentials {
        pid_t pid;
        uid_t uid;
        gid_t gid;
    } credentials{};
    socklen_t length = sizeof(credentials);
    const int descriptor = static_cast<int>(socketDescriptor);
    return ::getsockopt(descriptor, SOL_SOCKET, SO_PEERCRED, &credentials, &length) == 0 &&
           length == sizeof(credentials) && credentials.uid == ::getuid();
}

// The Linux network core may carry CAP_NET_ADMIN. Restrict its IPC connection
// to the exact child started by this GUI, not merely another process owned by
// the same user. SO_PEERCRED is supplied by the kernel for AF_UNIX sockets.
inline bool isExpectedCorePeer(qintptr socketDescriptor, qint64 expectedPid) {
    if (socketDescriptor < 0 || expectedPid <= 0) return false;

    struct PeerCredentials {
        pid_t pid;
        uid_t uid;
        gid_t gid;
    } credentials{};
    socklen_t length = sizeof(credentials);
    const int descriptor = static_cast<int>(socketDescriptor);
    if (::getsockopt(descriptor, SOL_SOCKET, SO_PEERCRED, &credentials, &length) != 0 ||
        length != sizeof(credentials)) {
        return false;
    }

    return credentials.pid == static_cast<pid_t>(expectedPid) && credentials.uid == ::getuid();
}

} // namespace LocalPeerCredentials
#elif defined(Q_OS_MACOS) || defined(Q_OS_FREEBSD)
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace LocalPeerCredentials {
inline bool isSameUserPeer(qintptr socketDescriptor) {
    if (socketDescriptor < 0) return false;
    uid_t uid = static_cast<uid_t>(-1);
    gid_t gid = static_cast<gid_t>(-1);
    return ::getpeereid(static_cast<int>(socketDescriptor), &uid, &gid) == 0 && uid == ::getuid();
}
} // namespace LocalPeerCredentials
#endif
