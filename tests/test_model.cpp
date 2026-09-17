#include <QtTest>
#include <QAbstractItemModelTester>
#include <QSignalSpy>
#include "ui/QuoteModel.h"

static Quote makeQuote(double price, double prevClose) {
    Quote q;
    q.code = "sh600000";
    q.name = "浦发";
    q.price = price;
    q.prevClose = prevClose;
    q.priceText = QString::number(price, 'f', 2);
    q.changeText = QString::number(price - prevClose, 'f', 2);
    q.deltaSign = (price > prevClose) - (price < prevClose);
    q.kOpen = 10;
    q.kClose = price;
    q.kHigh = 11;
    q.kLow = 9;
    q.kPrev = prevClose;
    return q;
}

class TestModel : public QObject {
    Q_OBJECT
private slots:
    void respectsColumnConfig() {
        QuoteModel m;
        QJsonObject cfg;
        cfg["price_visible"] = true;
        cfg["name_visible"] = true;
        m.setColumns(QuoteColumns::activeColumns(cfg));
        QAbstractItemModelTester tester(&m);
        m.setQuotes({makeQuote(10.5, 10.0)});
        QCOMPARE(m.rowCount(), 1);
        QCOMPARE(m.columnCount(), 2);
        QCOMPARE(m.headerData(0, Qt::Horizontal).toString(), QString("名称"));
        QCOMPARE(m.headerData(1, Qt::Horizontal).toString(), QString("现价"));
    }
    void emitsDataChangedOnlyForChangedCells() {
        QuoteModel m;
        QJsonObject cfg;
        cfg["price_visible"] = true;
        m.setColumns(QuoteColumns::activeColumns(cfg));
        m.setQuotes({makeQuote(10.5, 10.0)});
        QSignalSpy spy(&m, &QAbstractItemModel::dataChanged);
        m.setQuotes({makeQuote(10.5, 10.0)});
        QCOMPARE(spy.count(), 0);
        m.setQuotes({makeQuote(10.6, 10.0)});
        QCOMPARE(spy.count(), 1);
        const auto args = spy.takeFirst();
        QCOMPARE(args.at(0).toModelIndex().column(), 0);
    }
    void exposesKLinePayload() {
        QuoteModel m;
        QJsonObject cfg;
        cfg["kline_visible"] = true;
        m.setColumns(QuoteColumns::activeColumns(cfg));
        m.setQuotes({makeQuote(10.5, 10.0)});
        const QVariant v = m.data(m.index(0, 0), QuoteModel::KLineRole);
        QCOMPARE(v.toList().size(), 5);
    }
    void mapsHeadersToSortKeys() {
        QCOMPARE(QuoteColumns::sortKeyFor("代码"), QString("code"));
        QCOMPARE(QuoteColumns::sortKeyFor("名称"), QString("name"));
        QCOMPARE(QuoteColumns::sortKeyFor("现价"), QString("price"));
        QCOMPARE(QuoteColumns::sortKeyFor("涨跌值"), QString("change"));
        QCOMPARE(QuoteColumns::sortKeyFor("涨跌幅"), QString("change_pct"));
        QCOMPARE(QuoteColumns::sortKeyFor("买一"), QString("buy1"));
        QCOMPARE(QuoteColumns::sortKeyFor("卖一"), QString("sell1"));
        QCOMPARE(QuoteColumns::sortKeyFor("委比"), QString("commi"));
        QCOMPARE(QuoteColumns::sortKeyFor("成交量"), QString("vol"));
        QCOMPARE(QuoteColumns::sortKeyFor("成交额"), QString("amount"));
        QCOMPARE(QuoteColumns::sortKeyFor("均价"), QString("avg"));
        QVERIFY(QuoteColumns::sortKeyFor("K线").isEmpty());  // K 线不可排序
        QVERIFY(QuoteColumns::sortKeyFor("不存在").isEmpty());
    }
    void colorsBySign() {
        QuoteModel m;
        QJsonObject cfg;
        cfg["price_visible"] = true;
        m.setColumns(QuoteColumns::activeColumns(cfg));
        m.setColorScheme(true, Qt::white);
        m.setQuotes({makeQuote(10.6, 10.0)});
        QCOMPARE(m.data(m.index(0, 0), Qt::ForegroundRole).value<QColor>(),
                 QColor(0xdd, 0x21, 0x00));
    }
};

QTEST_GUILESS_MAIN(TestModel)
#include "test_model.moc"
