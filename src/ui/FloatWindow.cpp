#include "ui/FloatWindow.h"
#include "data/QuoteColumns.h"
#include "data/Schedule.h"
#include "data/SinaQuoteSource.h"
#include "data/StockCode.h"
#include "ui/KLineDelegate.h"
#include "ui/QuoteModel.h"

#include <QAction>
#include <QApplication>
#include <QContextMenuEvent>
#include <QCursor>
#include <QDate>
#include <QDateTime>
#include <QEnterEvent>
#include <QFrame>
#include <QHeaderView>
#include <QJsonArray>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QTableView>
#include <QTime>
#include <QTimer>
#include <QVBoxLayout>

namespace {

// 贴边隐藏后，留在屏幕边缘的可见细条宽度（px）。
constexpr int kEdgeHandlePx = 4;

QString b1s1ToString(QuoteFormatOptions::B1S1Display d) {
    switch (d) {
        case QuoteFormatOptions::B1S1Display::Price: return QStringLiteral("price");
        case QuoteFormatOptions::B1S1Display::Both: return QStringLiteral("both");
        default: return QStringLiteral("qty");
    }
}

QuoteFormatOptions::B1S1Display b1s1FromString(const QString& s) {
    if (s == QStringLiteral("price")) return QuoteFormatOptions::B1S1Display::Price;
    if (s == QStringLiteral("both")) return QuoteFormatOptions::B1S1Display::Both;
    return QuoteFormatOptions::B1S1Display::Qty;
}

}  // namespace

FloatWindow::FloatWindow(const QJsonObject& cfg, QWidget* parent) : QWidget(parent) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setFocusPolicy(Qt::StrongFocus);

    m_panel = new QWidget(this);
    m_vbox = new QVBoxLayout(m_panel);
    m_vbox->setContentsMargins(10, 6, 10, 6);
    m_vbox->setSpacing(0);

    m_errorLabel = new QLabel(QString(), m_panel);
    m_errorLabel->setStyleSheet(QStringLiteral("color: #ff6666; padding: 2px 4px;"));
    m_errorLabel->setVisible(false);
    m_vbox->addWidget(m_errorLabel);

    m_table = new QTableView(m_panel);
    m_table->setFrameShape(QFrame::NoFrame);
    m_table->setShowGrid(false);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setFocusPolicy(Qt::NoFocus);
    m_table->setTextElideMode(Qt::ElideNone);
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setMinimumSectionSize(1);
    m_table->verticalHeader()->setDefaultSectionSize(1);
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_vbox->addWidget(m_table);

    m_model = new QuoteModel(this);
    m_table->setModel(m_model);
    m_klineDelegate = new KLineDelegate(m_table);
    m_table->setItemDelegate(m_klineDelegate);

    m_source = new SinaQuoteSource(this);
    connect(m_source, &SinaQuoteSource::quotesReady, this, &FloatWindow::onQuotesReady);
    connect(m_source, &SinaQuoteSource::error, this, [this](const QString& msg) {
        m_errorLabel->setText(msg);
        m_errorLabel->setVisible(true);
        refitSize();
    });

    QWidget* filtered[] = {m_panel, m_table, m_errorLabel, m_table->viewport(),
                           m_table->horizontalHeader()};
    for (QWidget* w : filtered) w->installEventFilter(this);

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &FloatWindow::refreshNow);

    // 时段调度：不随窗口隐藏而停止，用于在进入时段时自动显示、离开时自动隐藏。
    m_scheduleTimer = new QTimer(this);
    m_scheduleTimer->setInterval(20000);
    connect(m_scheduleTimer, &QTimer::timeout, this, &FloatWindow::evaluateSchedule);

    // 贴边隐藏：轮询鼠标位置，离开 400ms 后收起，划过边缘细条时恢复。
    m_edgeTimer = new QTimer(this);
    m_edgeTimer->setInterval(150);
    connect(m_edgeTimer, &QTimer::timeout, this, &FloatWindow::checkEdgeHover);

    applyConfig(cfg);
    m_timer->start(m_refreshSeconds * 1000);
    m_scheduleTimer->start();

    const QRect scr = QApplication::primaryScreen()->availableGeometry();
    const QJsonObject pos = cfg.value(QStringLiteral("pos")).toObject();
    if (pos.contains(QStringLiteral("x")) && pos.contains(QStringLiteral("y"))) {
        const int x = qBound(scr.left(), pos.value(QStringLiteral("x")).toInt(),
                             scr.right() - width());
        const int y = qBound(scr.top(), pos.value(QStringLiteral("y")).toInt(),
                             scr.bottom() - height());
        move(x, y);
    } else {
        move(scr.right() - width() - 40, scr.bottom() - height() - 80);
    }

    refreshNow();
    m_wasInShowWindow = inShowWindow();
}

