// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

#pragma once

#include <QObject>
#include <QString>

class ToastManager : public QObject {
    Q_OBJECT

public:
    explicit ToastManager(QObject *parent = nullptr);
    ~ToastManager() override;

    static ToastManager *instance();

    Q_INVOKABLE void show(const QString &message, const QString &type = QStringLiteral("info"), int durationMs = 3500);
    Q_INVOKABLE void showError(const QString &message, int durationMs = 4500);
    Q_INVOKABLE void showSuccess(const QString &message, int durationMs = 3000);
    Q_INVOKABLE void showInfo(const QString &message, int durationMs = 3500);

signals:
    void toastRequested(const QString &message, const QString &type, int durationMs);

private:
    static ToastManager *s_instance;
};
