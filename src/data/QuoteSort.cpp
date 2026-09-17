#include "data/QuoteSort.h"
#include <algorithm>

namespace {

const QStringList kKeys{QStringLiteral("code"),    QStringLiteral("name"),
                        QStringLiteral("price"),   QStringLiteral("change"),
                        QStringLiteral("change_pct"), QStringLiteral("buy1"),
                        QStringLiteral("sell1"),   QStringLiteral("commi"),
                        QStringLiteral("vol"),     QStringLiteral("amount"),
                        QStringLiteral("avg")};

bool isTextKey(const QString& key) {
    return key == QStringLiteral("code") || key == QStringLiteral("name");
}

double numericValue(const Quote& q, const QString& key) {
    if (key == QStringLiteral("price")) return q.price;
    if (key == QStringLiteral("change")) return q.prevClose ? q.price - q.prevClose : 0.0;
    if (key == QStringLiteral("change_pct"))
        return q.prevClose ? (q.price / q.prevClose - 1.0) * 100.0 : 0.0;
    if (key == QStringLiteral("buy1")) return q.buy1;
    if (key == QStringLiteral("sell1")) return q.sell1;
    if (key == QStringLiteral("commi")) return q.committee;
    if (key == QStringLiteral("vol")) return q.volume;
    if (key == QStringLiteral("amount")) return q.amount;
    if (key == QStringLiteral("avg")) return q.avg;
    return 0.0;
}

}  // namespace

QStringList QuoteSort::sortableKeys() { return kKeys; }

bool QuoteSort::isValidKey(const QString& key) { return kKeys.contains(key); }

void QuoteSort::sortQuotes(QVector<Quote>& quotes, const QString& key, bool asc) {
    if (!isValidKey(key) || quotes.size() < 2) return;
    if (isTextKey(key)) {
        const bool byCode = key == QStringLiteral("code");
        std::stable_sort(quotes.begin(), quotes.end(),
                         [byCode, asc](const Quote& a, const Quote& b) {
                             const int cmp = byCode ? QString::compare(a.code, b.code)
                                                    : QString::compare(a.name, b.name);
                             return asc ? cmp < 0 : cmp > 0;
                         });
        return;
    }
    std::stable_sort(quotes.begin(), quotes.end(), [&key, asc](const Quote& a, const Quote& b) {
        const double va = numericValue(a, key);
        const double vb = numericValue(b, key);
        return asc ? va < vb : va > vb;
    });
}
