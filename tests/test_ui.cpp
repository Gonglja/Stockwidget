#include <QtTest>
#include <QApplication>
#include <QContextMenuEvent>
#include <QJsonArray>
#include <QJsonObject>
#include <QTableView>
#include "ui/FloatWindow.h"

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
};

QTEST_MAIN(TestUi)
#include "test_ui.moc"
