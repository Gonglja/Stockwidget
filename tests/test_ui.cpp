#include <QtTest>
#include <QApplication>
#include <QContextMenuEvent>
#include <QJsonArray>
#include <QJsonObject>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QSlider>
#include <QTabWidget>
#include <QTableView>
#include <QTime>
#include "ui/FloatWindow.h"
#include "ui/SettingsDialog.h"
#include <QAction>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QSignalSpy>
#include <QTableWidget>
#include <QTimer>
#include "data/QuoteParser.h"
#include "data/QuoteSort.h"
#include "ui/CustomConfigDialog.h"
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

    void setAliasUpdatesModelImmediately() {
        ProbeWindow w(baseConfig());
        w.show();
        QTest::qWait(200);

        auto* table = w.findChild<QTableView*>();
        QVERIFY(table);
        w.pushQuotes(twoQuotes());

        const int col = columnOf(table->model(), "名称");
        QVERIFY(col >= 0);
        QCOMPARE(cellText(table->model(), 0, col), QString("浦发银行"));

        w.setAlias("600000", "浦发(老仓)");
        QCOMPARE(w.currentConfig().value("name_map").toObject().value("sh600000").toString(),
                 QString("浦发(老仓)"));
        QCOMPARE(cellText(table->model(), 0, col), QString("浦发(老仓)"));

        w.setAlias("600000", "   ");  // 留空 = 恢复行情名称
        QCOMPARE(w.currentConfig().value("name_map").toObject().size(), 0);
        QCOMPARE(cellText(table->model(), 0, col), QString("浦发银行"));
    }

    void quoteNameForReturnsRawNameOnly() {
        ProbeWindow w(baseConfig());
        w.show();
        QTest::qWait(150);
        QVERIFY2(w.quoteNameFor("sh600000").isEmpty(), "行情未到达时不应有名称");

        w.pushQuotes(twoQuotes());
        QTest::qWait(30);
        QCOMPARE(w.quoteNameFor("sh600000"), QString("浦发银行"));
        QCOMPARE(w.quoteNameFor("600000"), QString("浦发银行"));  // 不带前缀也可查
        QVERIFY(w.quoteNameFor("sh601318").isEmpty());            // 不在行情里
        QVERIFY(w.quoteNameFor("bad").isEmpty());                 // 非法代码
    }

    void quotesReadyEmitsQuotesUpdated() {
        ProbeWindow w(baseConfig());
        w.show();
        QTest::qWait(150);
        QSignalSpy spy(&w, &FloatWindow::quotesUpdated);
        w.pushQuotes(twoQuotes());
        QTest::qWait(30);
        QCOMPARE(spy.count(), 1);
    }

    void infoLabelShowsNonTradingHintOutsideFetchWindow() {
        ProbeWindow w(baseConfig());  // fetch_mode=custom 00:00–00:00 ⇒ 非请求时段
        w.show();
        QTest::qWait(150);

        auto* info = w.findChild<QLabel*>("infoLabel");
        QVERIFY(info);
        QVERIFY2(info->isVisible(), "非请求时段且从无数据时应显示提示");
        QCOMPARE(info->text(), QString("非交易时段"));

        w.pushQuotes(twoQuotes());
        QTest::qWait(30);
        QVERIFY2(!info->isVisible(), "拿到数据后应撤掉提示，改为显示数据");
    }

    void infoLabelSilentInsideFetchWindow() {
        QJsonObject cfg = baseConfig();
        cfg["fetch_start"] = "00:00";
        cfg["fetch_end"] = "23:59";  // custom 全时段 ⇒ 处于请求时段（本用例不发真实请求断言）
        ProbeWindow w(cfg);
        w.show();
        QTest::qWait(100);

        auto* info = w.findChild<QLabel*>("infoLabel");
        QVERIFY(info);
        QVERIFY2(!info->isVisible(), "请求时段内首次数据未到时不应误报「非交易时段」");
    }

    void contextMenuOnRowOffersAliasEdit() {
        ProbeWindow w(baseConfig());
        w.show();
        QTest::qWait(200);

        auto* table = w.findChild<QTableView*>();
        QVERIFY(table);
        w.pushQuotes(twoQuotes());

        QMenu* menu = w.menuAt(table->viewport()->mapToGlobal(QPoint(5, 5)));
        QVERIFY(menu);
        QVERIFY(!menu->actions().isEmpty());
        QCOMPARE(menu->actions().first()->text(), QString("自定义名称…"));
        menu->deleteLater();

        QMenu* blank = w.menuAt(w.mapToGlobal(QPoint(2, w.height() - 2)));
        QVERIFY(blank);
        QVERIFY(!blank->actions().isEmpty());
        QVERIFY(blank->actions().first()->text() != QString("自定义名称…"));
        blank->deleteLater();
    }

    void settingsNameLengthComboWritesConfig() {
        ProbeWindow w(baseConfig());
        w.show();
        QTest::qWait(150);

        SettingsDialog dlg(&w, &w);
        dlg.show();
        QTest::qWait(50);
        dlg.findChild<QTabWidget*>()->setCurrentIndex(1);  // 显示数据页
        QTest::qWait(50);

        auto* combo = dlg.findChild<QComboBox*>("nameLengthCombo");
        QVERIFY(combo);
        QCOMPARE(combo->count(), 5);  // 全称 / 1~4 字
        combo->setCurrentIndex(combo->findData(2));
        QTest::qWait(30);
        QCOMPARE(w.currentConfig().value("name_length").toInt(), 2);
        combo->setCurrentIndex(combo->findData(0));
        QTest::qWait(30);
        QCOMPARE(w.currentConfig().value("name_length").toInt(), 0);
        dlg.close();
    }

    void settingsListShowsAliasAndApplyCodeEditWritesMap() {
        ProbeWindow w(baseConfig());
        w.show();
        QTest::qWait(150);

        SettingsDialog dlg(&w, &w);
        dlg.show();
        QTest::qWait(50);

        auto* list = dlg.findChild<QListWidget*>("codeList");
        QVERIFY(list);
        QCOMPARE(list->count(), 1);
        QCOMPARE(list->item(0)->text(), QString("sh600000"));

        QVERIFY(dlg.applyCodeEdit("600000", "浦发(老仓)"));
        QTest::qWait(30);
        QCOMPARE(list->item(0)->text(), QString("sh600000  浦发(老仓)"));
        QCOMPARE(w.currentConfig().value("name_map").toObject().value("sh600000").toString(),
                 QString("浦发(老仓)"));

        QVERIFY(dlg.applyCodeEdit("600000", ""));
        QTest::qWait(30);
        QCOMPARE(list->item(0)->text(), QString("sh600000"));
        QCOMPARE(w.currentConfig().value("name_map").toObject().size(), 0);

        QVERIFY(!dlg.applyCodeEdit("not-a-code", "x"));  // 非法代码被拒
        dlg.close();
    }

    void doubleClickListItemEditsAlias() {
        ProbeWindow w(baseConfig());
        w.show();
        QTest::qWait(150);

        SettingsDialog dlg(&w, &w);
        dlg.show();
        QTest::qWait(50);

        auto* list = dlg.findChild<QListWidget*>("codeList");
        QVERIFY(list);
        QVERIFY(list->count() == 1);

        bool sawDialog = false;
        // context object = &dlg：用例提前结束时定时器自动取消，避免 lambda 访问已销毁对象
        QTimer::singleShot(200, &dlg, [&sawDialog, &dlg] {
            QWidget* modal = QApplication::activeModalWidget();
            if (!modal) return;
            sawDialog = modal->objectName() == QString("codeEditDialog");
            if (auto* alias = modal->findChild<QLineEdit*>("aliasEdit"))
                alias->setText(QStringLiteral("浦发(老仓)"));
            if (auto* box = modal->findChild<QDialogButtonBox*>())
                if (auto* ok = box->button(QDialogButtonBox::Ok)) ok->click();
        });
        // QAbstractItemView 只在 pressedIndex 匹配时才发 doubleClicked，
        // 因此先 click 补上 press/release，再 dclick（单发 mouseDClick 不会触发）
        const QRect rect = list->visualItemRect(list->item(0));
        QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier, rect.center());
        QTest::mouseDClick(list->viewport(), Qt::LeftButton, Qt::NoModifier, rect.center());
        QTest::qWait(50);

        QVERIFY(sawDialog);
        QCOMPARE(list->item(0)->text(), QString("sh600000  浦发(老仓)"));
        QCOMPARE(w.currentConfig().value("name_map").toObject().value("sh600000").toString(),
                 QString("浦发(老仓)"));
        dlg.close();
    }

    void customConfigDialogSortAndAliasEdit() {
        ProbeWindow w(baseConfig());
        w.show();
        QTest::qWait(200);

        auto* table = w.findChild<QTableView*>();
        QVERIFY(table);
        w.pushQuotes(twoQuotes());

        CustomConfigDialog dlg(&w, &w);
        dlg.show();
        QTest::qWait(50);
        QCOMPARE(dlg.objectName(), QString("customConfigDialog"));

        auto* keyCombo = dlg.findChild<QComboBox*>("sortKeyCombo");
        auto* dirCombo = dlg.findChild<QComboBox*>("sortDirCombo");
        QVERIFY(keyCombo);
        QVERIFY(dirCombo);
        QCOMPARE(keyCombo->count(), QuoteSort::sortableKeys().size() + 1);  // 不排序 + 11 指标
        QVERIFY(!dirCombo->isEnabled());  // 未选指标时方向禁用

        const int col = columnOf(table->model(), "涨跌幅");
        QVERIFY(col >= 0);

        keyCombo->setCurrentIndex(keyCombo->findData(QString("change_pct")));
        QTest::qWait(50);
        QCOMPARE(w.currentConfig().value("sort_key").toString(), QString("change_pct"));
        QVERIFY(dirCombo->isEnabled());
        QCOMPARE(cellText(table->model(), 0, col), QString("+2.00%"));  // 降序

        dirCombo->setCurrentIndex(dirCombo->findData(true));
        QTest::qWait(50);
        QCOMPARE(w.currentConfig().value("sort_asc").toBool(), true);
        QCOMPARE(cellText(table->model(), 0, col), QString("-5.00%"));  // 升序

        keyCombo->setCurrentIndex(0);  // 不排序 → 回到自选顺序
        QTest::qWait(50);
        QCOMPARE(w.currentConfig().value("sort_key").toString(), QString());
        QVERIFY(!dirCombo->isEnabled());
        QCOMPARE(cellText(table->model(), 0, col), QString("+2.00%"));

        // 自定义名称：表格第二列可编辑，清空 = 恢复行情名称
        auto* aliasTable = dlg.findChild<QTableWidget*>("aliasTable");
        QVERIFY(aliasTable);
        QCOMPARE(aliasTable->rowCount(), 1);  // baseConfig 的 codes = {sh600000}
        QCOMPARE(aliasTable->item(0, 0)->text(), QString("sh600000"));

        const int nameCol = columnOf(table->model(), "名称");
        QVERIFY(nameCol >= 0);
        QCOMPARE(cellText(table->model(), 0, nameCol), QString("浦发银行"));

        aliasTable->item(0, 1)->setText(QStringLiteral("浦发(老仓)"));
        QTest::qWait(50);
        QCOMPARE(w.currentConfig().value("name_map").toObject().value("sh600000").toString(),
                 QString("浦发(老仓)"));
        QCOMPARE(cellText(table->model(), 0, nameCol), QString("浦发(老仓)"));

        aliasTable->item(0, 1)->setText(QString());
        QTest::qWait(50);
        QCOMPARE(w.currentConfig().value("name_map").toObject().size(), 0);
        QCOMPARE(cellText(table->model(), 0, nameCol), QString("浦发银行"));
        dlg.close();
    }

    void gearOpensCustomConfigWithoutHiding() {
        ProbeWindow w(baseConfig());
        w.show();
        QTest::qWait(200);

        auto* table = w.findChild<QTableView*>();
        QVERIFY(table);
        w.pushQuotes(twoQuotes());

        // 右键菜单里有「显示配置按钮」开关
        QMenu* menu = w.menuAt(table->viewport()->mapToGlobal(QPoint(5, 5)));
        QVERIFY(menu);
        bool hasGearToggle = false;
        for (QAction* act : menu->actions())
            if (act->text() == QString("显示配置按钮")) hasGearToggle = true;
        QVERIFY(hasGearToggle);
        menu->deleteLater();

        // 齿轮位于窗口右下角（12x12，内缩 4）；点击应打开配置对话框且不隐藏窗口
        const QPoint gear(w.width() - 4 - 6, w.height() - 4 - 6);
        QTest::mouseClick(&w, Qt::LeftButton, Qt::NoModifier, gear);
        QTest::qWait(50);
        QVERIFY2(w.isVisible(), "clicking the config button must not hide the window");
        QVERIFY(w.findChild<CustomConfigDialog*>());
    }

    void gearHiddenFallsBackToHide() {
        QJsonObject cfg = baseConfig();
        cfg["gear_visible"] = false;
        ProbeWindow w(cfg);
        w.show();
        QTest::qWait(200);

        const QPoint gear(w.width() - 10, w.height() - 10);
        QTest::mouseClick(&w, Qt::LeftButton, Qt::NoModifier, gear);
        QTest::qWait(50);

        QVERIFY(!w.findChild<CustomConfigDialog*>());
        QVERIFY2(!w.isVisible(), "without the config button, a click still hides the window");
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
