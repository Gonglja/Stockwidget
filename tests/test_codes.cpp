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
    void rejectsInvalid() {
        QVERIFY(!StockCode::normalize("").has_value());
        QVERIFY(!StockCode::normalize("abc").has_value());
        QVERIFY(!StockCode::normalize("12345").has_value());
        QVERIFY(!StockCode::normalize("700000").has_value());
    }
    void dedupesPreservingOrder() {
        const QStringList in{"600000", "sh600000", "000001", "sz000001", "bad"};
        QCOMPARE(StockCode::normalizeList(in), QStringList({"sh600000", "sz000001"}));
    }
};

QTEST_GUILESS_MAIN(TestCodes)
#include "test_codes.moc"
