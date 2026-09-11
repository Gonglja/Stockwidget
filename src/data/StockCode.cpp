#include "data/StockCode.h"
#include <QRegularExpression>
#include <QSet>

namespace {
const QRegularExpression kPrefixed(QStringLiteral("^(sh|sz|bj)\\d+$"));
const QRegularExpression kNumeric(QStringLiteral("^\\d{6}$"));
}

std::optional<QString> StockCode::normalize(const QString& input) {
    QString s = input.trimmed().toLower();
    s.remove(QRegularExpression(QStringLiteral("[^a-z0-9]")));
    if (s.isEmpty()) return std::nullopt;
    if (kPrefixed.match(s).hasMatch()) return s;
    if (kNumeric.match(s).hasMatch()) {
        if (s.startsWith(QStringLiteral("6")) || s.startsWith(QStringLiteral("90")) ||
            s.startsWith(QStringLiteral("5")))
            return QStringLiteral("sh") + s;
        if (s.startsWith(QStringLiteral("0")) || s.startsWith(QStringLiteral("3")) ||
            s.startsWith(QStringLiteral("2")) || s.startsWith(QStringLiteral("1")))
            return QStringLiteral("sz") + s;
        if (s.startsWith(QStringLiteral("8")) || s.startsWith(QStringLiteral("4")) ||
            s.startsWith(QStringLiteral("92")))
            return QStringLiteral("bj") + s;
    }
    return std::nullopt;
}

QStringList StockCode::normalizeList(const QStringList& inputs) {
    QStringList out;
    QSet<QString> seen;
    for (const QString& in : inputs) {
        const auto n = normalize(in);
        if (!n || seen.contains(*n)) continue;
        seen.insert(*n);
        out.append(*n);
    }
    return out;
}
