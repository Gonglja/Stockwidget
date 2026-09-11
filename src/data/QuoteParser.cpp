#include "data/QuoteParser.h"
#include <QtMath>

namespace {

QString formatPrice(double v, bool etf) {
    return QString::number(v, 'f', etf ? 3 : 2);
}

QString signedNumber(qint64 v) {
    return (v > 0 ? QStringLiteral("+") : QString()) + QString::number(v);
}

QString formatVolume(double v) {
    if (v < 1e4) return QString::number(qint64(v));
    if (v < 1e8) return QString::number(v / 1e4, 'f', 2) + QStringLiteral("万");
    return QString::number(v / 1e8, 'f', 2) + QStringLiteral("亿");
}

QString formatAmount(double v) {
    if (v < 1e8) return QString::number(v / 1e4, 'f', 2) + QStringLiteral("万");
    if (v < 1e12) return QString::number(v / 1e8, 'f', 2) + QStringLiteral("亿");
    return QString::number(v / 1e12, 'f', 2) + QStringLiteral("万亿");
}

double toNum(const QString& s) { return s.isEmpty() ? 0.0 : s.toDouble(); }
qint64 toInt(const QString& s) { return s.isEmpty() ? 0LL : s.toLongLong(); }

}  // namespace

QVector<Quote> QuoteParser::parseText(const QString& text, const QuoteFormatOptions& opt) {
    QVector<Quote> out;
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (const QString& rawLine : lines) {
        const QString line = rawLine.trimmed();
        const int eq = line.indexOf(QStringLiteral("=\""));
        if (eq < 0) continue;

        const QString code = line.left(eq).split(QLatin1Char('_')).last().trimmed();
        const QStringList parts = line.mid(eq + 2).split(QLatin1Char(','));
        if (parts.size() < 32) continue;

        Quote q;
        q.code = code;
        q.name = parts.at(0);
        if (q.name.isEmpty()) continue;

        q.open = toNum(parts.at(1));
        q.prevClose = toNum(parts.at(2));
        q.price = toNum(parts.at(3));
        q.high = toNum(parts.at(4));
        q.low = toNum(parts.at(5));
        q.buy1 = toNum(parts.at(6));
        q.sell1 = toNum(parts.at(7));
        q.volume = toNum(parts.at(8));
        q.amount = toNum(parts.at(9));

        QVector<qint64> buyVols, sellVols;
        for (int i : {10, 12, 14, 16, 18}) buyVols.append(toInt(parts.at(i)));
        for (int i : {20, 22, 24, 26, 28}) sellVols.append(toInt(parts.at(i)));

        q.etf = code.size() >= 3 &&
                (code.at(2) == QLatin1Char('1') || code.at(2) == QLatin1Char('5'));
        const int dec = q.etf ? 3 : 2;
        const double scale = std::pow(10.0, dec);
        auto almostEq = [scale](double a, double b) {
            return qRound64(a * scale) == qRound64(b * scale);
        };

        QString buyMarker = QStringLiteral(" ");
        QString sellMarker = QStringLiteral(" ");
        if (q.buy1 > 0 && almostEq(q.price, q.buy1)) buyMarker = QStringLiteral("<");
        if (q.sell1 > 0 && almostEq(q.price, q.sell1)) sellMarker = QStringLiteral(">");

        const auto mode = opt.b1s1;
        if (q.buy1 == q.sell1 && q.buy1 > 0) {
            q.price = q.sell1;  // 集合竞价 9:15~9:25 / 14:57~15:00
            const qint64 paired = sellVols.at(0);
            const qint64 unpaired = (sellVols.at(1) > 0) ? -sellVols.at(1) : buyVols.at(1);
            const qint64 pairedCnt = paired / 100;
            const qint64 unpairedCnt = unpaired / 100;
            const QString bp = formatPrice(q.buy1, q.etf);
            const QString sp = formatPrice(q.sell1, q.etf);
            if (mode == QuoteFormatOptions::B1S1Display::Price) {
                q.b1Text = bp;
                q.s1Text = sp;
            } else if (mode == QuoteFormatOptions::B1S1Display::Both) {
                q.b1Text = QString::number(pairedCnt) + "(" + bp + ")";
                q.s1Text = signedNumber(unpairedCnt) + "(" + sp + ")";
            } else {
                q.b1Text = QString::number(pairedCnt);
                q.s1Text = signedNumber(unpairedCnt);
            }
            if (unpaired > 0) q.b1Sign = q.s1Sign = 1;
            else if (unpaired < 0) q.b1Sign = q.s1Sign = -1;
        } else {
            if (q.buy1 > 0) {
                const QString cnt = QString::number(buyVols.at(0) / 100);
                const QString bp = formatPrice(q.buy1, q.etf);
                if (mode == QuoteFormatOptions::B1S1Display::Price) q.b1Text = bp + buyMarker;
                else if (mode == QuoteFormatOptions::B1S1Display::Both)
                    q.b1Text = cnt + "(" + bp + ")" + buyMarker;
                else q.b1Text = cnt + buyMarker;
            } else {
                q.b1Text = QStringLiteral("-") + buyMarker;
            }
            if (q.sell1 > 0) {
                const QString cnt = QString::number(sellVols.at(0) / 100);
                const QString sp = formatPrice(q.sell1, q.etf);
                if (mode == QuoteFormatOptions::B1S1Display::Price) q.s1Text = sellMarker + sp;
                else if (mode == QuoteFormatOptions::B1S1Display::Both)
                    q.s1Text = sellMarker + cnt + "(" + sp + ")";
                else q.s1Text = sellMarker + cnt;
            } else {
                q.s1Text = sellMarker + QStringLiteral("-");
            }
            q.b1Sign = 1;
            q.s1Sign = -1;
        }

        if (q.price == 0) q.price = q.prevClose;  // 9:00~9:15 无数据
        if (q.open == 0) {
            q.open = q.price;
            q.high = q.price;
            q.low = q.price;
        }

        const double change = q.prevClose ? q.price - q.prevClose : 0.0;
        const double changePct = q.prevClose ? (q.price / q.prevClose - 1) * 100.0 : 0.0;
        q.avg = q.volume > 0 ? q.amount / q.volume : q.prevClose;

        qint64 pSum = 0, sSum = 0;
        for (qint64 v : buyVols) pSum += v;
        for (qint64 v : sellVols) sSum += v;
        q.committee = (pSum + sSum) > 0 ? 100.0 * (pSum - sSum) / (pSum + sSum) : 0.0;

        QString arrow = QStringLiteral(" ");
        if (q.high > q.low) {
            if (q.price == q.high) arrow = QStringLiteral("↑");
            else if (q.price == q.low) arrow = QStringLiteral("↓");
        }

        q.priceText = formatPrice(q.price, q.etf) + arrow;
        q.changeText = (change >= 0 ? QStringLiteral("+") : QString()) +
                       QString::number(change, 'f', dec);
        q.changePctText = (changePct >= 0 ? QStringLiteral("+") : QString()) +
                          QString::number(changePct, 'f', 2) + QStringLiteral("%");
        q.committeeText = (q.committee >= 0 ? QStringLiteral("+") : QString()) +
                          QString::number(q.committee, 'f', 2) + QStringLiteral("%");
        q.avgText = QString::number(q.avg, 'f', dec);
        q.volumeText = formatVolume(q.volume);
        q.amountText = formatAmount(q.amount);

        q.deltaSign = (change > 0) - (change < 0);
        q.committeeSign = (q.committee > 0) - (q.committee < 0);
        q.avgSign = (q.avg > q.prevClose) - (q.avg < q.prevClose);

        q.kOpen = q.open;
        q.kClose = q.price;
        q.kHigh = q.high;
        q.kLow = q.low;
        q.kPrev = q.prevClose;

        if (opt.nameLength > 0) q.name = q.name.left(opt.nameLength);
        if (opt.shortCode && q.code.size() > 2) q.code = q.code.mid(2);

        out.append(q);
    }
    return out;
}
