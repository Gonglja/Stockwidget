#include "data/StockSuggestSource.h"
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTextCodec>
#include <QUrl>

QVector<StockSuggestion> SuggestParser::parseSuggest(const QString& text) {
    QVector<StockSuggestion> out;
    const int eq = text.indexOf(QStringLiteral("=\""));
    if (eq < 0) return out;
    QString payload = text.mid(eq + 2);
    const int lastQuote = payload.lastIndexOf(QLatin1Char('"'));
    if (lastQuote >= 0) payload = payload.left(lastQuote);

    static const QRegularExpression kFullCode(QStringLiteral("^(sh|sz|bj)\\d{6}$"));
    const QStringList entries = payload.split(QLatin1Char(';'));
    for (const QString& entry : entries) {
        const QStringList parts = entry.split(QLatin1Char(','));
        if (parts.size() < 5) continue;
        // 实际格式：[0]带前缀代码 [1]类型 [2]代码 [3]带前缀代码 [4]名称 ...
        QString name = parts.at(4).trimmed();
        if (name.isEmpty() && parts.size() > 6) name = parts.at(6).trimmed();
        const QString full = parts.at(3).trimmed().toLower();
        if (name.isEmpty() || !kFullCode.match(full).hasMatch()) continue;
        out.append({full, name});
    }
    return out;
}

StockSuggestSource::StockSuggestSource(QObject* parent) : QObject(parent) {}

void StockSuggestSource::query(const QString& keyword) {
    const QString key = keyword.trimmed();
    if (key.isEmpty()) {
        emit suggestionsReady({});
        return;
    }
    if (m_reply) {
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }

    // 新浪 suggest 的 key 需要 UTF-8 百分号编码（实测：GBK 会返回通用列表）。
    // 用 QUrl::fromEncoded 构造，避免 QUrl 对 '%' 二次编码。
    const QByteArray encoded =
        QByteArray("https://suggest3.sinajs.cn/suggest/"
                   "type=11,12,13,14,15,21,22,23,24,25,26&key=") +
        key.toUtf8().toPercentEncoding();
    QNetworkRequest req{QUrl::fromEncoded(encoded)};
    req.setRawHeader("Referer", "https://finance.sina.com.cn");
    req.setRawHeader("User-Agent", "Mozilla/5.0");
    req.setTransferTimeout(3000);

    m_reply = m_nam.get(req);
    connect(m_reply, &QNetworkReply::finished, this, [this] {
        QNetworkReply* reply = m_reply;
        m_reply = nullptr;
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit error(QStringLiteral("查询失败"));
            return;
        }
        const QByteArray bytes = reply->readAll();
        QTextCodec* codec = QTextCodec::codecForName("GB18030");
        const QString text = codec ? codec->toUnicode(bytes) : QString::fromUtf8(bytes);
        emit suggestionsReady(SuggestParser::parseSuggest(text));
    });
}
