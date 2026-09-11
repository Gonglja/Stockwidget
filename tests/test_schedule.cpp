#include <QtTest>
#include "data/Schedule.h"

class TestSchedule : public QObject {
    Q_OBJECT
private slots:
    void parsesHhMm() {
        QCOMPARE(Schedule::minutesFromHhMm("09:15"), 555);
        QCOMPARE(Schedule::minutesFromHhMm("15:00"), 900);
        QCOMPARE(Schedule::minutesFromHhMm("00:00"), 0);
        QCOMPARE(Schedule::minutesFromHhMm("23:59"), 23 * 60 + 59);
        QCOMPARE(Schedule::minutesFromHhMm("25:00"), -1);
        QCOMPARE(Schedule::minutesFromHhMm("9:65"), -1);
        QCOMPARE(Schedule::minutesFromHhMm(""), -1);
        QCOMPARE(Schedule::minutesFromHhMm("abc"), -1);
    }
    void marketWindow() {
        // 周一 10:00 → true
        QVERIFY(Schedule::inWindow("market", "", "", 600, 1));
        // 边界 9:15 与 15:00 → true
        QVERIFY(Schedule::inWindow("market", "", "", 555, 1));
        QVERIFY(Schedule::inWindow("market", "", "", 900, 1));
        // 9:14 / 15:01 → false
        QVERIFY(!Schedule::inWindow("market", "", "", 554, 1));
        QVERIFY(!Schedule::inWindow("market", "", "", 901, 1));
        // 周六 / 周日 → false
        QVERIFY(!Schedule::inWindow("market", "", "", 600, 6));
        QVERIFY(!Schedule::inWindow("market", "", "", 600, 7));
        // 周五 10:00 → true
        QVERIFY(Schedule::inWindow("market", "", "", 600, 5));
    }
    void customWindow() {
        QVERIFY(Schedule::inWindow("custom", "08:00", "20:00", 9 * 60, 1));
        QVERIFY(!Schedule::inWindow("custom", "08:00", "20:00", 21 * 60, 1));
        QVERIFY(Schedule::inWindow("custom", "08:00", "20:00", 8 * 60, 1));   // 8:00 边界
        QVERIFY(Schedule::inWindow("custom", "08:00", "20:00", 20 * 60, 1));  // 20:00 边界
        // 非法时间 → false
        QVERIFY(!Schedule::inWindow("custom", "bad", "20:00", 10 * 60, 1));
    }
    void alwaysWindow() {
        QVERIFY(Schedule::inWindow("always", "", "", 0, 6));
        QVERIFY(Schedule::inWindow("always", "", "", 23 * 60 + 59, 7));
    }
};

QTEST_GUILESS_MAIN(TestSchedule)
#include "test_schedule.moc"
