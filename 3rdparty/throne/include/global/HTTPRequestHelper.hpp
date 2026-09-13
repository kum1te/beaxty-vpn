#pragma once

#include <QObject>
#include <functional>

namespace Configs_network {
    struct HTTPResponse {
        QString error;
        QByteArray data;
        QList<QPair<QByteArray, QByteArray>> header;
    };

    struct DownloadProgressReport
    {
        QString fileName;
        qint64 downloadedSize;
        qint64 totalSize;
    };

    class NetworkRequestHelper : QObject {
        Q_OBJECT

        explicit NetworkRequestHelper(QObject *parent) : QObject(parent){};

        ~NetworkRequestHelper() override = default;
        ;

    public:
        // maxBytes > 0 aborts the transfer once the body exceeds it and reports an error.
        static HTTPResponse HttpGet(const QString &url, bool sendHwid = false, bool useProxy = false, qint64 maxBytes = 0);

        // Callback runs on context's thread; destroying context cancels the request.
        static void HttpGetAsync(QObject *context, const QString &url, bool sendHwid,
                                 bool useProxy, qint64 maxBytes,
                                 std::function<void(HTTPResponse)> done);

        static QString GetHeader(const QList<QPair<QByteArray, QByteArray>> &header, const QString &name);

        static QString DownloadAsset(const QString &url, const QString &fileName, bool useProxy = false);
    };
} // namespace Configs_network

using namespace Configs_network;
