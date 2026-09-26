// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <iostream>
#include <QCoreApplication>
#include "src/core/DeepLinkManager.hpp"
#include "3rdparty/throne/include/global/HTTPRequestHelper.hpp"

int main(int argc, char *argv[]) {
    // Ensure loopback testing is not blocked by test environment if needed, but test strict production rejection
    qunsetenv("BEAXTY_ALLOW_LOCAL_TEST_REQUESTS");
    qputenv("QT_QPA_PLATFORM", "offscreen");

    QCoreApplication app(argc, argv);

    std::cout << "[TestDeepLink] Running deep link parsing & security tests..." << std::endl;

    DeepLinkManager manager;

    // Cabinet WebEngine policy uses Qt's parsed URL components, not string
    // splitting in QML. Reject confusing authorities and unexpected origins.
    assert(manager.isTrustedCabinetUrl(QStringLiteral("https://cabinet.beaxty.com/login")));
    assert(manager.isTrustedCabinetUrl(QStringLiteral("https://cabinet.beaxty.com:443/login")));
    assert(!manager.isTrustedCabinetUrl(QStringLiteral("http://cabinet.beaxty.com/login")));
    assert(!manager.isTrustedCabinetUrl(QStringLiteral("https://cabinet.beaxty.com:444/login")));
    assert(!manager.isTrustedCabinetUrl(QStringLiteral("https://auth.beaxty.com/login")));
    assert(!manager.isTrustedCabinetUrl(QStringLiteral("https://cabinet.beaxty.com.evil.example/")));
    assert(!manager.isTrustedCabinetUrl(QStringLiteral("https://cabinet.beaxty.com@evil.example/")));
    assert(!manager.isTrustedCabinetUrl(QStringLiteral("https://@cabinet.beaxty.com/")));
    assert(manager.isAllowedExternalUrl(QStringLiteral("https://accounts.google.com/signin")));
    assert(!manager.isAllowedExternalUrl(QStringLiteral("http://accounts.google.com/signin")));
    assert(!manager.isAllowedExternalUrl(QStringLiteral("https://user@accounts.google.com/")));
    assert(!manager.isAllowedExternalUrl(QStringLiteral("https://127.0.0.1/")));
    assert(!manager.isAllowedExternalUrl(QStringLiteral("https://192.168.1.10/")));
    assert(!manager.isAllowedExternalUrl(QStringLiteral("javascript:alert(1)")));
    assert(manager.isBeaxtyUrl(QStringLiteral("beaxty://import?url=https%3A%2F%2Fexample.com%2Fsub")));
    assert(!manager.isBeaxtyUrl(QStringLiteral("beaxty://%zz")));
    std::cout << "  [PASS] Strict URL policy for cabinet, external links, and deep links" << std::endl;

    // Test 1: Valid import deeplink with https URL and custom name
    {
        QString raw = QStringLiteral("beaxty://import?url=https%3A%2F%2Fcabinet.beaxty.com%2Fapi%2Fsub%2Ftest123&name=My%20Custom%20Sub");
        QString targetUrl, groupName, error;
        bool ok = DeepLinkManager::parseDeepLink(raw, targetUrl, groupName, &error);
        assert(ok);
        assert(targetUrl == QStringLiteral("https://cabinet.beaxty.com/api/sub/test123"));
        assert(groupName == QStringLiteral("My Custom Sub"));
        std::cout << "  [PASS] Valid import deeplink" << std::endl;
    }

    // Test 2: Valid subscribe deeplink without name
    {
        QString raw = QStringLiteral("beaxty://subscribe?url=https://example.com/sub/nodes.txt");
        QString targetUrl, groupName, error;
        bool ok = DeepLinkManager::parseDeepLink(raw, targetUrl, groupName, &error);
        assert(ok);
        assert(targetUrl == QStringLiteral("https://example.com/sub/nodes.txt"));
        assert(groupName.isEmpty());
        std::cout << "  [PASS] Valid subscribe deeplink" << std::endl;
    }

    // Test 3: Rejection of invalid scheme (not beaxty://)
    {
        QString raw = QStringLiteral("https://import?url=https://cabinet.beaxty.com");
        QString targetUrl, groupName, error;
        bool ok = DeepLinkManager::parseDeepLink(raw, targetUrl, groupName, &error);
        assert(!ok);
        assert(error.contains(QStringLiteral("beaxty://")));
        std::cout << "  [PASS] Reject non-beaxty scheme" << std::endl;
    }

    // Test 4: Rejection of unsupported action
    {
        QString raw = QStringLiteral("beaxty://delete?url=https://cabinet.beaxty.com");
        QString targetUrl, groupName, error;
        bool ok = DeepLinkManager::parseDeepLink(raw, targetUrl, groupName, &error);
        assert(!ok);
        assert(error.contains(QStringLiteral("Неподдерживаемое действие")));
        std::cout << "  [PASS] Reject unsupported action" << std::endl;
    }

    // Test 5: Rejection of missing url param
    {
        QString raw = QStringLiteral("beaxty://import?name=Hello");
        QString targetUrl, groupName, error;
        bool ok = DeepLinkManager::parseDeepLink(raw, targetUrl, groupName, &error);
        assert(!ok);
        assert(error.contains(QStringLiteral("url=")));
        std::cout << "  [PASS] Reject missing url param" << std::endl;
    }

    // Test 6: SSRF - Cloud Metadata (169.254.169.254) rejection
    {
        QString raw = QStringLiteral("beaxty://import?url=http://169.254.169.254/latest/meta-data/");
        QString targetUrl, groupName, error;
        bool ok = DeepLinkManager::parseDeepLink(raw, targetUrl, groupName, &error);
        assert(!ok);
        assert(error.contains(QStringLiteral("SSRF")));
        std::cout << "  [PASS] SSRF: Reject cloud metadata IP" << std::endl;
    }

    // Test 7: SSRF - Localhost / Loopback rejection
    {
        QString raw = QStringLiteral("beaxty://import?url=http://127.0.0.1:8080/secret");
        QString targetUrl, groupName, error;
        bool ok = DeepLinkManager::parseDeepLink(raw, targetUrl, groupName, &error);
        assert(!ok);
        assert(error.contains(QStringLiteral("SSRF")));
        std::cout << "  [PASS] SSRF: Reject loopback 127.0.0.1" << std::endl;
    }

    // Test 8: Rejection of file:// scheme
    {
        QString raw = QStringLiteral("beaxty://import?url=file:///etc/passwd");
        QString targetUrl, groupName, error;
        bool ok = DeepLinkManager::parseDeepLink(raw, targetUrl, groupName, &error);
        assert(!ok);
        assert(error.contains(QStringLiteral("Недопустимый протокол")));
        std::cout << "  [PASS] Reject file:// scheme" << std::endl;
    }

    // Incoming links are externally supplied protocol input. Bound their size
    // and require strict URL parsing before decoding the embedded subscription.
    {
        QString targetUrl, groupName, error;
        assert(!DeepLinkManager::parseDeepLink(QString(8193, QLatin1Char('a')),
                                               targetUrl, groupName, &error));
        assert(!DeepLinkManager::parseDeepLink(QStringLiteral(" beaxty://import?url=https://example.com/sub"),
                                               targetUrl, groupName, &error));
        assert(!DeepLinkManager::parseDeepLink(
            QStringLiteral("beaxty://import?url=https%3A%2F%2Fuser%3Apass%40example.com%2Fsub"),
            targetUrl, groupName, &error));
        std::cout << "  [PASS] Bound deep link size and reject malformed credential URLs" << std::endl;
    }

    // Test 9: Manager signal emission on valid deeplink
    {
        bool signalReceived = false;
        QString receivedUrl;
        QString receivedGroup;
        QObject::connect(&manager, &DeepLinkManager::deepLinkReceived, [&](const QString &url, const QString &group) {
            signalReceived = true;
            receivedUrl = url;
            receivedGroup = group;
        });

        bool handled = manager.handleDeepLink(QStringLiteral("beaxty://import?url=https://cabinet.beaxty.com/sub&name=Beaxty%20Premium"));
        assert(handled);
        assert(signalReceived);
        assert(receivedUrl == QStringLiteral("https://cabinet.beaxty.com/sub"));
        assert(receivedGroup == QStringLiteral("Beaxty Premium"));
        std::cout << "  [PASS] Manager signal emission" << std::endl;
    }

    std::cout << "[TestDeepLink] All tests passed successfully!" << std::endl;
    return 0;
}