QString FloatWindow::layoutSignature() const {
    QString s;
    for (const QString& h : QuoteColumns::allHeaders())
        s += QuoteColumns::configKeyFor(h) +
             (QuoteColumns::isVisible(m_columnCfg, h) ? QStringLiteral("1") : QStringLiteral("0"));
    s += QLatin1Char('|') + m_fontFamily + QLatin1Char('|') + QString::number(m_fontSize) +
         QLatin1Char('|') + QString::number(m_lineExtraPx) +
         (m_headerVisible ? QStringLiteral("|H1") : QStringLiteral("|H0"));
    return s;
}

void FloatWindow::applyConfig(const QJsonObject& raw) {
    m_columnCfg = raw;

    const QStringList oldChecked = m_checkedCodes;

    QStringList codes;
    for (const QJsonValue& v : raw.value(QStringLiteral("codes")).toArray()) codes << v.toString();
    m_codes = StockCode::normalizeList(codes);
    if (m_codes.isEmpty()) m_codes = QStringList{QStringLiteral("sh000001")};

    QStringList checked;
    const QJsonArray checkedArr = raw.contains(QStringLiteral("checked_codes"))
        ? raw.value(QStringLiteral("checked_codes")).toArray()
        : raw.value(QStringLiteral("visible_codes")).toArray();
    for (const QJsonValue& v : checkedArr) checked << v.toString();
    m_checkedCodes = StockCode::normalizeList(checked);
    if (m_checkedCodes.isEmpty()) m_checkedCodes = m_codes;

    m_refreshSeconds = raw.value(QStringLiteral("refresh_seconds")).toInt(2);
    m_shortCode = raw.value(QStringLiteral("short_code")).toBool(false);
    m_nameLength = raw.value(QStringLiteral("name_length")).toInt(0);
    m_b1s1 = b1s1FromString(raw.value(QStringLiteral("b1s1_display"))
                                .toString(raw.value(QStringLiteral("b1s1_price")).toBool(false)
                                              ? QStringLiteral("price")
                                              : QStringLiteral("qty")));
    m_headerVisible = raw.value(QStringLiteral("header_visible")).toBool(false);
    m_gridVisible = raw.value(QStringLiteral("grid_visible")).toBool(false);
    m_defaultColor = raw.value(QStringLiteral("default_color")).toBool(false);
    m_lineExtraPx = raw.value(QStringLiteral("line_extra_px")).toInt(1);
    m_opacityPct = raw.value(QStringLiteral("opacity_pct")).toInt(90);
    m_fontFamily = raw.value(QStringLiteral("font_family")).toString(QStringLiteral("Microsoft YaHei"));
    m_fontSize = raw.value(QStringLiteral("font_size")).toInt(10);
    m_hotkey = raw.value(QStringLiteral("hotkey")).toString(QStringLiteral("Ctrl+Alt+F"));
    m_startOnBoot = raw.value(QStringLiteral("start_on_boot")).toBool(false);
    m_appIcon = raw.value(QStringLiteral("app_icon")).toString();
    m_showMode = raw.value(QStringLiteral("show_mode")).toString(QStringLiteral("always"));
    m_showStart = raw.value(QStringLiteral("show_start")).toString(QStringLiteral("09:15"));
    m_showEnd = raw.value(QStringLiteral("show_end")).toString(QStringLiteral("15:00"));
    m_fetchMode = raw.value(QStringLiteral("fetch_mode")).toString(QStringLiteral("always"));
    m_fetchStart = raw.value(QStringLiteral("fetch_start")).toString(QStringLiteral("09:15"));
    m_fetchEnd = raw.value(QStringLiteral("fetch_end")).toString(QStringLiteral("15:00"));
    const bool edgeHide = raw.value(QStringLiteral("edge_hide")).toBool(false);
    if (m_edgeHide && !edgeHide) restoreFromEdge();
    m_edgeHide = edgeHide;
    if (m_edgeHide) {
        if (!m_edgeTimer->isActive()) m_edgeTimer->start();
    } else {
        m_edgeTimer->stop();
        m_outsideSince = 0;
    }

    m_fg = QColor(raw.value(QStringLiteral("fg")).toString(QStringLiteral("#FFFFFF")));
    const QJsonObject bg = raw.value(QStringLiteral("bg")).toObject();
    m_bg = QColor(bg.value(QStringLiteral("r")).toInt(0), bg.value(QStringLiteral("g")).toInt(0),
                  bg.value(QStringLiteral("b")).toInt(0), bg.value(QStringLiteral("a")).toInt(191));

    m_font = QFont(m_fontFamily, qBound(8, m_fontSize, 15));
    m_table->setFont(m_font);
    m_table->horizontalHeader()->setFont(m_font);
    m_table->horizontalHeader()->setVisible(m_headerVisible);
    setWindowOpacity(qBound(20, m_opacityPct, 100) / 100.0);

    QuoteFormatOptions opt;
    opt.shortCode = m_shortCode;
    opt.nameLength = m_nameLength;
    opt.b1s1 = m_b1s1;
    m_source->setFormatOptions(opt);

    if (m_refreshSeconds > 0) m_timer->setInterval(m_refreshSeconds * 1000);

    applyFontAndMetrics();
    applyStyle();
    update();

    const QString sig = layoutSignature();
    if (sig != m_layoutSig) {
        m_layoutSig = sig;
        rebuildColumns();
    }

    if (oldChecked != m_checkedCodes && isVisible()) refreshNow();
}

