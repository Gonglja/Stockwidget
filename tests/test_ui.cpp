#include <QtTest>
#include <QApplication>
#include <QContextMenuEvent>
#include <QJsonArray>
#include <QJsonObject>
#include <QKeySequenceEdit>
#include <QSlider>
#include <QTabWidget>
#include <QTableView>
#include <QTime>
#include "ui/FloatWindow.h"
#include "ui/SettingsDialog.h"
#include <QComboBox>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QTimer>
#include "data/QuoteParser.h"
#include "data/QuoteSort.h"
#include "ui/QuoteModel.h"

// Subclass so the context-menu slot doesn't open a modal menu during tests.
class ProbeWindow : public FloatWindow {
public:
    using FloatWindow::FloatWindow;
    int ctxCount = 0;
    void contextMenuEvent(QContextMenuEvent*) override { ++ctxCount; }
    QMenu* menuAt(const QPoint& globalPos) { return buildContextMenu(globalPos); }
    void pushQuotes(const QVector<Quote>& quotes) {
        QMetaObject::invokeMethod(this, "onQuotesReady", Qt::DirectConnection,
                                  Q_ARG(QVector<Quote>, quotes));
    }
};

// 两行真实新浪格式行情：sh600000 +2.00%，sz000001 -5.00%（行序 = 自选顺序）
static const char* kTwoLines =
    "var hq_str_sh600000=\"浦发银行,10.100,10.000,10.200,10.300,9.900,"
    "10.190,10.200,8000,81600.000,"
    "100,10.180,200,10.170,300,10.160,400,10.150,500,10.140,"
    "600,10.210,700,10.220,800,10.230,900,10.240,1000,10.250,"
    "2026-09-11,15:00:00,00\";\n"
    "var hq_str_sz000001=\"平安银行,10.000,10.000,9.500,10.100,9.400,"
    "9.490,9.500,5000,47500.000,"
    "100,9.480,200,9.470,300,9.460,400,9.450,500,9.440,"
    "600,9.510,700,9.520,800,9.530,900,9.540,1000,9.550,"
    "2026-09-11,15:00:00,00\";\n";

static QVector<Quote> twoQuotes() {
    return QuoteParser::parseText(QString::fromUtf8(kTwoLines), QuoteFormatOptions{});
}

static int columnOf(QAbstractItemModel* model, const QString& header) {
    for (int c = 0; c < model->columnCount(); ++c)
        if (model->headerData(c, Qt::Horizontal).toString() == header) return c;
    return -1;
}

static QString cellText(QAbstractItemModel* model, int row, int col) {
    return model->data(model->index(row, col), Qt::DisplayRole).toString();
}

static QJsonObject baseConfig() {
    QJsonObject cfg;
    cfg["checked_codes"] = QJsonArray{"sh600000"};
    cfg["codes"] = QJsonArray{"sh600000"};
    cfg["name_visible"] = true;
    cfg["price_visible"] = true;
    cfg["change_pct_visible"] = true;
    cfg["header_visible"] = false;
    // 测试保持离线/快速：默认不在请求时段，FloatWindow 不发起网络请求。
    cfg["fetch_mode"] = "custom";
    cfg["fetch_start"] = "00:00";
    cfg["fetch_end"] = "00:00";
    return cfg;
}

