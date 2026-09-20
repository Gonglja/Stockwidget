#pragma once
#include "data/Quote.h"
#include <QNetworkAccessManager>
#include <QObject>

class QNetworkReply;

class SinaQuoteSource : public QObject {
    Q_OBJECT
public:
    explicit SinaQuoteSource(QObject* parent = nullptr);
    // 注入外部 NAM（不拥有，nullptr = 恢复内置）。测试用来避免真实联网。
    void setNetworkAccessManager(QNetworkAccessManager* nam);
    void setFormatOptions(const QuoteFormatOptions& opt);
    void fetch(const QStringList& codes);

signals:
    void quotesReady(const QVector<Quote>& quotes);
    void error(const QString& message);

private:
    QNetworkAccessManager* m_nam = nullptr;       // 外部注入或内置
    QNetworkAccessManager* m_ownedNam = nullptr;  // 内置实例（父对象 = this）
    QuoteFormatOptions m_opt;
    QNetworkReply* m_reply = nullptr;
};