void FloatWindow::applyStyle() {
    const QString colorRule =
        m_defaultColor ? QString() : QStringLiteral("color: %1;").arg(m_fg.name());
    const QString lineCol = QStringLiteral("rgba(%1,%2,%3,80)")
                                .arg(m_fg.red())
                                .arg(m_fg.green())
                                .arg(m_fg.blue());
    const QString gridBorder =
        m_gridVisible ? QStringLiteral("1px solid %1").arg(lineCol) : QStringLiteral("none");
    const QString sheet =
        QStringLiteral(
            "QTableView{background:transparent;border:none;outline:none;%1}"
            "QHeaderView{background-color:transparent;}"
            "QHeaderView::section{background:transparent;border:none;padding:2px 4px;%1}"
            "QTableView::item{border-right:%2;border-bottom:%2;}")
            .arg(colorRule, gridBorder);
    m_table->setStyleSheet(sheet);
}

void FloatWindow::applyFontAndMetrics() {
    const int rowH = m_table->fontMetrics().height() + qMax(0, m_lineExtraPx);
    m_table->verticalHeader()->setDefaultSectionSize(rowH);
    m_klineDelegate->setColorScheme(m_defaultColor, m_fg);
    m_klineDelegate->setPointSize(m_font.pointSize());
    m_model->setColorScheme(m_defaultColor, m_fg);
}

void FloatWindow::rebuildColumns() {
    m_model->setColumns(QuoteColumns::activeColumns(m_columnCfg));
    const int kcol = m_model->klineColumn();
    if (kcol >= 0) m_table->setItemDelegateForColumn(kcol, m_klineDelegate);
    m_columnWidthsFrozen = false;
    refitSize();
}

void FloatWindow::onQuotesReady(const QVector<Quote>& quotes) {
    m_errorLabel->setVisible(false);
    const int before = m_model->rowCount();
    m_model->setQuotes(quotes);
    if (m_model->rowCount() != before) m_columnWidthsFrozen = false;
    refitSize();
}

void FloatWindow::refreshNow() {
    if (!isVisible()) return;
    if (!inFetchWindow()) return;  // 非请求时段：跳过拉取，保留上次数据
    m_source->fetch(m_checkedCodes);
}

bool FloatWindow::inShowWindow() const {
    const QTime now = QTime::currentTime();
    return Schedule::inWindow(m_showMode, m_showStart, m_showEnd, now.hour() * 60 + now.minute(),
                              QDate::currentDate().dayOfWeek());
}

bool FloatWindow::inFetchWindow() const {
    const QTime now = QTime::currentTime();
    return Schedule::inWindow(m_fetchMode, m_fetchStart, m_fetchEnd, now.hour() * 60 + now.minute(),
                              QDate::currentDate().dayOfWeek());
}

void FloatWindow::evaluateSchedule() {
    const bool inside = inShowWindow();
    if (inside == m_wasInShowWindow) return;  // 仅在跨越时段边界时切换，不打断手动操作
    m_wasInShowWindow = inside;
    if (inside) {
        show();
        raise();
    } else {
        hide();
    }
}

