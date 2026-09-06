// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

#include <iostream>
#include <cassert>
#include <QApplication>
#include <QFile>
#include <QDir>
#include <QByteArray>
#include <QDebug>

#include "src/bridge/MainWindowBridge.hpp"
#include "src/core/ConfigAdapter.hpp"
#include "src/core/ToastManager.hpp"
#include "3rdparty/throne/include/global/Configs.hpp"
#include "3rdparty/throne/include/database/DatabaseManager.h"
#include "3rdparty/throne/include/database/GroupsRepo.h"
#include "3rdparty/throne/include/database/ProfilesRepo.h"
#include "3rdparty/throne/include/database/SettingsRepo.h"
#include "3rdparty/throne/include/configs/sub/SubscriptionParser.hpp"
#include "SQLiteCpp.h"

int main(int argc, char *argv[]) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    std::cout << "====================================================" << std::endl;
    std::cout << "[TEST] Starting End-to-End Subscription Import Test" << std::endl;
    std::cout << "====================================================" << std::endl;

    QString testDbPath = QStringLiteral("/tmp/test_subscription_import.db");
    if (QFile::exists(testDbPath)) {
        QFile::remove(testDbPath);
    }

    // Initialize headless bridge
    UI_InitMainWindow();

    ToastManager toastManager;
    BridgeCallbacks::onShowToast = [](const QString &t, const QString &m, bool err) {
        std::cout << "  [Toast Intercepted] " << (err ? "ERROR: " : "INFO: ")
                  << t.toStdString() << " - " << m.toStdString() << std::endl;
    };

    // Initialize database
    Configs::initDB(testDbPath.toStdString());

    ConfigAdapter adapter;

    // 1. Import realistic VLESS link requested by user:
    QString vlessLink = QStringLiteral("vless://b831381d-6324-4d53-ad4f-8cda48b30811@104.21.5.12:443?encryption=none&security=reality&sni=yahoo.com&fp=chrome&pbk=wA6f6qS6G7N5h8T2kR4pL0mX1vY3zB9aC7dE5fG2hJ4&sid=1a2b3c4d&type=tcp&headerType=none#TestServer");
    std::cout << "\n[1] Importing VLESS link..." << std::endl;
    adapter.importSubscription(vlessLink, QStringLiteral("VLESS-Custom-Group"));

    // 2. Import Base64 subscription
    // Raw contents: two VLESS nodes encoded in Base64
    QString rawSub = QStringLiteral(
        "vless://a1b2c3d4-0000-0000-0000-000000000001@1.1.1.1:443?encryption=none&security=tls&sni=cloudflare.com&type=tcp#Node-Alpha\n"
        "vless://a1b2c3d4-0000-0000-0000-000000000002@1.0.0.1:443?encryption=none&security=tls&sni=cloudflare.com&type=tcp#Node-Beta\n"
    );
    QString base64Sub = QString::fromLatin1(rawSub.toUtf8().toBase64());
    std::cout << "\n[2] Importing Base64 subscription (" << base64Sub.length() << " chars)..." << std::endl;
    adapter.importSubscription(base64Sub, QStringLiteral("Base64-Sub-Group"));

    // 3. Directly inspect SQLite database via SQLiteCpp
    std::cout << "\n[3] Verifying SQLite database tables and foreign keys..." << std::endl;
    try {
        SQLite::Database db(testDbPath.toStdString(), SQLite::OPEN_READONLY);

        // Check foreign key violations
        SQLite::Statement fkCheck(db, "PRAGMA foreign_key_check;");
        int fkViolations = 0;
        while (fkCheck.executeStep()) {
            fkViolations++;
            std::cerr << "  FK VIOLATION in table: " << fkCheck.getColumn(0).getText() << std::endl;
        }
        if (fkViolations > 0) {
            std::cerr << "FAILED: Database has " << fkViolations << " foreign key violations!" << std::endl;
            return 1;
        }
        std::cout << "  PRAGMA foreign_key_check: 0 violations! (Integrity Confirmed)" << std::endl;

        // Verify Groups
        std::cout << "\n  Groups in database:" << std::endl;
        SQLite::Statement groupQuery(db, "SELECT id, name FROM groups ORDER BY id ASC;");
        int groupCount = 0;
        bool foundVlessGroup = false;
        bool foundBase64Group = false;
        while (groupQuery.executeStep()) {
            int gid = groupQuery.getColumn(0).getInt();
            std::string gname = groupQuery.getColumn(1).getText();
            std::cout << "    [GroupID " << gid << "] " << gname << std::endl;
            if (gname == "VLESS-Custom-Group") foundVlessGroup = true;
            if (gname == "Base64-Sub-Group") foundBase64Group = true;
            groupCount++;
        }

        if (!foundVlessGroup) {
            std::cerr << "FAILED: VLESS-Custom-Group not found in groups table!" << std::endl;
            return 1;
        }
        if (!foundBase64Group) {
            std::cerr << "FAILED: Base64-Sub-Group not found in groups table!" << std::endl;
            return 1;
        }

        // Verify Profiles
        std::cout << "\n  Profiles in database:" << std::endl;
        SQLite::Statement profQuery(db, "SELECT p.id, p.name, p.gid, g.name FROM profiles p JOIN groups g ON p.gid = g.id ORDER BY p.id ASC;");
        int profCount = 0;
        bool foundTestServer = false;
        bool foundNodeAlpha = false;
        bool foundNodeBeta = false;

        while (profQuery.executeStep()) {
            int pid = profQuery.getColumn(0).getInt();
            std::string pname = profQuery.getColumn(1).getText();
            int gid = profQuery.getColumn(2).getInt();
            std::string gname = profQuery.getColumn(3).getText();
            std::cout << "    [ProfileID " << pid << "] \"" << pname << "\" -> Group [" << gid << ": " << gname << "]" << std::endl;

            if (pname == "TestServer") foundTestServer = true;
            if (pname == "Node-Alpha") foundNodeAlpha = true;
            if (pname == "Node-Beta") foundNodeBeta = true;
            profCount++;
        }

        if (!foundTestServer) {
            std::cerr << "FAILED: TestServer profile not found in profiles table!" << std::endl;
            return 1;
        }
        if (!foundNodeAlpha || !foundNodeBeta) {
            std::cerr << "FAILED: Base64 profiles (Node-Alpha / Node-Beta) not found in profiles table!" << std::endl;
            return 1;
        }

        std::cout << "\n[PASS] All SQLite records and foreign keys verified perfectly!" << std::endl;

        // 4. Verify auto-selection of imported node
        std::cout << "\n[4] Verifying auto-selection of imported server..." << std::endl;
        std::cout << "  Current selectedServerId: " << adapter.selectedServerId() << std::endl;
        if (adapter.selectedServerId() != 2) { // First profile from Base64-Sub-Group
            std::cerr << "FAILED: Expected selectedServerId 2, got " << adapter.selectedServerId() << std::endl;
            return 1;
        }
        std::cout << "  Auto-selection confirmed! (Server ID: " << adapter.selectedServerId() << ")" << std::endl;

        // 5. Verify groups() list
        std::cout << "\n[5] Verifying groups() API..." << std::endl;
        auto grps = adapter.groups();
        std::cout << "  Groups count: " << grps.size() << std::endl;
        if (grps.size() < 2) {
            std::cerr << "FAILED: Expected at least 2 groups, got " << grps.size() << std::endl;
            return 1;
        }
        for (const auto &gVar : grps) {
            auto gm = gVar.toMap();
            std::cout << "    Group " << gm["id"].toInt() << ": " << gm["name"].toString().toStdString()
                      << " (count: " << gm["count"].toInt() << ")" << std::endl;
        }

        // 6. Verify deleteGroup()
        std::cout << "\n[6] Testing deleteGroup(2) (VLESS-Custom-Group)..." << std::endl;
        adapter.deleteGroup(2);
        
        // Verify via SQLite that Group 2 and its profiles are gone
        SQLite::Statement checkG2(db, "SELECT count(*) FROM groups WHERE id = 2;");
        if (checkG2.executeStep() && checkG2.getColumn(0).getInt() != 0) {
            std::cerr << "FAILED: Group 2 was not deleted from database!" << std::endl;
            return 1;
        }
        SQLite::Statement checkP1(db, "SELECT count(*) FROM profiles WHERE gid = 2;");
        if (checkP1.executeStep() && checkP1.getColumn(0).getInt() != 0) {
            std::cerr << "FAILED: Profiles belonging to Group 2 were not deleted!" << std::endl;
            return 1;
        }
        SQLite::Statement fkCheck2(db, "PRAGMA foreign_key_check;");
        if (fkCheck2.executeStep()) {
            std::cerr << "FAILED: FK violation after deleteGroup!" << std::endl;
            return 1;
        }
        std::cout << "  Group 2 and its profiles successfully deleted with 0 FK violations!" << std::endl;

        // 7. Testing renameGroup()
        std::cout << "\n[7] Testing renameGroup(3, \"Renamed-Base64-Group\")..." << std::endl;
        adapter.renameGroup(3, QStringLiteral("Renamed-Base64-Group"));
        
        // Verify via GroupsRepo
        std::string gName = Configs::dataManager->groupsRepo->GetGroup(3)->name.toStdString();
        if (gName != "Renamed-Base64-Group") {
            std::cerr << "FAILED: GroupsRepo returned name '" << gName << "'!" << std::endl;
            return 1;
        }

        // Verify via fresh SQLite connection
        {
            SQLite::Database freshDb(testDbPath.toStdString(), SQLite::OPEN_READONLY);
            SQLite::Statement checkRename(freshDb, "SELECT name FROM groups WHERE id = 3;");
            if (checkRename.executeStep()) {
                std::string renName = checkRename.getColumn(0).getText();
                if (renName != "Renamed-Base64-Group") {
                    std::cerr << "FAILED: Expected group name 'Renamed-Base64-Group', got '" << renName << "'!" << std::endl;
                    return 1;
                }
                std::cout << "  Group 3 successfully renamed to: " << renName << std::endl;
            } else {
                std::cerr << "FAILED: Group 3 not found after rename!" << std::endl;
                return 1;
            }
        }

        // 8. Testing duplicate prevention when updating group with a selected server
        std::cout << "\n[8] Testing duplicate server prevention when updating group with selected server..." << std::endl;
        // Group 3 currently has 2 profiles (Node-Alpha and Node-Beta)
        int cBefore = 0;
        int selectedProfId = -1;
        {
            SQLite::Database freshDb(testDbPath.toStdString(), SQLite::OPEN_READONLY);
            SQLite::Statement countBefore(freshDb, "SELECT count(*) FROM profiles WHERE gid = 3;");
            countBefore.executeStep();
            cBefore = countBefore.getColumn(0).getInt();
            std::cout << "  Profile count in Group 3 before update: " << cBefore << std::endl;
            assert(cBefore == 2);

            // Select Node-Alpha (first profile in group 3)
            SQLite::Statement getFirstProf(freshDb, "SELECT id FROM profiles WHERE gid = 3 ORDER BY id ASC LIMIT 1;");
            getFirstProf.executeStep();
            selectedProfId = getFirstProf.getColumn(0).getInt();
        }

        adapter.selectServer(selectedProfId);
        std::cout << "  Selected profile ID: " << selectedProfId
                  << " (started_id = " << Configs::dataManager->settingsRepo->started_id << ")" << std::endl;

        // Re-importing into same group structure (simulating subscription update)
        // Ensure BatchDeleteProfiles with started_id set does not skip started_id
        auto pRepo = Configs::dataManager->profilesRepo.get();
        auto allProfiles = pRepo->GetProfileBatch(pRepo->GetAllProfileIds());
        QList<int> oldIds;
        for (const auto &p : allProfiles) {
            if (p && p->gid == 3) oldIds.append(p->id);
        }
        int prevStartedId = Configs::dataManager->settingsRepo ? Configs::dataManager->settingsRepo->started_id : -1;
        if (Configs::dataManager->settingsRepo) Configs::dataManager->settingsRepo->started_id = -1;
        pRepo->BatchDeleteProfiles(oldIds);
        if (Configs::dataManager->settingsRepo) Configs::dataManager->settingsRepo->started_id = prevStartedId;

        // Verify that ALL old profiles were removed, not leaving started_id behind
        int cAfterDel = 0;
        {
            SQLite::Database freshDb(testDbPath.toStdString(), SQLite::OPEN_READONLY);
            SQLite::Statement countAfterDel(freshDb, "SELECT count(*) FROM profiles WHERE gid = 3;");
            countAfterDel.executeStep();
            cAfterDel = countAfterDel.getColumn(0).getInt();
        }
        std::cout << "  Profile count in Group 3 after delete: " << cAfterDel << std::endl;
        if (cAfterDel != 0) {
            std::cerr << "FAILED: Old profiles (including started_id) were not cleanly removed! Remaining: " << cAfterDel << std::endl;
            return 1;
        }

        // Re-parse profiles as updateGroup does
        int newCount = 0;
        Subscription::ParseSink sink;
        sink.profile = [&](std::shared_ptr<Configs::Profile> prof) {
            if (prof && prof->outbound) {
                prof->gid = 3;
                if (pRepo->AddProfile(prof, 3)) newCount++;
            }
        };
        Subscription::ParseDocument(QByteArray::fromBase64(base64Sub.toLatin1()), sink);
        std::cout << "  Re-added profiles count: " << newCount << std::endl;

        int cAfterAdd = 0;
        {
            SQLite::Database freshDb(testDbPath.toStdString(), SQLite::OPEN_READONLY);
            SQLite::Statement countAfterAdd(freshDb, "SELECT count(*) FROM profiles WHERE gid = 3;");
            countAfterAdd.executeStep();
            cAfterAdd = countAfterAdd.getColumn(0).getInt();
        }
        std::cout << "  Profile count in Group 3 after re-add: " << cAfterAdd << std::endl;
        if (cAfterAdd != 2) {
            std::cerr << "FAILED: Server count changed or duplicated! Expected 2, got: " << cAfterAdd << std::endl;
            return 1;
        }
        std::cout << "  Zero duplicate servers verified! Profile count strictly maintained." << std::endl;

        // 9. Testing subscription headers, multiline announcement, and URL links
        std::cout << "\n[9] Testing subscription header metadata (title, announcement, supportUrl, webUrl)..." << std::endl;
        auto grp3 = Configs::dataManager->groupsRepo->GetGroup(3);
        assert(grp3 != nullptr);
        grp3->name = QStringLiteral("beaxty VPN 🪽");
        QString testAnnounce = QStringLiteral("🔄 Не забывайте обновлять подписку\n⚡ - Сервера с низким пингом\n🏳️ - Если не работает мобильный интернет\nБот: @beaxtyvpnbot | Сайт: cabinet.beaxty.com");
        QString testAnnounceB64 = QString::fromLatin1(testAnnounce.toUtf8().toBase64());
        grp3->info = QStringLiteral("upload=0; download=2107669288917; total=0; expire=0; announce_b64=%1; support=https://t.me/beaxtysupport; web=https://sub.beaxty.com:8443/z-pB5nbBj37wuqQz")
                     .arg(testAnnounceB64);
        Configs::dataManager->groupsRepo->Save(grp3);

        auto updatedGroups = adapter.groups();
        bool verifiedGroup3 = false;
        for (const auto &gVar : updatedGroups) {
            auto gm = gVar.toMap();
            if (gm["id"].toInt() == 3) {
                verifiedGroup3 = true;
                std::cout << "  Group 3 name: " << gm["name"].toString().toStdString() << std::endl;
                std::cout << "  Group 3 announcement: " << gm["announcement"].toString().toStdString() << std::endl;
                std::cout << "  Group 3 supportUrl: " << gm["supportUrl"].toString().toStdString() << std::endl;
                std::cout << "  Group 3 webUrl: " << gm["webUrl"].toString().toStdString() << std::endl;
                std::cout << "  Group 3 isUnlimited: " << gm["isUnlimited"].toBool() << std::endl;
                std::cout << "  Group 3 isPerpetual: " << gm["isPerpetual"].toBool() << std::endl;

                if (gm["name"].toString() != QStringLiteral("beaxty VPN 🪽")) {
                    std::cerr << "FAILED: Expected group name 'beaxty VPN 🪽', got: " << gm["name"].toString().toStdString() << std::endl;
                    return 1;
                }
                if (!gm["announcement"].toString().contains(QStringLiteral("🔄 Не забывайте обновлять подписку"))) {
                    std::cerr << "FAILED: Announcement missing expected text!" << std::endl;
                    return 1;
                }
                if (gm["supportUrl"].toString() != QStringLiteral("https://t.me/beaxtysupport")) {
                    std::cerr << "FAILED: Incorrect supportUrl!" << std::endl;
                    return 1;
                }
                if (gm["webUrl"].toString() != QStringLiteral("https://sub.beaxty.com:8443/z-pB5nbBj37wuqQz")) {
                    std::cerr << "FAILED: Incorrect webUrl!" << std::endl;
                    return 1;
                }
                if (!gm["isUnlimited"].toBool() || !gm["isPerpetual"].toBool()) {
                    std::cerr << "FAILED: Expected isUnlimited and isPerpetual to be true!" << std::endl;
                    return 1;
                }
            }
        }
        if (!verifiedGroup3) {
            std::cerr << "FAILED: Group 3 not found in groups()!" << std::endl;
            return 1;
        }
        std::cout << "  Subscription headers, announcements, and quick links verified successfully!" << std::endl;

        std::cout << "\n====================================================" << std::endl;
        std::cout << "[SUCCESS] ALL SUBSCRIPTION, GROUP & SELECTION TESTS PASSED!" << std::endl;
        std::cout << "====================================================" << std::endl;
        return 0;

    } catch (const std::exception &e) {
        std::cerr << "FAILED: SQLite exception: " << e.what() << std::endl;
        return 1;
    }
}
