#include "data/SinaQuoteSource.h"
#include "data/QuoteParser.h"
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTextCodec>

SinaQuoteSource::SinaQuoteSource(QObject* parent) : QObject(parent) {
    m_ownedNam = new QNetworkAccessManager(this);
    m_nam = m_ownedNam;
}

void SinaQuoteSource::setNetworkAccessManager(QNetworkAccessManager* nam) {
    m_nam = nam ? nam : m_ownedNam;
}

void SinaQuoteSource::setFormatOptions(const QuoteFormatOptions& opt) { m_opt = opt; }

void SinaQuoteSource::fetch(const QStringList& codes) {
    if (!m_nam) return;
    if (codes.isEmpty()) {
        emit error(QStringLiteral("暂无数据，请添加自选"));
        return;
    }
    // 上一个请求仍在进行则跳过本次（不 cancel，避免慢网络下永远拉不到数据，
    // 也避免请求重叠导致的 reply 生命周期错乱）。
    if (m_reply) return;

    QNetworkRequest req{QUrl(QStringLiteral("https://hq.sinajs.cn/list=") +
                            codes.join(QLatin1Char(',')))};
    req.setRawHeader("Referer", "https://finance.sina.com.cn");
    req.setRawHeader("User-Agent", "Mozilla/5.0");
    req.setTransferTimeout(3000);

    QNetworkReply* reply = m_nam->get(req);
    m_reply = reply;
    // 必须捕获 reply 本身，不能读共享成员 m_reply：
    // 否则并发/重叠时会把别的 reply 置空并 deleteLater(nullptr) -> 崩溃。
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        if (m_reply == reply) m_reply = nullptr;
        reply->deleteLater();
        if (reply->error() == QNetworkReply::OperationCanceledError) return;
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