void FloatWindow::refitSize() {
    if (!m_columnWidthsFrozen) {
        m_table->resizeColumnsToContents();
        m_columnWidthsFrozen = true;
    }
    const int cols = m_model->columnCount();
    const int rows = m_model->rowCount();
    int totalW = m_table->verticalHeader()->width() + 2 * m_table->frameWidth();
    for (int c = 0; c < cols; ++c) totalW += m_table->columnWidth(c);
    const int hh =
        m_table->horizontalHeader()->isVisible() ? m_table->horizontalHeader()->height() : 0;
    const int totalH =
        hh + 2 * m_table->frameWidth() + rows * m_table->verticalHeader()->defaultSectionSize();
    m_table->setFixedSize(qMax(1, totalW), qMax(1, totalH));
    m_panel->adjustSize();
    resize(m_panel->size());
}

void FloatWindow::enterEvent(QEnterEvent* event) {
    QWidget::enterEvent(event);
    if (m_edgeHide) restoreFromEdge();
}

void FloatWindow::checkEdgeHover() {
    if (!m_edgeHide || !isVisible() || m_dragging || QApplication::activePopupWidget()) {
        m_outsideSince = 0;
        return;
    }
    if (rect().contains(mapFromGlobal(QCursor::pos()))) {
        m_outsideSince = 0;
        restoreFromEdge();
        return;
    }
    if (m_collapsed) return;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now < m_edgeGraceUntil) {  // 刚显示/刚定位：先给一段宽限期，不要立刻收起
        m_outsideSince = 0;
        return;
    }
    if (m_outsideSince == 0) {
        m_outsideSince = now;
        return;
    }
    if (now - m_outsideSince >= 400) {
        m_outsideSince = 0;
        collapseToEdge();
    }
}

void FloatWindow::locate() {
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    m_edgeGraceUntil = now + 10000;    // 10s 内不自动收起
    m_forceVisibleUntil = now + 10000; // 10s 内不受显示时段限制
    restoreFromEdge();
    if (QScreen* scr = QApplication::primaryScreen()) {
        const QRect g = scr->availableGeometry();
        move(g.center().x() - width() / 2, g.center().y() - height() / 2);
    }
    if (!isVisible()) show();
    raise();
    activateWindow();
    notifyChanged();
}

void FloatWindow::collapseToEdge() {
    QScreen* scr = screen() ? screen() : QApplication::primaryScreen();
    if (!scr) return;
    const QRect g = scr->availableGeometry();
    const QRect w = frameGeometry();
    if (w.width() <= 0 || w.height() <= 0) return;

    const int cx = w.center().x(), cy = w.center().y();
    const int dLeft = cx - g.left(), dRight = g.right() - cx;
    const int dTop = cy - g.top(), dBottom = g.bottom() - cy;
    const int nearest = qMin(qMin(dLeft, dRight), qMin(dTop, dBottom));

    QPoint flush = w.topLeft();
    QPoint hidden = w.topLeft();
    if (nearest == dLeft) {
        flush.setX(g.left());
        hidden.setX(g.left() - w.width() + kEdgeHandlePx);
    } else if (nearest == dRight) {
        flush.setX(g.right() - w.width() + 1);
        hidden.setX(g.right() - kEdgeHandlePx + 1);
    } else if (nearest == dTop) {
        flush.setY(g.top());
        hidden.setY(g.top() - w.height() + kEdgeHandlePx);
    } else {
        flush.setY(g.bottom() - w.height() + 1);
        hidden.setY(g.bottom() - kEdgeHandlePx + 1);
    }
    flush.setX(qBound(g.left(), flush.x(), qMax(g.left(), g.right() - w.width() + 1)));
    flush.setY(qBound(g.top(), flush.y(), qMax(g.top(), g.bottom() - w.height() + 1)));

    m_flushPos = flush;
    move(hidden);
    m_collapsed = true;
}

void FloatWindow::restoreFromEdge() {
    if (!m_collapsed) return;
    m_collapsed = false;
    move(m_flushPos);
}

void FloatWindow::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setBrush(m_bg);
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(rect(), 5, 5);
}

void FloatWindow::notifyChanged() { emit configChanged(); }

