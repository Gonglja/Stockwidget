#include <QtTest>
#include "data/Quote.h"
#include "data/QuoteColumns.h"
#include "data/QuoteSort.h"

namespace {

Quote makeQuote(const QString& code, const QString& name, double price, double prevClose,
                double volume, double amount = 0.0) {
    Quote q;
    q.code = code;
    q.name = name;
    q.price = price;
    q.prevClose = prevClose;
    q.open = prevClose;
    q.high = qMax(price, prevClose);
    q.low = qMin(price, prevClose);
    q.volume = volume;
    q.amount = amount;
    q.avg = volume > 0 ? amount / volume : prevClose;
    q.committee = 0.0;
    return q;
}

QVector<Quote> threeQuotes() {
    return {makeQuote("sh600000", "浦发银行", 10.2, 10.0, 8000, 81600.0),
            makeQuote("sz000001", "平安银行", 9.5, 10.0, 5000, 47500.0),
            makeQuote("sz300136", "信维通信", 10.5, 10.0, 1000, 10500.0)};
}

}  // namespace

class TestSorter : public QObject {
    Q_OBJECT
private slots:
    void sortableKeysCoverEverySortableColumn() {
        QStringList fromColumns;
        for (const QString& h : QuoteColumns::allHeaders()) {
            const QString k = QuoteColumns::sortKeyFor(h);
            if (!k.isEmpty()) fromColumns << k;
        }
        QCOMPARE(fromColumns, QuoteSort::sortableKeys());
        QVERIFY(QuoteSort::isValidKey("change_pct"));
        QVERIFY(!QuoteSort::isValidKey(""));
        QVERIFY(!QuoteSort::isValidKey("kline_visible"));
    }
    void sortsChangePctDescending() {
        QVector<Quote> q = threeQuotes();
        QuoteSort::sortQuotes(q, "change_pct", false);
        QCOMPARE(q.at(0).code, QString("sz300136"));  // +5.00%
        QCOMPARE(q.at(1).code, QString("sh600000"));  // +2.00%
        QCOMPARE(q.at(2).code, QString("sz000001"));  // -5.00%
    }
    void sortsChangePctAscending() {
        QVector<Quote> q = threeQuotes();
        QuoteSort::sortQuotes(q, "change_pct", true);
        QCOMPARE(q.at(0).code, QString("sz000001"));
        QCOMPARE(q.at(1).code, QString("sh600000"));
        QCOMPARE(q.at(2).code, QString("sz300136"));
    }
    void sortsByAmountAndAveragePrice() {
        QVector<Quote> q = threeQuotes();
        QuoteSort::sortQuotes(q, "amount", false);
        QCOMPARE(q.at(0).code, QString("sh600000"));  // 8.16 万
        QCOMPARE(q.at(1).code, QString("sz000001"));  // 4.75 万
        QCOMPARE(q.at(2).code, QString("sz300136"));  // 1.05 万
        QuoteSort::sortQuotes(q, "avg", true);
        QCOMPARE(q.at(0).code, QString("sz000001"));  // 9.50
        QCOMPARE(q.at(1).code, QString("sh600000"));  // 10.20
        QCOMPARE(q.at(2).code, QString("sz300136"));  // 10.50
    }
    void stableForEqualValues() {
        QVector<Quote> q{makeQuote("sh600000", "A", 10.0, 10.0, 1),
                         makeQuote("sz000001", "B", 10.0, 10.0, 2),
                         makeQuote("sz300136", "C", 10.0, 10.0, 3)};
        QuoteSort::sortQuotes(q, "change_pct", false);
        QCOMPARE(q.at(0).code, QString("sh600000"));
        QCOMPARE(q.at(1).code, QString("sz000001"));
        QCOMPARE(q.at(2).code, QString("sz300136"));
        QuoteSort::sortQuotes(q, "change_pct", true);  // 升序同样保持原序
        QCOMPARE(q.at(0).code, QString("sh600000"));
        QCOMPARE(q.at(2).code, QString("sz300136"));
    }
    void zeroValuesParticipateNaturally() {
        QVector<Quote> q{makeQuote("sh600000", "A", 10.0, 10.0, 8000),
                         makeQuote("sz000001", "B", 10.0, 10.0, 0)};
        QuoteSort::sortQuotes(q, "vol", false);
        QCOMPARE(q.at(0).code, QString("sh600000"));
        QCOMPARE(q.at(1).code, QString("sz000001"));
    }
    void textKeysSortLexically() {
        QVector<Quote> q{makeQuote("sh600001", "乙", 10.0, 10.0, 1),
                         makeQuote("sh600000", "甲", 10.0, 10.0, 1)};
        QuoteSort::sortQuotes(q, "code", true);
        QCOMPARE(q.at(0).code, QString("sh600000"));
        QCOMPARE(q.at(1).code, QString("sh600001"));
        QuoteSort::sortQuotes(q, "name", true);
        // QString::compare 按 UTF-16 码位比较（不做拼音）：乙 U+4E59 < 甲 U+7532
        QCOMPARE(q.at(0).name, QString("乙"));
        QCOMPARE(q.at(1).name, QString("甲"));
    }
    void unknownKeyKeepsOrder() {
        QVector<Quote> q = threeQuotes();
        QuoteSort::sortQuotes(q, "", false);
        QCOMPARE(q.at(0).code, QString("sh600000"));
        QuoteSort::sortQuotes(q, "nope", true);
        QCOMPARE(q.at(1).code, QString("sz000001"));
    }
    void emptyListIsSafe() {
        QVector<Quote> q;
        QuoteSort::sortQuotes(q, "price", false);
        QVERIFY(q.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestSorter)
#include "test_sorter.moc"
