#pragma once
#include "data/Quote.h"
#include <QNetworkAccessManager>
#include <QObject>

class QNetworkReply;

class SinaQuoteSource : public QObject {
    Q_OBJECT
public:
    explicit SinaQuoteSource(QObject* parent = nullptr);
    void setFormatOptions(const QuoteFormatOptions& opt);
    void fetch(const QStringList& codes);

signals:
    void quotesReady(const QVector<Quote>& quotes);
    void error(const QString& message);

private:
    QNetworkAccessManager m_nam;
    QuoteFormatOptions m_opt;
    QNetworkReply* m_reply = nullptr;
};