QJsonObject FloatWindow::currentConfig() const {
    QJsonObject cfg = m_columnCfg;
    cfg[QStringLiteral("codes")] = QJsonArray::fromStringList(m_codes);
    cfg[QStringLiteral("checked_codes")] = QJsonArray::fromStringList(m_checkedCodes);
    cfg[QStringLiteral("refresh_seconds")] = m_refreshSeconds;
    cfg[QStringLiteral("short_code")] = m_shortCode;
    cfg[QStringLiteral("name_length")] = m_nameLength;
    cfg[QStringLiteral("b1s1_display")] = b1s1ToString(m_b1s1);
    cfg[QStringLiteral("b1s1_price")] = (m_b1s1 == QuoteFormatOptions::B1S1Display::Price);
    cfg[QStringLiteral("header_visible")] = m_headerVisible;
    cfg[QStringLiteral("grid_visible")] = m_gridVisible;
    cfg[QStringLiteral("default_color")] = m_defaultColor;
    cfg[QStringLiteral("line_extra_px")] = m_lineExtraPx;
    cfg[QStringLiteral("opacity_pct")] = int(qRound(windowOpacity() * 100));
    cfg[QStringLiteral("font_family")] = m_fontFamily;
    cfg[QStringLiteral("font_size")] = m_fontSize;
    cfg[QStringLiteral("hotkey")] = m_hotkey;
    cfg[QStringLiteral("start_on_boot")] = m_startOnBoot;
    cfg[QStringLiteral("app_icon")] = m_appIcon;
    cfg[QStringLiteral("show_mode")] = m_showMode;
    cfg[QStringLiteral("show_start")] = m_showStart;
    cfg[QStringLiteral("show_end")] = m_showEnd;
    cfg[QStringLiteral("fetch_mode")] = m_fetchMode;
    cfg[QStringLiteral("fetch_start")] = m_fetchStart;
    cfg[QStringLiteral("fetch_end")] = m_fetchEnd;
    cfg[QStringLiteral("edge_hide")] = m_edgeHide;
    cfg[QStringLiteral("fg")] = m_fg.name(QColor::HexRgb);
    QJsonObject bg;
    bg[QStringLiteral("r")] = m_bg.red();
    bg[QStringLiteral("g")] = m_bg.green();
    bg[QStringLiteral("b")] = m_bg.blue();
    bg[QStringLiteral("a")] = m_bg.alpha();
    cfg[QStringLiteral("bg")] = bg;
    QJsonObject pos;
    const QPoint saved = m_collapsed ? m_flushPos : QPoint(x(), y());
    pos[QStringLiteral("x")] = saved.x();
    pos[QStringLiteral("y")] = saved.y();
    cfg[QStringLiteral("pos")] = pos;
    return cfg;
}

void FloatWindow::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragMoved = false;
        m_pressGlobalPos = e->globalPosition().toPoint();
        m_dragOffset = m_pressGlobalPos - frameGeometry().topLeft();
        setFocus(Qt::MouseFocusReason);
        e->accept();
    }
}

void FloatWindow::mouseMoveEvent(QMouseEvent* e) {
    if (m_dragging && (e->buttons() & Qt::LeftButton)) {
        const QPoint global = e->globalPosition().toPoint();
        if ((global - m_pressGlobalPos).manhattanLength() >= QApplication::startDragDistance())
            m_dragMoved = true;
        if (m_dragMoved) move(global - m_dragOffset);
        e->accept();
    }
}

void FloatWindow::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton && m_dragging) {
        m_dragging = false;
        if (m_dragMoved)
            persistGeometry();
        else if (!m_edgeHide)
            hide();  // 单击（非拖动）隐藏；开启贴边隐藏时不隐藏
        e->accept();
    }
}

void FloatWindow::mouseDoubleClickEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        m_dragging = false;
        hide();
    }
}

bool FloatWindow::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::MouseButtonDblClick) {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            m_dragging = false;
            hide();
            return true;
        }
    } else if (event->type() == QEvent::MouseButtonPress) {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            m_dragging = true;
            m_dragMoved = false;
            m_pressGlobalPos = me->globalPosition().toPoint();
            m_dragOffset = m_pressGlobalPos - frameGeometry().topLeft();
            return true;
        }
    } else if (event->type() == QEvent::MouseMove) {
        auto* me = static_cast<QMouseEvent*>(event);
        if (m_dragging && (me->buttons() & Qt::LeftButton)) {
            const QPoint global = me->globalPosition().toPoint();
            if ((global - m_pressGlobalPos).manhattanLength() >=
                QApplication::startDragDistance())
                m_dragMoved = true;
            if (m_dragMoved) move(global - m_dragOffset);
            return true;
        }
    } else if (event->type() == QEvent::MouseButtonRelease) {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton && m_dragging) {
            m_dragging = false;
            if (m_dragMoved)
                persistGeometry();
            else if (!m_edgeHide)
                hide();  // 单击（非拖动）隐藏；开启贴边隐藏时不隐藏
            return true;
        }
    } else if (event->type() == QEvent::ContextMenu) {
        auto* ce = static_cast<QContextMenuEvent*>(event);
        // 转发给窗口自身，保留 contextMenuEvent 的虚分派（事件可能被子控件吃掉）
        QContextMenuEvent forward(ce->reason(), mapFromGlobal(ce->globalPos()), ce->globalPos());
        QApplication::sendEvent(this, &forward);
        return true;
    }
    return QWidget::eventFilter(obj, event);
}

