#include "data/StockCode.h"
#include <QRegularExpression>
#include <QSet>

namespace {
// A 股：sh/sz/bj + 6 位；港股：hk + 5 位
const QRegularExpression kPrefixed(QStringLiteral("^(sh|sz|bj)\\d{6}$|^hk\\d{5}$"));
const QRegularExpression kNumeric(QStringLiteral("^\\d{6}$"));
// 裸港股代码：形如 00700 / 09988（港股主板 5 位且以 0 开头）。
// 其他 1~4 位数字留给「模糊搜索」（如 600 / 6000 → 600000）。
const QRegularExpression kHkDigits(QStringLiteral("^0\\d{4}$"));
const QRegularExpression kDigits(QStringLiteral("^\\d{1,5}$"));
}  // namespace

std::optional<QString> StockCode::normalize(const QString& input) {
    QString s = input.trimmed().toLower();
    s.remove(QRegularExpression(QStringLiteral("[^a-z0-9]")));
    if (s.isEmpty()) return std::nullopt;
    // 容错：hk700 → hk00700（港股代码固定 5 位，前导 0 可省略）
    if (s.startsWith(QStringLiteral("hk"))) {
        const QString digits = s.mid(2);
        if (kDigits.match(digits).hasMatch())
            return QStringLiteral("hk") + digits.rightJustified(5, QLatin1Char('0'));
        return std::nullopt;
    }
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
        return std::nullopt;
    }
    // 1~5 位纯数字：仅以 0 开头的 5 位按港股处理（如 00700 → hk00700）；
    // 其他留给模糊搜索（如 6000 可能是 600000 的前缀）。
    if (kHkDigits.match(s).hasMatch())
        return QStringLiteral("hk") + s;
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
