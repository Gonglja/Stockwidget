#include "data/SinaQuoteSource.h"
#include "data/QuoteParser.h"
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTextCodec>

SinaQuoteSource::SinaQuoteSource(QObject* parent) : QObject(parent) {}

void SinaQuoteSource::setFormatOptions(const QuoteFormatOptions& opt) { m_opt = opt; }

void SinaQuoteSource::fetch(const QStringList& codes) {
    if (codes.isEmpty()) {
        emit error(QStringLiteral("暂无数据，请添加自选"));
        return;
    }
    if (m_reply) {
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }

    QNetworkRequest req(QUrl(QStringLiteral("https://hq.sinajs.cn/list=") +
                             codes.join(QLatin1Char(','))));
    req.setRawHeader("Referer", "https://finance.sina.com.cn");
    req.setRawHeader("User-Agent", "Mozilla/5.0");
    req.setTransferTimeout(3000);

    m_reply = m_nam.get(req);
    connect(m_reply, &QNetworkReply::finished, this, [this] {
        QNetworkReply* reply = m_reply;
        m_reply = nullptr;
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit error(QStringLiteral("无网络连接"));
            return;
        }
        const QByteArray bytes = reply->readAll();
        QTextCodec* codec = QTextCodec::codecForName("GB18030");
        const QString text = codec ? codec->toUnicode(bytes) : QString::fromUtf8(bytes);
        emit quotesReady(QuoteParser::parseText(text, m_opt));
    });
}
