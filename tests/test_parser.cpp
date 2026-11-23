#include <QtTest>
#include "data/QuoteParser.h"

static const char* kPufa =
    "var hq_str_sh600000=\"浦发银行,10.100,10.000,10.200,10.300,9.900,"
    "10.190,10.200,8000,81600.000,"
    "100,10.180,200,10.170,300,10.160,400,10.150,500,10.140,"
    "600,10.210,700,10.220,800,10.230,900,10.240,1000,10.250,"
    "2026-09-11,15:00:00,00\";\n";

static const char* kTencent =
    "var hq_str_rt_hk00700=\"TENCENT,腾讯控股,335.000,344.000,341.400,333.000,338.000,"
    "-6.000,-1.744,338.00000,338.20001,6343939839,18883190,0.000,0.000,631.641,333.000,"
    "2026/11/23,11:59:57,30|3,N|Y|Y,0.000|0.000|0.000,0|||0.000|0.000|0.000, |0,Y\";\n";

class TestParser : public QObject {
    Q_OBJECT
private slots:
    void parsesContinuousTrading() {
        QuoteFormatOptions opt;
        opt.shortCode = false;
        opt.nameLength = 2;
        const auto q = QuoteParser::parseText(QString::fromUtf8(kPufa), opt);
        QCOMPARE(q.size(), 1);
        const Quote& s = q.first();
        QCOMPARE(s.code, QString("sh600000"));
        QCOMPARE(s.name, QString("浦发"));
        QCOMPARE(s.priceText, QString("10.20 "));
        QCOMPARE(s.changeText, QString("+0.20"));
        QCOMPARE(s.changePctText, QString("+2.00%"));
        QCOMPARE(s.deltaSign, 1);
        QCOMPARE(s.b1Text, QString("1 "));
        QCOMPARE(s.s1Text, QString(">6"));
        QCOMPARE(s.b1Sign, 1);
        QCOMPARE(s.s1Sign, -1);
        QCOMPARE(s.committeeText, QString("-45.45%"));
        QCOMPARE(s.volumeText, QString("8000"));
        QCOMPARE(s.amountText, QString("8.16万"));
        QCOMPARE(s.avgText, QString("10.20"));
    }
    void parsesHongKongQuote() {
        QuoteFormatOptions opt;
        opt.nameLength = 0;
        const auto q = QuoteParser::parseText(QString::fromUtf8(kTencent), opt);
        QCOMPARE(q.size(), 1);
        const Quote& s = q.first();
        QCOMPARE(s.code, QString("hk00700"));
        QCOMPARE(s.name, QString("腾讯控股"));
        QCOMPARE(s.fullName, QString("腾讯控股"));
        QCOMPARE(s.open, 335.0);
        QCOMPARE(s.prevClose, 344.0);
        QCOMPARE(s.price, 338.0);
        QCOMPARE(s.high, 341.4);
        QCOMPARE(s.low, 333.0);
        QCOMPARE(s.buy1, 338.0);
        QCOMPARE(s.sell1, 338.20001);
        QCOMPARE(s.volume, 18883190.0);
        QCOMPARE(s.amount, 6343939839.0);
        QCOMPARE(s.priceText, QString("338.000 "));
        QCOMPARE(s.changeText, QString("-6.000"));
        QCOMPARE(s.changePctText, QString("-1.74%"));
        QCOMPARE(s.volumeText, QString("1888.32万"));
        QCOMPARE(s.amountText, QString("63.44亿"));
        QCOMPARE(s.avgText, QString("335.957"));
        QCOMPARE(s.b1Text, QString("338.000<"));
        QCOMPARE(s.s1Text, QString(">338.200"));
    }
    void hongKongShortCodeAndTruncation() {
        QuoteFormatOptions opt;
        opt.shortCode = true;
        opt.nameLength = 2;
        const auto q = QuoteParser::parseText(QString::fromUtf8(kTencent), opt);
        QCOMPARE(q.first().code, QString("00700"));
        QCOMPARE(q.first().name, QString("腾讯"));
        QCOMPARE(q.first().fullName, QString("腾讯控股"));
    }
    void shortCodeAndFullName() {
        QuoteFormatOptions opt;
        opt.shortCode = true;
        opt.nameLength = 0;
        const auto q = QuoteParser::parseText(QString::fromUtf8(kPufa), opt);
        QCOMPARE(q.first().code, QString("600000"));
        QCOMPARE(q.first().name, QString("浦发银行"));
    }
    void etfUsesThreeDecimals() {
        const QString line = QStringLiteral(
            "var hq_str_sh510300=\"沪深300,1.200,1.100,1.234,1.300,1.000,"
            "1.230,1.240,5000,6170.000,"
            "100,1.220,200,1.210,300,1.200,400,1.190,500,1.180,"
            "600,1.250,700,1.260,800,1.270,900,1.280,1000,1.290,"
            "2026-09-11,15:00:00,00\";\n");
        const auto q = QuoteParser::parseText(line, QuoteFormatOptions{});
        QCOMPARE(q.first().priceText, QString("1.234 "));
        QCOMPARE(q.first().changeText, QString("+0.134"));
    }
    void callAuctionShowsPairedAndUnpaired() {
        const QString line = QStringLiteral(
            "var hq_str_sh600000=\"浦发银行,10.000,10.000,10.000,10.000,10.000,"
            "10.000,10.000,1000,10000.000,"
            "100,10.000,700,9.990,300,9.980,400,9.970,500,9.960,"
            "600,10.000,0,10.010,800,10.020,900,10.030,1000,10.040,"
            "2026-09-11,09:20:00,00\";\n");
        const auto q = QuoteParser::parseText(line, QuoteFormatOptions{});
        QCOMPARE(q.first().b1Text, QString("6"));   // paired 600/100 = 6
        QCOMPARE(q.first().s1Text, QString("+7"));  // unpaired 700/100 = 7
        QCOMPARE(q.first().b1Sign, 1);              // 买方优势
    }
    void skipsDirtyAndEmptyLines() {
        const QString text = QStringLiteral(
            "garbage line\n"
            "var hq_str_sh600000=\"\";\n"
            "var hq_str_sh600001=\"only,three,fields\";\n");
        QVERIFY(QuoteParser::parseText(text, QuoteFormatOptions{}).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestParser)
#include "test_parser.moc"
