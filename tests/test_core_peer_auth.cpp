// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <iostream>

#include "src/core/LocalPeerCredentials.hpp"

#if defined(Q_OS_LINUX)
#include <sys/socket.h>
#include <unistd.h>
#endif

int main() {
#if defined(Q_OS_LINUX)
    int sockets[2] = {-1, -1};
    assert(::socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);

    const qint64 currentPid = static_cast<qint64>(::getpid());
    assert(LocalPeerCredentials::isSameUserPeer(sockets[0]));
    assert(LocalPeerCredentials::isExpectedCorePeer(sockets[0], currentPid));
    assert(!LocalPeerCredentials::isExpectedCorePeer(sockets[0], currentPid + 1));
    assert(!LocalPeerCredentials::isExpectedCorePeer(-1, currentPid));

    ::close(sockets[0]);
    ::close(sockets[1]);
    std::cout << "Linux local peer authentication test passed.\n";
#else
    std::cout << "Linux local peer authentication test skipped on this platform.\n";
#endif
    return 0;
}
