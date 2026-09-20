#pragma once
#include "data/Quote.h"
#include <QColor>
#include <QFont>
#include <QJsonObject>
#include <QStringList>
#include <QWidget>
#include <functional>

class QLabel;
class QMenu;
class QTableView;
class QTimer;
class QVBoxLayout;
class QuoteModel;
class KLineDelegate;
class SinaQuoteSource;

class FloatWindow : public QWidget {
    Q_OBJECT
public:
    explicit FloatWindow(const QJsonObject& cfg, QWidget* parent = nullptr);

    QJsonObject currentConfig() const;
    void applyConfig(const QJsonObject& cfg);
    void setOpenSettingsCallback(std::function<void()> cb) { m_openSettings = std::move(cb); }
    void locate();  // 定位：居中显示并暂时挂起自动隐藏
    void stop();
    void setAlias(const QString& code, const QString& alias);
    void openCustomConfig();  // 右下角配置按钮/测试入口

    // 该代码当前的行情名称（原始名，不含用户别名）；无数据或未命中返回空串。
    QString quoteNameFor(const QString& code) const;

signals:
    void configChanged();
    void quotesUpdated();  // 行情刷新完成（供设置面板回填名称列）

protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    QMenu* buildContextMenu(const QPoint& globalPos);
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private slots:
    void onQuotesReady(const QVector<Quote>& quotes);

private:
    void applyStyle();
    void applyFontAndMetrics();
    void rebuildColumns();
    void refreshNow();
    void refitSize();
    void notifyChanged();
    void persistGeometry();
    void setHeaderFlag(const QString& header, bool on);
    void showContextMenu(const QPoint& globalPos);
    void updateSortIndicator();
    void cycleSort(int column);
    void redisplayLastQuotes();
    void updateGearRect();
    void updateGearHover(const QPoint& localPos);
    void collapseToEdge();
    void restoreFromEdge();
    void checkEdgeHover();
    bool inShowWindow() const;
    bool inFetchWindow() const;
    void evaluateSchedule();
    QColor effectiveBg() const;
    QColor effectiveFg() const;
    QString layoutSignature() const;

    // 配置状态
    QStringList m_codes;
    QStringList m_checkedCodes;
    int m_refreshSeconds = 2;
    bool m_shortCode = false;
    int m_nameLength = 0;
    QJsonObject m_nameMap;
    QString m_sortKey;
    bool m_sortAsc = false;
    QVector<Quote> m_rawQuotes;
    QuoteFormatOptions::B1S1Display m_b1s1 = QuoteFormatOptions::B1S1Display::Qty;
    bool m_headerVisible = false;
    bool m_gridVisible = false;
    bool m_defaultColor = false;
    int m_lineExtraPx = 1;
    int m_opacityPct = 90;
    QString m_fontFamily = QStringLiteral("Microsoft YaHei");
    int m_fontSize = 10;
    QString m_hotkey = QStringLiteral("Ctrl+Alt+F");
    bool m_startOnBoot = false;
    QString m_appIcon;
    QString m_showMode = QStringLiteral("always");
    QString m_showStart = QStringLiteral("09:15");
    QString m_showEnd = QStringLiteral("15:00");
    QString m_fetchMode = QStringLiteral("always");
    QString m_fetchStart = QStringLiteral("09:15");
    QString m_fetchEnd = QStringLiteral("15:00");
    QJsonObject m_columnCfg;
    QString m_layoutSig;

    QColor m_fg = QColor(QStringLiteral("#FFFFFF"));
    QColor m_bg = QColor(0, 0, 0, 191);
    QFont m_font;

    // UI
    QWidget* m_panel = nullptr;
    QVBoxLayout* m_vbox = nullptr;
    QTableView* m_table = nullptr;
    QLabel* m_errorLabel = nullptr;
    QuoteModel* m_model = nullptr;
    KLineDelegate* m_klineDelegate = nullptr;
    SinaQuoteSource* m_source = nullptr;
    QTimer* m_timer = nullptr;
    QTimer* m_scheduleTimer = nullptr;
    QTimer* m_edgeTimer = nullptr;
    bool m_wasInShowWindow = false;
    bool m_edgeHide = false;
    QString m_edgeSide = QStringLiteral("auto");
    bool m_collapsed = false;
    qint64 m_outsideSince = 0;
    qint64 m_edgeGraceUntil = 0;
    qint64 m_forceVisibleUntil = 0;
    QPoint m_flushPos;

    bool m_dragging = false;
    bool m_dragMoved = false;
    QPoint m_pressGlobalPos;
    QPoint m_dragOffset;
    std::function<void()> m_openSettings;
    bool m_columnWidthsFrozen = false;
    bool m_pressOnHeader = false;
    int m_headerPressColumn = -1;
    QRect m_gearRect;
    bool m_gearVisible = true;
    bool m_gearHover = false;
    bool m_pressOnGear = false;
};
