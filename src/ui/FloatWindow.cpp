#include "ui/FloatWindow.h"
#include "data/QuoteColumns.h"
#include "data/SinaQuoteSource.h"
#include "data/StockCode.h"
#include "ui/KLineDelegate.h"
#include "ui/QuoteModel.h"

#include <QAction>
#include <QApplication>
#include <QContextMenuEvent>
#include <QFrame>
#include <QHeaderView>
#include <QJsonArray>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>

namespace {

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
    auto* vbox = new QVBoxLayout(m_panel);
    vbox->setContentsMargins(10, 6, 10, 6);
    vbox->setSpacing(0);

    m_errorLabel = new QLabel(QString(), m_panel);
    m_errorLabel->setStyleSheet(QStringLiteral("color: #ff6666; padding: 2px 4px;"));
    m_errorLabel->setVisible(false);
    vbox->addWidget(m_errorLabel);

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
    m_table->horizontalHeader()->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    vbox->addWidget(m_table);

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

    applyConfig(cfg);
    m_timer->start(m_refreshSeconds * 1000);

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
    m_source->fetch(m_checkedCodes);
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
    cfg[QStringLiteral("fg")] = m_fg.name(QColor::HexRgb);
    QJsonObject bg;
    bg[QStringLiteral("r")] = m_bg.red();
    bg[QStringLiteral("g")] = m_bg.green();
    bg[QStringLiteral("b")] = m_bg.blue();
    bg[QStringLiteral("a")] = m_bg.alpha();
    cfg[QStringLiteral("bg")] = bg;
    QJsonObject pos;
    pos[QStringLiteral("x")] = x();
    pos[QStringLiteral("y")] = y();
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
        else
            hide();  // 单击（非拖动）隐藏
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
            else
                hide();  // 单击（非拖动）隐藏
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
    if (!m_timer->isActive()) m_timer->start(m_refreshSeconds * 1000);
    refreshNow();
}

void FloatWindow::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    m_timer->stop();
    notifyChanged();
}

void FloatWindow::stop() { m_timer->stop(); }
