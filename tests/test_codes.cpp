#include <QtTest>
#include "data/StockCode.h"

class TestCodes : public QObject {
    Q_OBJECT
private slots:
    void normalizesShanghai() {
        QCOMPARE(StockCode::normalize("600000").value(), QString("sh600000"));
        QCOMPARE(StockCode::normalize("900901").value(), QString("sh900901"));
        QCOMPARE(StockCode::normalize("510300").value(), QString("sh510300"));
        QCOMPARE(StockCode::normalize("  SH600000 ").value(), QString("sh600000"));
    }
    void normalizesShenzhen() {
        QCOMPARE(StockCode::normalize("000001").value(), QString("sz000001"));
        QCOMPARE(StockCode::normalize("300136").value(), QString("sz300136"));
        QCOMPARE(StockCode::normalize("159915").value(), QString("sz159915"));
    }
    void normalizesBeijing() {
        QCOMPARE(StockCode::normalize("830799").value(), QString("bj830799"));
        QCOMPARE(StockCode::normalize("430047").value(), QString("bj430047"));
        QCOMPARE(StockCode::normalize("920001").value(), QString("bj920001"));
    }
    void normalizesHongKong() {
        QCOMPARE(StockCode::normalize("00700").value(), QString("hk00700"));
        QCOMPARE(StockCode::normalize("09988").value(), QString("hk09988"));
        QCOMPARE(StockCode::normalize("hk00700").value(), QString("hk00700"));
        QCOMPARE(StockCode::normalize("HK700").value(), QString("hk00700"));
        QCOMPARE(StockCode::normalize("  hk00001 ").value(), QString("hk00001"));
    }
    void keepsShortDigitsForSuggestion() {
        // 1~4 位数字不当作港股，避免抢走模糊搜索（如 6000 → 600000）
        QVERIFY(!StockCode::normalize("700").has_value());
        QVERIFY(!StockCode::normalize("6000").has_value());
    }
    void rejectsInvalid() {
        QVERIFY(!StockCode::normalize("").has_value());
        QVERIFY(!StockCode::normalize("abc").has_value());
        QVERIFY(!StockCode::normalize("700000").has_value());
        QVERIFY(!StockCode::normalize("1234567").has_value());
        QVERIFY(!StockCode::normalize("hk123456").has_value());
    }
    void dedupesPreservingOrder() {
        const QStringList in{"600000", "sh600000", "000001", "sz000001", "00700", "hk700",
                            "bad"};
        QCOMPARE(StockCode::normalizeList(in),
                 QStringList({"sh600000", "sz000001", "hk00700"}));
    }
};

QTEST_GUILESS_MAIN(TestCodes)
#include "test_codes.moc"
