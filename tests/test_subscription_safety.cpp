#include <QApplication>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <QThread>
#include <QTimer>
#include <iostream>
#include "src/core/ConfigAdapter.hpp"
#include "src/core/AppPrefs.hpp"
#include "src/bridge/MainWindowBridge.hpp"
#include "3rdparty/throne/include/global/Configs.hpp"
#include "3rdparty/throne/include/global/HTTPRequestHelper.hpp"
#include "3rdparty/throne/include/database/DatabaseManager.h"
#include "3rdparty/throne/include/database/ProfilesRepo.h"
#include "3rdparty/throne/include/database/GroupsRepo.h"

static void require(bool condition, const char *message) {
    if (!condition) throw std::runtime_error(message);
}
static bool waitUntil(const std::function<bool()> &done) {
    QElapsedTimer timer;
    timer.start();
    while (!done() && timer.elapsed() < 5000) {
        QCoreApplication::processEvents();
        QThread::msleep(1);
    }
    return done();
}

int main(int argc, char **argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    QTemporaryDir dir;
    UI_InitMainWindow();
    // Match application startup: facade construction precedes database initialization.
    ConfigAdapter adapter;
    Configs::initDB(dir.filePath("test.db").toStdString());
    AppPrefs::setInt("auto_update_subs_mode", 0);
    AppPrefs::setInt("server_sort_mode", 2);
    adapter.reloadServers();
    const QString node = "vless://a1b2c3d4-0000-0000-0000-000000000001@1.1.1.1:443?security=tls&sni=example.com&type=tcp#Alpha";
    try {
        require(adapter.autoUpdateSubsMode() == 0 && adapter.serverSortMode() == 2,
                "preferences not restored after database initialization");
        const auto initialGroups = Configs::dataManager->groupsRepo->GetAllGroupIds();
        adapter.importSubscription("this is not a subscription");
        require(Configs::dataManager->groupsRepo->GetAllGroupIds() == initialGroups, "invalid import created an empty group");
        adapter.importSubscription(node, "Local");
        const int localId = adapter.selectedServerId();
        adapter.selectServer(999999);
        require(adapter.selectedServerId() == localId, "accepted nonexistent server selection");
        auto saved = Configs::dataManager->profilesRepo->GetProfile(localId);
        saved->latency = 42;
        Configs::dataManager->profilesRepo->Save(saved);
        adapter.reloadServers();
        require(adapter.selectedServerPing() == 42, "lost persisted latency");
        QObject::connect(&adapter, &ConfigAdapter::selectedServerPingChanged, &app, [&](int ping) {
            require(adapter.selectedServerPing() == ping, "ping notification exposed stale property value");
        });
        int modelUpdates = 0;
        QObject::connect(&adapter, &ConfigAdapter::serversChanged, &app, [&] { ++modelUpdates; });
        for (int ping = 1; ping <= 20; ++ping) adapter.updateServerPing(localId, ping);
        require(modelUpdates == 0, "ping results rebuilt the model synchronously");
        require(waitUntil([&] { return modelUpdates > 0; }) && modelUpdates == 1,
                "ping results were not coalesced");

        QTcpServer server;
        require(server.listen(QHostAddress::LocalHost), "cannot listen on loopback for network regression tests");
        QByteArray body = node.toUtf8();
        int requests = 0;
        QObject::connect(&server, &QTcpServer::newConnection, &app, [&] {
            while (auto socket = server.nextPendingConnection()) {
                QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
                QObject::connect(socket, &QTcpSocket::readyRead, socket, [&, socket] {
                    const auto request = socket->readAll();
                    if (socket->property("answered").toBool()) return;
                    socket->setProperty("answered", true);
                    ++requests;
                    const auto response = body;
                    QTimer::singleShot(100, socket, [socket, response] {
                        socket->write("HTTP/1.1 200 OK\r\nContent-Length: " + QByteArray::number(response.size()) +
                                      "\r\nConnection: close\r\n\r\n" + response);
                        socket->disconnectFromHost();
                    });
                });
            }
        });
        const QString url = QString("http://127.0.0.1:%1/subscription").arg(server.serverPort());
        bool heartbeat = false;
        QTimer::singleShot(10, &app, [&] { heartbeat = true; });
        adapter.importSubscription(url, "Remote");
        require(adapter.importing(), "URL import did not return asynchronously");
        adapter.importSubscription(url, "Duplicate");
        require(waitUntil([&] { return !adapter.importing(); }), "URL import timed out");
        require(heartbeat && requests == 1, "import blocked events or allowed duplicate requests");
        const int remoteId = adapter.selectedServerId();
        require(remoteId != localId, "remote import failed");
        const int gid = Configs::dataManager->profilesRepo->GetProfile(remoteId)->gid;
        auto group = Configs::dataManager->groupsRepo->GetGroup(gid);
        const auto idsBefore = Configs::dataManager->profilesRepo->GetAllProfileIds();
        const auto updatedBefore = group->sub_last_update;
        body = "<html>upstream failure</html>";
        adapter.updateGroup(gid, true);
        adapter.updateGroup(gid, true);
        require(waitUntil([&] { return !adapter.refreshing(); }), "refresh timed out");
        require(requests == 2, "duplicate refresh was not coalesced");
        require(Configs::dataManager->profilesRepo->GetAllProfileIds() == idsBefore, "invalid refresh deleted servers");
        require(group->sub_last_update == updatedBefore, "invalid refresh advanced timestamp");
        body = node.toUtf8();
        adapter.selectServer(localId);
        adapter.updateGroup(gid, true);
        require(waitUntil([&] { return !adapter.refreshing(); }), "valid refresh timed out");
        require(adapter.selectedServerId() == localId, "refresh stole selection from another group");
        require(Configs::dataManager->profilesRepo->GetAllProfileIds() == idsBefore, "unchanged refresh replaced profile IDs");
        body = node.toUtf8() + '\n' + node.toUtf8().replace("Alpha", "Beta").replace("1.1.1.1", "1.0.0.1");
        adapter.updateGroup(gid, true);
        require(waitUntil([&] { return !adapter.refreshing(); }), "expanded refresh timed out");
        require(adapter.serversForGroup(gid).size() == 2, "expanded refresh has incorrect count");
        // Inject a real SQLite write failure: an insertion failure must retain old data.
        Configs::dataManager->getDatabase().execThrow("CREATE TRIGGER fail_insert BEFORE INSERT ON profiles BEGIN SELECT RAISE(ABORT, 'test failure'); END");
        body = node.toUtf8().replace("Alpha", "Gamma");
        adapter.updateGroup(gid, true);
        require(waitUntil([&] { return !adapter.refreshing(); }), "failed-write refresh timed out");
        require(adapter.serversForGroup(gid).size() == 2, "failed database write removed working servers");
        Configs::dataManager->getDatabase().execThrow("DROP TRIGGER fail_insert");
        body = node.toUtf8();
        adapter.updateGroup(gid, true);
        require(waitUntil([&] { return !adapter.refreshing(); }), "shrinking refresh timed out");
        require(adapter.serversForGroup(gid).size() == 1, "stale node was not removed");
        adapter.updateGroup(gid, true);
        adapter.deleteGroup(gid);
        require(waitUntil([&] { return !adapter.refreshing(); }), "deleted-group refresh timed out");
        require(!Configs::dataManager->groupsRepo->GetGroup(gid), "in-flight refresh recreated deleted group");
        bool callback = false;
        body = QByteArray(2048, 'x');
        Configs_network::NetworkRequestHelper::HttpGetAsync(&app, url, false, false, 1024,
            [&](Configs_network::HTTPResponse response) {
                callback = true;
                require(!response.error.isEmpty() && response.data.isEmpty(), "oversized response was accepted");
            });
        require(waitUntil([&] { return callback; }), "bounded download timed out");
        auto context = new QObject;
        callback = false;
        Configs_network::NetworkRequestHelper::HttpGetAsync(context, url, false, false, 1024,
            [&](Configs_network::HTTPResponse) { callback = true; });
        delete context;
        QCoreApplication::processEvents();
        require(!callback, "callback ran after owner destruction");
        std::cout << "PASS: subscription safety, asynchronous UI, selection, persistence, and bounded downloads\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