class TestUi : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { qRegisterMetaType<QVector<Quote>>("QVector<Quote>"); }

    void singleClickOnViewportHides() {
        ProbeWindow w(baseConfig());
        w.show();
        QTest::qWait(200);
        QVERIFY(w.isVisible());

        auto* table = w.findChild<QTableView*>();
        QVERIFY(table);
        QTest::mouseClick(table->viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(5, 5));
        QTest::qWait(50);
        QVERIFY2(!w.isVisible(), "single click on table viewport did not hide the window");
    }

    void doubleClickOnViewportHides() {
        ProbeWindow w(baseConfig());
        w.show();
        QTest::qWait(200);
        QVERIFY(w.isVisible());

        auto* table = w.findChild<QTableView*>();
        QVERIFY(table);
        QTest::mouseDClick(table->viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(5, 5));
        QTest::qWait(50);
        QVERIFY2(!w.isVisible(), "double-click on table viewport did not hide the window");
    }

    void contextMenuOnViewportReachesWindow() {
        ProbeWindow w(baseConfig());
        w.show();
        QTest::qWait(200);

        auto* table = w.findChild<QTableView*>();
        QVERIFY(table);
        QWidget* vp = table->viewport();
        const QPoint local(5, 5);
        QContextMenuEvent ev(QContextMenuEvent::Mouse, local, vp->mapToGlobal(local));
        QApplication::sendEvent(vp, &ev);
        QTest::qWait(50);
        QCOMPARE(w.ctxCount, 1);
    }

    void contextMenuOnWindowReachesWindow() {
        ProbeWindow w(baseConfig());
        w.show();
        QTest::qWait(200);

        const QPoint local(2, 2);
        QContextMenuEvent ev(QContextMenuEvent::Mouse, local, w.mapToGlobal(local));
        QApplication::sendEvent(&w, &ev);
        QTest::qWait(50);
        QCOMPARE(w.ctxCount, 1);
    }

    void alwaysShowStaysVisible() {
        ProbeWindow w(baseConfig());
        w.show();
        QTest::qWait(150);
        QVERIFY(w.isVisible());
    }

    void customShowWindowInsideStaysVisible() {
        QJsonObject cfg = baseConfig();
        cfg["show_mode"] = "custom";
        cfg["show_start"] = "00:00";
        cfg["show_end"] = "23:59";
        ProbeWindow w(cfg);
        w.show();
        QTest::qWait(150);
        QVERIFY(w.isVisible());
    }

    void customShowWindowOutsideHidesImmediately() {
        const QTime now = QTime::currentTime();
        const bool atMidnight = now.hour() == 0 && now.minute() == 0;
        QJsonObject cfg = baseConfig();
        cfg["show_mode"] = "custom";
        cfg["show_start"] = "00:00";
        cfg["show_end"] = "00:00";  // 仅 00:00 这一分钟在窗口内
        ProbeWindow w(cfg);
        w.show();
        QTest::qWait(50);
        if (!atMidnight) QVERIFY(!w.isVisible());
    }

    void locateForcesVisibleOutsideSchedule() {
        const QTime now = QTime::currentTime();
        const bool atMidnight = now.hour() == 0 && now.minute() == 0;
        QJsonObject cfg = baseConfig();
        cfg["show_mode"] = "custom";
        cfg["show_start"] = "00:00";
        cfg["show_end"] = "00:00";
        ProbeWindow w(cfg);
        w.show();
        QTest::qWait(50);
        if (!atMidnight) QVERIFY(!w.isVisible());  // 构造后按时段自动隐藏
        w.locate();
        QTest::qWait(50);
        if (!atMidnight) QVERIFY(w.isVisible());   // 定位可强制显示
    }

    void transparentBackgroundStillHitTestable() {
        // 回归：背景 alpha=0 + 低整体不透明度时，必须仍可命中。
        // Windows 分层窗口对 alpha=0 的像素会鼠标穿透，因此背景 alpha 需钳制到 >=1。
        QJsonObject cfg = baseConfig();
        cfg["bg"] = QJsonObject{{"r", 0}, {"g", 0}, {"b", 0}, {"a", 0}};
        cfg["opacity_pct"] = 24;
        ProbeWindow w(cfg);
        w.show();
        QTest::qWait(150);
        QCOMPARE(w.windowOpacity(), 1.0);  // 不得再用 setWindowOpacity
        const QImage img = w.grab().toImage();
        QVERIFY(!img.isNull());
        const QColor bg = img.pixelColor(w.width() / 2, 4);  // 顶部内边距处（无文字）
        QVERIFY2(bg.alpha() >= 1, "background alpha must be >= 1 to stay hit-testable");
    }

    void sortFromConfigAppliedOnQuotesReady() {
        QJsonObject cfg = baseConfig();
        cfg["sort_key"] = "change_pct";
        cfg["sort_asc"] = false;
        ProbeWindow w(cfg);
        w.show();
        QTest::qWait(200);

        auto* table = w.findChild<QTableView*>();
        QVERIFY(table);
        w.pushQuotes(twoQuotes());

        const int col = columnOf(table->model(), "涨跌幅");
        QVERIFY(col >= 0);
        QCOMPARE(cellText(table->model(), 0, col), QString("+2.00%"));
        QCOMPARE(cellText(table->model(), 1, col), QString("-5.00%"));

        auto* header = table->horizontalHeader();
        QVERIFY(header->isSortIndicatorShown());
        QCOMPARE(header->sortIndicatorSection(), col);
        QCOMPARE(header->sortIndicatorOrder(), Qt::DescendingOrder);
    }

    void headerClickCyclesSortDescAscOff() {
        QJsonObject cfg = baseConfig();
        cfg["header_visible"] = true;  // 表头不可点时点击会落到视图（老行为：隐藏窗口）
        ProbeWindow w(cfg);
        w.show();
        QTest::qWait(200);

        auto* table = w.findChild<QTableView*>();
        QVERIFY(table);
        w.pushQuotes(twoQuotes());

        auto* header = table->horizontalHeader();
        const int col = columnOf(table->model(), "涨跌幅");
        QVERIFY(col >= 0);
        // 每次都按当前列宽重算点击位置：排序指示器出现/数据变化都会让列宽微调
        auto clickHeader = [header, col] {
            const QPoint p(header->sectionViewportPosition(col) + header->sectionSize(col) / 2,
                           header->height() / 2);
            QTest::mouseClick(header, Qt::LeftButton, Qt::NoModifier, p);
            QTest::qWait(50);
        };

        clickHeader();  // 第一次 → 降序
        QCOMPARE(w.currentConfig().value("sort_key").toString(), QString("change_pct"));
        QCOMPARE(w.currentConfig().value("sort_asc").toBool(), false);
        QVERIFY(header->isSortIndicatorShown());
        QCOMPARE(header->sortIndicatorSection(), col);
        QCOMPARE(cellText(table->model(), 0, col), QString("+2.00%"));
        QVERIFY2(w.isVisible(), "clicking the header must not hide the window");

        clickHeader();  // 第二次 → 升序
        QCOMPARE(w.currentConfig().value("sort_key").toString(), QString("change_pct"));
        QCOMPARE(w.currentConfig().value("sort_asc").toBool(), true);
        QCOMPARE(header->sortIndicatorOrder(), Qt::AscendingOrder);
        QCOMPARE(cellText(table->model(), 0, col), QString("-5.00%"));
        QVERIFY(w.isVisible());

        clickHeader();  // 第三次 → 取消排序，回到自选顺序
        QCOMPARE(w.currentConfig().value("sort_key").toString(), QString());
        QVERIFY(!header->isSortIndicatorShown());
        QCOMPARE(cellText(table->model(), 0, col), QString("+2.00%"));
        QVERIFY(w.isVisible());
    }

    void headerClickOnKLineColumnIsIgnored() {
        QJsonObject cfg = baseConfig();
        cfg["kline_visible"] = true;
        cfg["header_visible"] = true;
        ProbeWindow w(cfg);
        w.show();
        QTest::qWait(200);

        auto* table = w.findChild<QTableView*>();
        QVERIFY(table);
        w.pushQuotes(twoQuotes());

        auto* header = table->horizontalHeader();
        const int col = columnOf(table->model(), "K线");
        QVERIFY(col >= 0);
        const QPoint pos(header->sectionViewportPosition(col) + header->sectionSize(col) / 2,
                         header->height() / 2);
        QTest::mouseClick(header, Qt::LeftButton, Qt::NoModifier, pos);
        QTest::qWait(50);

        QCOMPARE(w.currentConfig().value("sort_key").toString(), QString());
        QVERIFY(!header->isSortIndicatorShown());
        QVERIFY(w.isVisible());
    }

    void settingsDialogBuildsAllTabs() {
        ProbeWindow w(baseConfig());
        w.show();
        QTest::qWait(150);

        SettingsDialog dlg(&w, &w);
        dlg.show();
        QTest::qWait(50);
        QVERIFY(dlg.isVisible());

        auto* tabs = dlg.findChild<QTabWidget*>();
        QVERIFY(tabs);
        QCOMPARE(tabs->count(), 4);
        for (int i = 0; i < tabs->count(); ++i) {
            tabs->setCurrentIndex(i);
            QTest::qWait(30);
        }
        // 常规页应含快捷键编辑器；外观页应含点击区域滑块
        QVERIFY(dlg.findChild<QKeySequenceEdit*>());
        QVERIFY(dlg.findChild<QSlider*>());
        dlg.close();
        QTest::qWait(50);
    }
};

QTEST_MAIN(TestUi)
#include "test_ui.moc"
