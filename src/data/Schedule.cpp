#include "data/Schedule.h"

namespace {

// 开盘时段：周一~五 9:15–15:00（连续，不分午休，不含节假日）。
constexpr int kMarketStart = 9 * 60 + 15;  // 555
constexpr int kMarketEnd = 15 * 60;        // 900

}  // namespace

int Schedule::minutesFromHhMm(const QString& hhmm) {
    const int idx = hhmm.indexOf(QLatin1Char(':'));
    if (idx <= 0) return -1;
    bool okH = false, okM = false;
    const int h = hhmm.left(idx).toInt(&okH);
    const int m = hhmm.mid(idx + 1).toInt(&okM);
    if (!okH || !okM || h < 0 || h > 23 || m < 0 || m > 59) return -1;
    return h * 60 + m;
}

bool Schedule::inWindow(const QString& mode, const QString& start, const QString& end,
                        int nowMinutes, int weekday) {
    if (mode == QStringLiteral("always")) return true;
    if (mode == QStringLiteral("market")) {
        if (weekday >= 6) return false;  // 周六/周日
        return nowMinutes >= kMarketStart && nowMinutes <= kMarketEnd;
    }
    if (mode == QStringLiteral("custom")) {
        const int s = minutesFromHhMm(start);
        const int e = minutesFromHhMm(end);
        if (s < 0 || e < 0) return false;
        return nowMinutes >= s && nowMinutes <= e;
    }
    return true;
}