void FloatWindow::persistGeometry() { notifyChanged(); }

void FloatWindow::contextMenuEvent(QContextMenuEvent* event) {
    showContextMenu(event->globalPos());
}

void FloatWindow::showContextMenu(const QPoint& globalPos) {
    QMenu menu(this);
    QMenu* cols = menu.addMenu(QStringLiteral("显示指标"));
    for (const QString& header : QuoteColumns::allHeaders()) {
        if (header == QStringLiteral("卖一")) continue;
        if (header == QStringLiteral("买一")) {
            auto* act = cols->addAction(QStringLiteral("买一/卖一"));
            act->setCheckable(true);
            act->setChecked(QuoteColumns::isVisible(m_columnCfg, header));
            connect(act, &QAction::toggled, this,
                    [this](bool on) { setHeaderFlag(QStringLiteral("买一"), on); });
            continue;
        }
        auto* act = cols->addAction(header);
        act->setCheckable(true);
        act->setChecked(QuoteColumns::isVisible(m_columnCfg, header));
        connect(act, &QAction::toggled, this,
                [this, header](bool on) { setHeaderFlag(header, on); });
    }
    auto* actHeader = menu.addAction(QStringLiteral("显示表头"));
    actHeader->setCheckable(true);
    actHeader->setChecked(m_headerVisible);
    connect(actHeader, &QAction::toggled, this, [this](bool on) {
        m_headerVisible = on;
        m_table->horizontalHeader()->setVisible(on);
        refitSize();
        notifyChanged();
    });
    auto* actGrid = menu.addAction(QStringLiteral("显示网格"));
    actGrid->setCheckable(true);
    actGrid->setChecked(m_gridVisible);
    connect(actGrid, &QAction::toggled, this, [this](bool on) {
        m_gridVisible = on;
        m_columnCfg[QStringLiteral("grid_visible")] = on;
        applyStyle();
        notifyChanged();
    });
    auto* actColor = menu.addAction(QStringLiteral("默认颜色"));
    actColor->setCheckable(true);
    actColor->setChecked(m_defaultColor);
    connect(actColor, &QAction::toggled, this, [this](bool on) {
        m_defaultColor = on;
        m_columnCfg[QStringLiteral("default_color")] = on;
        applyFontAndMetrics();
        applyStyle();
        notifyChanged();
    });
    menu.addSeparator();
    menu.addAction(QStringLiteral("设置…"), this, [this] {
        if (m_openSettings) m_openSettings();
    });
    menu.addSeparator();
    menu.addAction(QStringLiteral("隐藏浮窗"), this, &QWidget::hide);
    menu.exec(globalPos);
}

void FloatWindow::setHeaderFlag(const QString& header, bool on) {
    const QString key = QuoteColumns::configKeyFor(header);
    if (key.isEmpty()) return;
    m_columnCfg[key] = on;
    const QString sig = layoutSignature();
    if (sig != m_layoutSig) {
        m_layoutSig = sig;
        rebuildColumns();
    }
    notifyChanged();
}

void FloatWindow::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    m_edgeGraceUntil = qMax(m_edgeGraceUntil, now + 3000);  // 刚显示时宽限 3s
    // 按时段显示：当前不在时段内则立即隐藏（定位时 force 优先）。
    if (m_showMode != QStringLiteral("always") && now >= m_forceVisibleUntil &&
        !inShowWindow()) {
        m_timer->stop();
        hide();
        return;
    }
    restoreFromEdge();  // 从托盘/快捷键重新显示时，不要在收起位置
    if (!m_timer->isActive()) m_timer->start(m_refreshSeconds * 1000);
    refreshNow();
}

void FloatWindow::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    m_timer->stop();
    notifyChanged();
}

void FloatWindow::stop() { m_timer->stop(); }
