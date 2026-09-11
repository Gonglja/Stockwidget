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

// Subclass so the context-menu slot doesn't open a modal menu during tests.
class ProbeWindow : public FloatWindow {
public:
    using FloatWindow::FloatWindow;
    int ctxCount = 0;
    void contextMenuEvent(QContextMenuEvent*) override { ++ctxCount; }
};

static QJsonObject baseConfig() {
    QJsonObject cfg;
    cfg["checked_codes"] = QJsonArray{"sh600000"};
    cfg["price_visible"] = true;
    cfg["change_pct_visible"] = true;
    cfg["header_visible"] = false;
    return cfg;
}

class TestUi : public QObject {
    Q_OBJECT
private slots:
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
