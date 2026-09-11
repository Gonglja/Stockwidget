#include <QtTest>
#include "data/StockSuggestSource.h"

// 新浪 suggest3 实际格式：
// var suggestvalue="带前缀,类型,代码,带前缀,名称,,名称,...;..."
class TestSuggest : public QObject {
    Q_OBJECT
private slots:
    void parsesRealFormat() {
        const QString text = QStringLiteral(
            "var suggestvalue=\"sh600030,11,600030,sh600030,中信证券,,中信证券,99,1,ESG,,;"
            "sh600036,11,600036,sh600036,招商银行,,招商银行,99,1,ESG,,;"
            "06000,31,06000,06000,IFC DEV N2904,,IFC DEV N2904,99,1,,,\";");
        const auto items = SuggestParser::parseSuggest(text);
        QCOMPARE(items.size(), 2);  // 06000 非沪深京 6 位，被过滤
        QCOMPARE(items.at(0).code, QString("sh600030"));
        QCOMPARE(items.at(0).name, QString("中信证券"));
        QCOMPARE(items.at(1).code, QString("sh600036"));
        QCOMPARE(items.at(1).name, QString("招商银行"));
    }
    void keepsShenzhenAndBeijing() {
        const QString text = QStringLiteral(
            "var suggestvalue=\"sz000001,11,000001,sz000001,平安银行,,平安银行,99,1,ESG,,;"
            "bj830799,11,830799,bj830799,北交所股,,北交所股,99,1,,,\";");
        const auto items = SuggestParser::parseSuggest(text);
        QCOMPARE(items.size(), 2);
        QCOMPARE(items.at(0).code, QString("sz000001"));
        QCOMPARE(items.at(0).name, QString("平安银行"));
        QCOMPARE(items.at(1).code, QString("bj830799"));
    }
    void handlesEmptyAndInvalid() {
        QVERIFY(SuggestParser::parseSuggest(QString()).isEmpty());
        QVERIFY(SuggestParser::parseSuggest("garbage").isEmpty());
        QVERIFY(SuggestParser::parseSuggest("var suggestvalue=\"\";").isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestSuggest)
#include "test_suggest.moc"
