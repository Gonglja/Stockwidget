#include <QtTest>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include "app/ConfigStore.h"
#include "data/QuoteColumns.h"

class TestConfig : public QObject {
    Q_OBJECT
private slots:
    void init() {
        m_tmp = new QTemporaryDir();
        QVERIFY(m_tmp->isValid());
        qputenv("SW_CONFIG_DIR", m_tmp->path().toUtf8());
    }
    void cleanup() {
        delete m_tmp;
        m_tmp = nullptr;
        qunsetenv("SW_CONFIG_DIR");
    }

    void migratesLegacyFlags() {
        QJsonObject raw;
        raw["flags"] = QJsonArray{false, false, true, false, true, false,
                                  false, false, false, false, false, false};
        raw["b1s1_price"] = true;
        raw["visible_codes"] = QJsonArray{"sh600000"};
        const QJsonObject out = ConfigStore::normalize(raw);
        QCOMPARE(out.value("price_visible").toBool(), true);
        QCOMPARE(out.value("change_pct_visible").toBool(), true);
        QCOMPARE(out.value("b1s1_display").toString(), QString("price"));
        QCOMPARE(out.value("checked_codes").toArray().first().toString(), QString("sh600000"));
        QVERIFY(!out.contains("flags"));
    }
    void roundTripsToDisk() {
        QJsonObject cfg;
        cfg["refresh_seconds"] = 5;
        cfg["codes"] = QJsonArray{"sh600000"};
        QVERIFY(ConfigStore::save(cfg));
        const QJsonObject back = ConfigStore::load();
        QCOMPARE(back.value("refresh_seconds").toInt(), 5);
        QCOMPARE(back.value("codes").toArray().first().toString(), QString("sh600000"));
    }
    void corruptFileFallsBackToEmpty() {
        const QString path = ConfigStore::configFilePath();
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("{ not json");
        f.close();
        QVERIFY(ConfigStore::load().isEmpty());
    }
    void columnKeyMapping() {
        QCOMPARE(QuoteColumns::configKeyFor("现价"), QString("price_visible"));
        QCOMPARE(QuoteColumns::configKeyFor("卖一"), QString("b1s1_visible"));
        QCOMPARE(QuoteColumns::allHeaders().size(), 12);
    }
    void normalizesNameMapKeys() {
        QJsonObject raw;
        raw["name_map"] = QJsonObject{{"600000", "浦发(老仓)"},
                                      {"sz000001", "  "},
                                      {"bad", "x"}};
        const QJsonObject out = ConfigStore::normalize(raw);
        const QJsonObject map = out.value("name_map").toObject();
        QCOMPARE(map.size(), 1);
        QCOMPARE(map.value("sh600000").toString(), QString("浦发(老仓)"));
    }
    void dropsNonObjectNameMap() {
        QJsonObject raw;
        raw["name_map"] = QJsonArray{"sh600000"};
        QVERIFY(!ConfigStore::normalize(raw).contains("name_map"));
    }
    void resetsUnknownSortKeyButKeepsOrderFlag() {
        QJsonObject raw;
        raw["sort_key"] = "kline_visible";
        raw["sort_asc"] = true;
        QJsonObject out = ConfigStore::normalize(raw);
        QCOMPARE(out.value("sort_key").toString(), QString());
        QCOMPARE(out.value("sort_asc").toBool(), true);
        raw["sort_key"] = "change_pct";
        out = ConfigStore::normalize(raw);
        QCOMPARE(out.value("sort_key").toString(), QString("change_pct"));
    }
    void legacyConfigKeepsOldBehavior() {
        const QJsonObject out = ConfigStore::normalize(QJsonObject{{"refresh_seconds", 5}});
        QCOMPARE(out.value("sort_key").toString(), QString());
        QCOMPARE(out.value("sort_asc").toBool(), false);
        QCOMPARE(out.value("name_map").toObject().size(), 0);
    }

private:
    QTemporaryDir* m_tmp = nullptr;
};

QTEST_GUILESS_MAIN(TestConfig)
#include "test_config.moc"
