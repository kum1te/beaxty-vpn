// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

#pragma once

#include <QObject>
#include <QString>
#include <QTranslator>
#include <QQmlApplicationEngine>

class BeaxtyTranslator : public QTranslator {
    Q_OBJECT
public:
    explicit BeaxtyTranslator(QObject *parent = nullptr);
    QString translate(const char *context, const char *sourceText,
                      const char *disambiguation = nullptr, int n = -1) const override;
    bool isEmpty() const override;

    void setLanguage(const QString &lang);
    QString language() const { return m_lang; }

private:
    QString m_lang = QStringLiteral("ru");
};

class LocalizationManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)

public:
    explicit LocalizationManager(QObject *parent = nullptr);
    static LocalizationManager *instance();

    void initialize(QQmlApplicationEngine *engine);

    QString language() const;
    Q_INVOKABLE void setLanguage(const QString &lang);

signals:
    void languageChanged(const QString &lang);

private:
    static LocalizationManager *s_instance;
    QQmlApplicationEngine *m_engine = nullptr;
    BeaxtyTranslator m_translator;
    QString m_language = QStringLiteral("ru");
};
