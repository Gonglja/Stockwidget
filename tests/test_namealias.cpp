#include <QtTest>
#include <QJsonObject>
#include "data/NameAlias.h"
#include "data/QuoteParser.h"

namespace {
const char* kPufa =
    "var hq_str_sh600000=\"浦发银行,10.100,10.000,10.200,10.300,9.900,"
    "10.190,10.200,8000,81600.000,"
    "100,10.180,200,10.170,300,10.160,400,10.150,500,10.140,"
    "600,10.210,700,10.220,800,10.230,900,10.240,1000,10.250,"
    "2026-09-11,15:00:00,00\";\n";

QVector<Quote> pufa(const QuoteFormatOptions& opt = {}) {
    return QuoteParser::parseText(QString::fromUtf8(kPufa), opt);
}
}  // namespace

class TestNameAlias : public QObject {
    Q_OBJECT
private slots:
    void aliasOverridesQuoteName() {
        QVector<Quote> q = pufa();
        NameAlias::applyAliases(q, QJsonObject{{QStringLiteral("sh600000"),
                                                QStringLiteral("浦发(老仓)")}});
        QCOMPARE(q.at(0).name, QString("浦发(老仓)"));
    }
    void emptyAliasFallsBackToQuoteName() {
        QVector<Quote> q = pufa();
        NameAlias::applyAliases(q, QJsonObject{{QStringLiteral("sh600000"), QString()}});
        QCOMPARE(q.at(0).name, QString("浦发银行"));
    }
    void unknownCodeKeepsQuoteName() {
        QVector<Quote> q = pufa();
        NameAlias::applyAliases(q, QJsonObject{{QStringLiteral("sz000001"),
                                                QStringLiteral("平安银行(别名)")}});
        QCOMPARE(q.at(0).name, QString("浦发银行"));
    }
    void bareNumericKeyStillMatches() {
        QVector<Quote> q = pufa();
        NameAlias::applyAliases(q, QJsonObject{{QStringLiteral("600000"),
                                                QStringLiteral("老仓")}});
        QCOMPARE(q.at(0).name, QString("老仓"));
    }
    void aliasIsNotTruncatedByParser() {
        QuoteFormatOptions opt;
        opt.nameLength = 2;
        QVector<Quote> q = pufa(opt);
        QCOMPARE(q.at(0).name, QString("浦发"));  // 截断只作用于行情名称
        NameAlias::applyAliases(q, QJsonObject{{QStringLiteral("sh600000"),
                                                QStringLiteral("浦发(老仓)")}});
        QCOMPARE(q.at(0).name, QString("浦发(老仓)"));
    }
    void aliasForReturnsEmptyWhenMissing() {
        QVERIFY(NameAlias::aliasFor({}, QStringLiteral("sh600000")).isEmpty());
        QVERIFY(NameAlias::aliasFor(QJsonObject{{QStringLiteral("sh600000"), QStringLiteral("x")}},
                                    QStringLiteral("sz000001"))
                    .isEmpty());
    }
    void emptyMapIsNoOp() {
        QVector<Quote> q = pufa();
        NameAlias::applyAliases(q, {});
        QCOMPARE(q.at(0).name, QString("浦发银行"));
    }
};

QTEST_GUILESS_MAIN(TestNameAlias)
#include "test_namealias.moc"
