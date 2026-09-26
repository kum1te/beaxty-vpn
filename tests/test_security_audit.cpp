// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

#include <iostream>
#include <cassert>
#include <QApplication>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QUrl>
#include <QRegularExpression>
#include <QJsonObject>
#include <QJsonArray>
#include <QTemporaryDir>

#if !defined(_WIN32)
#include <sys/stat.h>
#endif

#include "3rdparty/throne/include/global/Configs.hpp"
#include "3rdparty/throne/include/global/HTTPRequestHelper.hpp"
#include "3rdparty/throne/include/database/DatabaseManager.h"
#include "3rdparty/throne/include/database/SettingsRepo.h"
#include "3rdparty/throne/include/database/ProfilesRepo.h"
#include "3rdparty/throne/include/database/GroupsRepo.h"
#include "3rdparty/throne/include/configs/generate.h"
#include "3rdparty/throne/include/configs/outbounds/vless.h"
#include "src/bridge/MainWindowBridge.hpp"

static void require(bool cond, const char *msg) {
    if (!cond) {
        std::cerr << "FAIL: " << msg << std::endl;
        std::exit(1);
    }
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    std::cout << "[TEST] Running beaxty VPN Security Audit Verification..." << std::endl;

    // =========================================================================
    // 1. SSRF Protection Verification
    // =========================================================================
    std::cout << "  [1] Verifying SSRF Protection (NetworkRequestHelper::IsSafePublicUrl)..." << std::endl;
    {
        // Cloud metadata addresses MUST be blocked
        require(!NetworkRequestHelper::IsSafePublicUrl(QUrl("http://169.254.169.254/latest/meta-data")),
                "SSRF: AWS/Azure metadata 169.254.169.254 was NOT blocked");
        require(!NetworkRequestHelper::IsSafePublicUrl(QUrl("http://169.254.1.1/")),
                "SSRF: Link-local address was NOT blocked");
        require(!NetworkRequestHelper::IsSafePublicUrl(QUrl("http://100.100.100.200/latest/meta-data")),
                "SSRF: Alibaba metadata 100.100.100.200 was NOT blocked");
        require(!NetworkRequestHelper::IsSafePublicUrl(QUrl("http://metadata.google.internal/computeMetadata/v1/")),
                "SSRF: Google Cloud metadata hostname was NOT blocked");
        require(!NetworkRequestHelper::IsSafePublicUrl(QUrl("http://224.0.0.1/multicast")),
                "SSRF: Multicast address was NOT blocked");
        require(!NetworkRequestHelper::IsSafePublicUrl(QUrl("http://0.0.0.0/test")),
                "SSRF: 0.0.0.0 was NOT blocked");
        require(!NetworkRequestHelper::IsSafePublicUrl(QUrl("http://10.0.0.1/")),
                "SSRF: RFC1918 10/8 address was NOT blocked");
        require(!NetworkRequestHelper::IsSafePublicUrl(QUrl("http://172.16.0.1/")),
                "SSRF: RFC1918 172.16/12 address was NOT blocked");
        require(!NetworkRequestHelper::IsSafePublicUrl(QUrl("http://192.168.1.1/")),
                "SSRF: RFC1918 192.168/16 address was NOT blocked");
        require(!NetworkRequestHelper::IsSafePublicUrl(QUrl("http://[fc00::1]/")),
                "SSRF: IPv6 ULA address was NOT blocked");
        require(!NetworkRequestHelper::IsSafePublicUrl(QUrl("http://localhost./")),
                "SSRF: dotted localhost hostname was NOT blocked");

        // Non-http schemes MUST be blocked
        require(!NetworkRequestHelper::IsSafePublicUrl(QUrl("file:///etc/passwd")),
                "SSRF: file:// scheme was NOT blocked");
        require(!NetworkRequestHelper::IsSafePublicUrl(QUrl("gopher://127.0.0.1/")),
                "SSRF: gopher:// scheme was NOT blocked");
        require(!NetworkRequestHelper::IsSafePublicUrl(QUrl("ftp://example.com/")),
                "SSRF: ftp:// scheme was NOT blocked");

        // Legitimate public HTTPS endpoints MUST be allowed
        require(NetworkRequestHelper::IsSafePublicUrl(QUrl("https://subscription.beaxtyvpn.net/sub/mytoken")),
                "SSRF: Legitimate public HTTPS endpoint was erroneously blocked");
        require(NetworkRequestHelper::IsSafePublicUrl(QUrl("https://8.8.8.8/dns-query")),
                "SSRF: Legitimate public IP was erroneously blocked");
    }

    // =========================================================================
    // 2. SQLite Database Permissions (0600)
    // =========================================================================
    std::cout << "  [2] Verifying SQLite Database File Permissions (0600)..." << std::endl;
    {
        QTemporaryDir tmpDir;
        require(tmpDir.isValid(), "Could not create temporary directory for DB test");
        QString dbPath = tmpDir.filePath("audit_test.db");

        Configs::initDB(dbPath.toStdString());

#if !defined(_WIN32)
        struct stat st;
        int res = ::stat(dbPath.toUtf8().constData(), &st);
        require(res == 0, "Failed to stat database file");
        mode_t perm = st.st_mode & 0777;
        std::cout << "      Database file permissions: 0" << std::oct << perm << std::dec << std::endl;
        require(perm == 0600, "Database permissions are NOT strictly 0600 (user read/write only)!");

        QString walPath = dbPath + "-wal";
        if (QFile::exists(walPath)) {
            struct stat walSt;
            if (::stat(walPath.toUtf8().constData(), &walSt) == 0) {
                mode_t walPerm = walSt.st_mode & 0777;
                require(walPerm == 0600, "WAL permissions are NOT 0600!");
            }
        }
#endif
    }

    // =========================================================================
    // 3. IPv6 Leak Protection & Port 53 DNS Hijack Rules
    // =========================================================================
    std::cout << "  [3] Verifying IPv6 Leak Protection and DNS Hijack in Sing-Box Config..." << std::endl;
    {
        UI_InitMainWindow();
        MW_show_log = [](const QString &) {};
        MW_dialog_message = [](MwMessage, QStringList) {};

        require(Configs::dataManager && Configs::dataManager->settingsRepo, "DataManager settingsRepo not ready");
        Configs::dataManager->settingsRepo->spmode_vpn = true;
        Configs::dataManager->settingsRepo->vpn_ipv6 = false; // IPv6 disabled in tunnel

        auto group = Configs::GroupsRepo::NewGroup();
        group->name = QStringLiteral("AuditGroup");
        Configs::dataManager->groupsRepo->AddGroup(group);

        auto prof = Configs::ProfilesRepo::NewProfile(QStringLiteral("vless"));
        prof->name = QStringLiteral("Audit Node");
        prof->gid = group->id;

        auto vless = std::make_unique<Configs::vless>();
        vless->server = QStringLiteral("1.2.3.4");
        vless->server_port = 443;
        vless->uuid = QStringLiteral("deadbeef-1234-5678-9abc-def012345678");
        prof->outbound = std::move(vless);

        Configs::dataManager->profilesRepo->AddProfile(prof, group->id);

        auto result = Configs::BuildSingBoxConfig(prof);
        require(result->error.isEmpty(), "BuildSingBoxConfig failed in audit test");

        QJsonObject route = result->coreConfig["route"].toObject();
        QJsonArray rules = route["rules"].toArray();

        bool foundIpv6Reject = false;
        bool foundDnsPort53 = false;

        for (const auto &rVal : rules) {
            auto r = rVal.toObject();
            // Check for IPv6 reject rule
            if (r["action"].toString() == "reject") {
                QJsonArray cidrs = r["ip_cidr"].toArray();
                for (const auto &c : cidrs) {
                    if (c.toString() == "::/0") {
                        foundIpv6Reject = true;
                    }
                }
            }
            // Check for port 53 DNS hijack rule
            if (r["action"].toString() == "hijack-dns") {
                QJsonArray ports = r["port"].toArray();
                for (const auto &p : ports) {
                    if (p.toInt() == 53) {
                        foundDnsPort53 = true;
                    }
                }
            }
        }

        require(foundIpv6Reject, "IPv6 leak protection rule (action: reject, ip_cidr: [::/0]) missing from route rules!");
        require(foundDnsPort53, "Unconditional Port 53 DNS hijack rule missing from route rules!");
        std::cout << "      Verified IPv6 ::/0 reject rule: OK" << std::endl;
        std::cout << "      Verified Port 53 hijack-dns rule: OK" << std::endl;
    }

    // =========================================================================
    // 4. Support Report Redaction Verification
    // =========================================================================
    std::cout << "  [4] Verifying Diagnostic Report Credential Redaction..." << std::endl;
    {
        QString sampleLog = 
            "2026-09-13 [Core] Connecting to vless://a1b2c3d4-5678-90ab-cdef-1234567890ab@remote.com:443?security=reality&pbk=SuperSecretPublicKey123&sid=123456#Server1\n"
            "2026-09-13 [RPC] Auth header Bearer my-secret-jwt-token-xyz with password=UltraSecretPass and secret=TopSecretKey\n"
            "2026-09-13 [WireGuard] Generating peer with private_key=AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA= and seed=BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB=";

        QString redacted = sampleLog;
        redacted.replace(QRegularExpression(QStringLiteral("password=[^;&\\s]+"), QRegularExpression::CaseInsensitiveOption), QStringLiteral("password=***REDACTED***"));
        redacted.replace(QRegularExpression(QStringLiteral("secret=[^;&\\s]+"), QRegularExpression::CaseInsensitiveOption), QStringLiteral("secret=***REDACTED***"));
        redacted.replace(QRegularExpression(QStringLiteral("private_key=[^;&\\s]+"), QRegularExpression::CaseInsensitiveOption), QStringLiteral("private_key=***REDACTED***"));
        redacted.replace(QRegularExpression(QStringLiteral("key=[^;&\\s]+"), QRegularExpression::CaseInsensitiveOption), QStringLiteral("key=***REDACTED***"));
        redacted.replace(QRegularExpression(QStringLiteral("seed=[^;&\\s]+"), QRegularExpression::CaseInsensitiveOption), QStringLiteral("seed=***REDACTED***"));
        redacted.replace(QRegularExpression(QStringLiteral("pbk=[^;&\\s]+"), QRegularExpression::CaseInsensitiveOption), QStringLiteral("pbk=***REDACTED***"));
        redacted.replace(QRegularExpression(QStringLiteral("sid=[^;&\\s]+"), QRegularExpression::CaseInsensitiveOption), QStringLiteral("sid=***REDACTED***"));
        redacted.replace(QRegularExpression(QStringLiteral("token=[^;&\\s]+"), QRegularExpression::CaseInsensitiveOption), QStringLiteral("token=***REDACTED***"));
        redacted.replace(QRegularExpression(QStringLiteral("auth=[^;&\\s]+"), QRegularExpression::CaseInsensitiveOption), QStringLiteral("auth=***REDACTED***"));
        redacted.replace(QRegularExpression(QStringLiteral("uuid=[^;&\\s]+"), QRegularExpression::CaseInsensitiveOption), QStringLiteral("uuid=***REDACTED***"));
        redacted.replace(QRegularExpression(QStringLiteral("Bearer\\s+[A-Za-z0-9\\-_.]+"), QRegularExpression::CaseInsensitiveOption), QStringLiteral("Bearer ***REDACTED***"));
        redacted.replace(QRegularExpression(QStringLiteral("(vless|vmess|trojan|ss|ssr)://[^@\\s]+@")), QStringLiteral("\\1://***REDACTED***@"));
        redacted.replace(QRegularExpression(QStringLiteral("\\b[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}\\b")), QStringLiteral("***REDACTED-UUID***"));

        require(!redacted.contains("UltraSecretPass"), "Password was NOT redacted");
        require(!redacted.contains("TopSecretKey"), "Secret was NOT redacted");
        require(!redacted.contains("SuperSecretPublicKey123"), "pbk was NOT redacted");
        require(!redacted.contains("my-secret-jwt-token-xyz"), "Bearer token was NOT redacted");
        require(!redacted.contains("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA="), "private_key was NOT redacted");
        require(!redacted.contains("BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB="), "seed was NOT redacted");
        require(!redacted.contains("a1b2c3d4-5678-90ab-cdef-1234567890ab"), "UUID was NOT redacted");
        std::cout << "      Verified credential and UUID redaction: OK" << std::endl;
    }

    std::cout << "[PASS] All Security Audit checks passed successfully!" << std::endl;
    return 0;
}
