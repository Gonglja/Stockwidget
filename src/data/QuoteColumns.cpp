#include "data/QuoteColumns.h"

namespace {
struct Entry {
    const char* header;
    const char* key;
    const char* sortKey;
    bool right;
    bool colored;
};
const Entry kEntries[] = {
    {"代码",   "code_visible",       "code",       true,  false},
    {"名称",   "name_visible",       "name",       false, false},
    {"现价",   "price_visible",      "price",      true,  true},
    {"涨跌值", "change_visible",     "change",     true,  true},
    {"涨跌幅", "change_pct_visible", "change_pct", true,  true},
    {"买一",   "b1s1_visible",       "buy1",       true,  true},
    {"卖一",   "b1s1_visible",       "sell1",      false, true},
    {"委比",   "commi_visible",      "commi",      true,  true},
    {"成交量", "vol_visible",        "vol",        true,  false},
    {"成交额", "amount_visible",     "amount",     true,  false},
    {"均价",   "avg_visible",        "avg",        true,  true},
    {"K线",    "kline_visible",      "",           false, false},
};
}  // namespace

QStringList QuoteColumns::allHeaders() {
    QStringList out;
    for (const Entry& e : kEntries) out.append(QString::fromUtf8(e.header));
    return out;
}

QString QuoteColumns::configKeyFor(const QString& header) {
    for (const Entry& e : kEntries)
        if (QString::fromUtf8(e.header) == header) return QString::fromUtf8(e.key);
    return QString();
}

QString QuoteColumns::sortKeyFor(const QString& header) {
    for (const Entry& e : kEntries)
        if (QString::fromUtf8(e.header) == header) return QString::fromUtf8(e.sortKey);
    return QString();
}

bool QuoteColumns::isVisible(const QJsonObject& cfg, const QString& header) {
    const QString key = configKeyFor(header);
    return !key.isEmpty() && cfg.value(key).toBool(false);
}

QVector<ColumnSpec> QuoteColumns::activeColumns(const QJsonObject& cfg) {
    QVector<ColumnSpec> cols;
    for (const Entry& e : kEntries) {
        const QString header = QString::fromUtf8(e.header);
        if (!isVisible(cfg, header)) continue;

        ColumnSpec spec;
        spec.header = header;
        spec.rightAlign = e.right;
        spec.colored = e.colored;
        spec.isKLine = header == QStringLiteral("K线");
        spec.text = [header](const Quote& q) -> QString {
            if (header == QStringLiteral("代码")) return q.code;
            if (header == QStringLiteral("名称")) return q.name;
            if (header == QStringLiteral("现价")) return q.priceText;
            if (header == QStringLiteral("涨跌值")) return q.changeText;
            if (header == QStringLiteral("涨跌幅")) return q.changePctText;
            if (header == QStringLiteral("买一")) return q.b1Text;
            if (header == QStringLiteral("卖一")) return q.s1Text;
            if (header == QStringLiteral("委比")) return q.committeeText;
            if (header == QStringLiteral("成交量")) return q.volumeText;
            if (header == QStringLiteral("成交额")) return q.amountText;
            if (header == QStringLiteral("均价")) return q.avgText;
            return QString();
        };
        spec.sign = [header](const Quote& q) -> int {
            if (header == QStringLiteral("现价") || header == QStringLiteral("涨跌值") ||
                header == QStringLiteral("涨跌幅"))
                return q.deltaSign;
            if (header == QStringLiteral("买一")) return q.b1Sign;
            if (header == QStringLiteral("卖一")) return q.s1Sign;
            if (header == QStringLiteral("委比")) return q.committeeSign;
            if (header == QStringLiteral("均价")) return q.avgSign;
            return 0;
        };
        cols.append(spec);
    }
    return cols;
}
