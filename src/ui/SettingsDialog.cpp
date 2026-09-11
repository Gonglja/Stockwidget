#include "ui/SettingsDialog.h"
#include "data/QuoteColumns.h"
#include "data/StockCode.h"
#include "ui/FloatWindow.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QFileDialog>
#include <QFontDatabase>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QKeySequence>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSlider>
#include <QTabWidget>
#include <QTime>
#include <QTimeEdit>
#include <QVBoxLayout>
#include <functional>

namespace {
SettingsDialog* g_instance = nullptr;
const int kIntervals[] = {1, 2, 3, 5, 10, 15, 30, 60};
const char* kTabTitles[] = {"自选列表", "显示数据", "外观", "常规"};
}  // namespace

SettingsDialog::SettingsDialog(FloatWindow* win, QWidget* parent) : QDialog(parent), m_win(win) {
    setWindowTitle(QStringLiteral("设置"));
    setModal(false);
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    m_tabs = new QTabWidget(this);
    root->addWidget(m_tabs);
    for (const char* title : kTabTitles) m_tabs->addTab(new QWidget(), QString::fromUtf8(title));
    connect(m_tabs, &QTabWidget::currentChanged, this, &SettingsDialog::ensureTab);
    ensureTab(0);
    resize(460, 440);
}

SettingsDialog* SettingsDialog::showFor(FloatWindow* win, QWidget* parent) {
    if (!g_instance) g_instance = new SettingsDialog(win, parent);
    g_instance->show();
    g_instance->raise();
    g_instance->activateWindow();
    return g_instance;
}

void SettingsDialog::ensureTab(int index) {
    if (index < 0 || index >= 4 || (m_builtTabs & (1 << index))) return;
    m_builtTabs |= (1 << index);
    QWidget* page = nullptr;
    switch (index) {
        case 0: page = buildCodesTab(); break;
        case 1: page = buildDataTab(); break;
        case 2: page = buildAppearanceTab(); break;
        default: page = buildGeneralTab(); break;
    }
    QWidget* old = m_tabs->widget(index);
    m_tabs->blockSignals(true);
    m_tabs->removeTab(index);
    m_tabs->insertTab(index, page, QString::fromUtf8(kTabTitles[index]));
    m_tabs->setCurrentIndex(index);
    m_tabs->blockSignals(false);
    delete old;
}

QWidget* SettingsDialog::buildCodesTab() {
    auto* page = new QWidget();
    auto* lay = new QVBoxLayout(page);
    auto* group = new QGroupBox(QStringLiteral("自选列表"), page);
    auto* h = new QHBoxLayout(group);
    m_codeList = new QListWidget(group);
    m_codeList->setFixedWidth(150);

    const QJsonObject cfg = m_win->currentConfig();
    const QJsonArray codes = cfg.value(QStringLiteral("codes")).toArray();
    const QJsonArray checked = cfg.value(QStringLiteral("checked_codes")).toArray();
    for (const QJsonValue& v : codes) {
        auto* item = new QListWidgetItem(v.toString(), m_codeList);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEditable);
        item->setCheckState(checked.contains(v) ? Qt::Checked : Qt::Unchecked);
    }

    auto commit = [this] {
        QStringList list, checkedList;
        for (int i = 0; i < m_codeList->count(); ++i) {
            const QListWidgetItem* it = m_codeList->item(i);
            const auto n = StockCode::normalize(it->text());
            if (!n) continue;
            list << *n;
            if (it->checkState() == Qt::Checked) checkedList << *n;
        }
        QJsonObject c = m_win->currentConfig();
        c[QStringLiteral("codes")] = QJsonArray::fromStringList(list);
        c[QStringLiteral("checked_codes")] = QJsonArray::fromStringList(checkedList);
        m_win->applyConfig(c);
    };

    auto* btnCol = new QVBoxLayout();
    auto addBtn = [&](const QString& text, std::function<void()> fn) {
        auto* b = new QPushButton(text, group);
        b->setFixedWidth(60);
        connect(b, &QPushButton::clicked, this, fn);
        btnCol->addWidget(b);
    };
    addBtn(QStringLiteral("添加"), [this, commit] {
        auto* it = new QListWidgetItem(QStringLiteral("sh000001"), m_codeList);
        it->setFlags(it->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEditable);
        it->setCheckState(Qt::Unchecked);
        m_codeList->setCurrentItem(it);
        m_codeList->editItem(it);
        commit();
    });
    addBtn(QStringLiteral("删除"), [this, commit] {
        delete m_codeList->takeItem(m_codeList->currentRow());
        commit();
    });
    addBtn(QStringLiteral("上移"), [this, commit] {
        const int r = m_codeList->currentRow();
        if (r > 0) {
            auto* it = m_codeList->takeItem(r);
            m_codeList->insertItem(r - 1, it);
            m_codeList->setCurrentRow(r - 1);
            commit();
        }
    });
    addBtn(QStringLiteral("下移"), [this, commit] {
        const int r = m_codeList->currentRow();
        if (r >= 0 && r < m_codeList->count() - 1) {
            auto* it = m_codeList->takeItem(r);
            m_codeList->insertItem(r + 1, it);
            m_codeList->setCurrentRow(r + 1);
            commit();
        }
    });
    btnCol->addStretch(1);

    h->addWidget(m_codeList, 1);
    h->addLayout(btnCol);
    lay->addWidget(group);
    connect(m_codeList, &QListWidget::itemChanged, this, [commit](QListWidgetItem*) { commit(); });
    return page;
}

QWidget* SettingsDialog::buildDataTab() {
    const QJsonObject cfg = m_win->currentConfig();
    auto* page = new QWidget();
    auto* lay = new QVBoxLayout(page);

    auto* intervalGroup = new QGroupBox(QStringLiteral("刷新间隔"), page);
    m_interval = new QComboBox(intervalGroup);
    for (int s : kIntervals) m_interval->addItem(QStringLiteral("%1 秒").arg(s), s);
    int idx = m_interval->findData(cfg.value(QStringLiteral("refresh_seconds")).toInt(2));
    m_interval->setCurrentIndex(idx >= 0 ? idx : 1);
    auto* intervalLay = new QVBoxLayout(intervalGroup);
    intervalLay->addWidget(m_interval);
    lay->addWidget(intervalGroup);

    auto* flags = new QGroupBox(QStringLiteral("显示指标"), page);
    auto* grid = new QGridLayout(flags);
    int col = 0, row = 0;
    for (const QString& header : QuoteColumns::allHeaders()) {
        if (header == QStringLiteral("卖一")) continue;
        const QString label =
            header == QStringLiteral("买一") ? QStringLiteral("买一/卖一") : header;
        auto* cb = new QCheckBox(label, flags);
        cb->setChecked(QuoteColumns::isVisible(cfg, header));
        connect(cb, &QCheckBox::toggled, this, [this, header](bool on) {
            QJsonObject c = m_win->currentConfig();
            c[QuoteColumns::configKeyFor(header)] = on;
            m_win->applyConfig(c);
        });
        grid->addWidget(cb, row, col);
        if (++col == 3) {
            col = 0;
            ++row;
        }
    }
    lay->addWidget(flags);

    connect(m_interval, &QComboBox::currentIndexChanged, this, [this](int) {
        QJsonObject c = m_win->currentConfig();
        c[QStringLiteral("refresh_seconds")] = m_interval->currentData().toInt();
        m_win->applyConfig(c);
    });
    return page;
}

QWidget* SettingsDialog::buildAppearanceTab() {
    const QJsonObject cfg = m_win->currentConfig();
    auto* page = new QWidget();
    auto* lay = new QVBoxLayout(page);

    auto* table = new QGroupBox(QStringLiteral("表格外观"), page);
    auto* tableGrid = new QGridLayout(table);
    m_tableHeader = new QCheckBox(QStringLiteral("显示表头"), table);
    m_tableHeader->setChecked(cfg.value(QStringLiteral("header_visible")).toBool(false));
    m_tableGrid = new QCheckBox(QStringLiteral("显示网格"), table);
    m_tableGrid->setChecked(cfg.value(QStringLiteral("grid_visible")).toBool(false));
    tableGrid->addWidget(m_tableHeader, 0, 0);
    tableGrid->addWidget(m_tableGrid, 0, 1);
    lay->addWidget(table);

    auto* color = new QGroupBox(QStringLiteral("颜色与透明度"), page);
    auto* cg = new QGridLayout(color);
    m_defaultColor = new QCheckBox(QStringLiteral("默认颜色"), color);
    m_defaultColor->setChecked(cfg.value(QStringLiteral("default_color")).toBool(false));
    m_fgButton = new QPushButton(QStringLiteral("文字颜色…"), color);
    m_fgButton->setEnabled(!m_defaultColor->isChecked());
    m_bgButton = new QPushButton(QStringLiteral("背景颜色…"), color);
    const QJsonObject bg = cfg.value(QStringLiteral("bg")).toObject();
    m_bgAlpha = new QSlider(Qt::Horizontal, color);
    m_bgAlpha->setRange(0, 100);
    m_bgAlpha->setMinimumWidth(150);
    m_bgAlpha->setValue(int(qRound(bg.value(QStringLiteral("a")).toInt(191) / 2.55)));
    m_winOpacity = new QSlider(Qt::Horizontal, color);
    m_winOpacity->setRange(20, 100);
    m_winOpacity->setMinimumWidth(150);
    m_winOpacity->setValue(cfg.value(QStringLiteral("opacity_pct")).toInt(90));
    cg->addWidget(m_defaultColor, 0, 0);
    cg->addWidget(m_fgButton, 0, 1);
    cg->addWidget(m_bgButton, 0, 2);
    cg->addWidget(new QLabel(QStringLiteral("背景不透明度"), color), 1, 0);
    cg->addWidget(m_bgAlpha, 1, 1, 1, 2);
    cg->addWidget(new QLabel(QStringLiteral("整体不透明度"), color), 2, 0);
    cg->addWidget(m_winOpacity, 2, 1, 1, 2);
    lay->addWidget(color);

    auto* font = new QGroupBox(QStringLiteral("字体与行距"), page);
    auto* fg = new QGridLayout(font);
    m_fontFamily = new QComboBox(font);
    m_fontFamily->setMinimumWidth(200);
    static const QStringList families = QFontDatabase::families();
    m_fontFamily->addItems(families);
    m_fontFamily->setCurrentText(cfg.value(QStringLiteral("font_family")).toString());
    m_fontSize = new QSlider(Qt::Horizontal, font);
    m_fontSize->setRange(8, 15);
    m_fontSize->setMinimumWidth(150);
    m_fontSize->setValue(cfg.value(QStringLiteral("font_size")).toInt(10));
    m_lineSpacing = new QSlider(Qt::Horizontal, font);
    m_lineSpacing->setRange(0, 20);
    m_lineSpacing->setMinimumWidth(150);
    m_lineSpacing->setValue(cfg.value(QStringLiteral("line_extra_px")).toInt(1));
    fg->addWidget(new QLabel(QStringLiteral("字体"), font), 0, 0);
    fg->addWidget(m_fontFamily, 0, 1);
    fg->addWidget(new QLabel(QStringLiteral("字号"), font), 1, 0);
    fg->addWidget(m_fontSize, 1, 1);
    fg->addWidget(new QLabel(QStringLiteral("行距"), font), 2, 0);
    fg->addWidget(m_lineSpacing, 2, 1);
    lay->addWidget(font);

    auto update = [this](const QString& key, const QJsonValue& value) {
        QJsonObject c = m_win->currentConfig();
        c[key] = value;
        m_win->applyConfig(c);
    };
    connect(m_tableHeader, &QCheckBox::toggled, this,
            [update](bool on) { update(QStringLiteral("header_visible"), on); });
    connect(m_tableGrid, &QCheckBox::toggled, this,
            [update](bool on) { update(QStringLiteral("grid_visible"), on); });
    connect(m_defaultColor, &QCheckBox::toggled, this, [this, update](bool on) {
        m_fgButton->setEnabled(!on);
        update(QStringLiteral("default_color"), on);
    });
    connect(m_bgAlpha, &QSlider::valueChanged, this, [this, update](int v) {
        QJsonObject c = m_win->currentConfig();
        QJsonObject bgc = c.value(QStringLiteral("bg")).toObject();
        bgc[QStringLiteral("a")] = int(qRound(v * 2.55));
        c[QStringLiteral("bg")] = bgc;
        m_win->applyConfig(c);
        Q_UNUSED(update);
    });
    connect(m_winOpacity, &QSlider::valueChanged, this,
            [update](int v) { update(QStringLiteral("opacity_pct"), v); });
    connect(m_fontFamily, &QComboBox::currentTextChanged, this,
            [update](const QString& f) { update(QStringLiteral("font_family"), f); });
    connect(m_fontSize, &QSlider::valueChanged, this,
            [update](int v) { update(QStringLiteral("font_size"), v); });
    connect(m_lineSpacing, &QSlider::valueChanged, this,
            [update](int v) { update(QStringLiteral("line_extra_px"), v); });
    connect(m_fgButton, &QPushButton::clicked, this, &SettingsDialog::pickForeground);
    connect(m_bgButton, &QPushButton::clicked, this, &SettingsDialog::pickBackground);
    return page;
}

QWidget* SettingsDialog::buildGeneralTab() {
    const QJsonObject cfg = m_win->currentConfig();
    auto* page = new QWidget();
    auto* lay = new QVBoxLayout(page);

    auto* startOnBoot = new QCheckBox(QStringLiteral("开机启动"), page);
    startOnBoot->setChecked(cfg.value(QStringLiteral("start_on_boot")).toBool(false));
    lay->addWidget(startOnBoot);

    auto makeScheduleRow = [&](const QString& title, const QString& modeKey, const QString& startKey,
                               const QString& endKey, QComboBox*& modeOut, QTimeEdit*& startOut,
                               QTimeEdit*& endOut) -> QWidget* {
        auto* group = new QGroupBox(title, page);
        auto* g = new QGridLayout(group);
        modeOut = new QComboBox(group);
        modeOut->addItem(QStringLiteral("一直"), QStringLiteral("always"));
        modeOut->addItem(QStringLiteral("开盘时段 (9:15–15:00)"), QStringLiteral("market"));
        modeOut->addItem(QStringLiteral("自定义"), QStringLiteral("custom"));
        const int mi = modeOut->findData(
            cfg.value(modeKey).toString(QStringLiteral("always")));
        modeOut->setCurrentIndex(mi >= 0 ? mi : 0);

        startOut = new QTimeEdit(QTime::fromString(
            cfg.value(startKey).toString(QStringLiteral("09:15")), QStringLiteral("HH:mm")), group);
        endOut = new QTimeEdit(QTime::fromString(
            cfg.value(endKey).toString(QStringLiteral("15:00")), QStringLiteral("HH:mm")), group);
        startOut->setDisplayFormat(QStringLiteral("HH:mm"));
        endOut->setDisplayFormat(QStringLiteral("HH:mm"));

        g->addWidget(modeOut, 0, 0, 1, 2);
        g->addWidget(new QLabel(QStringLiteral("起"), group), 1, 0);
        g->addWidget(startOut, 1, 1);
        g->addWidget(new QLabel(QStringLiteral("止"), group), 2, 0);
        g->addWidget(endOut, 2, 1);

        auto syncEnabled = [modeOut, startOut, endOut] {
            const bool custom = modeOut->currentData().toString() == QStringLiteral("custom");
            startOut->setEnabled(custom);
            endOut->setEnabled(custom);
        };
        syncEnabled();

        connect(modeOut, &QComboBox::currentIndexChanged, this,
                [this, modeKey, startKey, endKey, modeOut, startOut, endOut, syncEnabled](int) {
                    syncEnabled();
                    QJsonObject c = m_win->currentConfig();
                    c[modeKey] = modeOut->currentData().toString();
                    c[startKey] = startOut->time().toString(QStringLiteral("HH:mm"));
                    c[endKey] = endOut->time().toString(QStringLiteral("HH:mm"));
                    m_win->applyConfig(c);
                });
        auto onTimeChanged = [this, modeKey, startKey, endKey, modeOut, startOut, endOut] {
            QJsonObject c = m_win->currentConfig();
            c[modeKey] = modeOut->currentData().toString();
            c[startKey] = startOut->time().toString(QStringLiteral("HH:mm"));
            c[endKey] = endOut->time().toString(QStringLiteral("HH:mm"));
            m_win->applyConfig(c);
        };
        connect(startOut, &QTimeEdit::timeChanged, this, [onTimeChanged](const QTime&) { onTimeChanged(); });
        connect(endOut, &QTimeEdit::timeChanged, this, [onTimeChanged](const QTime&) { onTimeChanged(); });
        return group;
    };

    lay->addWidget(makeScheduleRow(QStringLiteral("显示时段"), QStringLiteral("show_mode"),
                                   QStringLiteral("show_start"), QStringLiteral("show_end"),
                                   m_showMode, m_showStart, m_showEnd));
    lay->addWidget(makeScheduleRow(QStringLiteral("请求时段"), QStringLiteral("fetch_mode"),
                                   QStringLiteral("fetch_start"), QStringLiteral("fetch_end"),
                                   m_fetchMode, m_fetchStart, m_fetchEnd));

    auto* hotkeyGroup = new QGroupBox(QStringLiteral("快捷键"), page);
    auto* hg = new QHBoxLayout(hotkeyGroup);
    hg->addWidget(new QLabel(QStringLiteral("显示/隐藏浮窗："), hotkeyGroup));
    m_hotkeyEdit = new QKeySequenceEdit(hotkeyGroup);
    m_hotkeyEdit->setMaximumSequenceLength(1);
    m_hotkeyEdit->setKeySequence(
        QKeySequence(cfg.value(QStringLiteral("hotkey")).toString(QStringLiteral("Ctrl+Alt+F"))));
    hg->addWidget(m_hotkeyEdit);
    hg->addStretch(1);
    lay->addWidget(hotkeyGroup);

    connect(m_hotkeyEdit, &QKeySequenceEdit::editingFinished, this, [this] {
        const QString seq =
            m_hotkeyEdit->keySequence().toString(QKeySequence::PortableText);
        if (seq.isEmpty()) return;
        QJsonObject c = m_win->currentConfig();
        c[QStringLiteral("hotkey")] = seq;
        m_win->applyConfig(c);
    });

    auto* iconGroup = new QGroupBox(QStringLiteral("程序图标"), page);
    auto* ih = new QHBoxLayout(iconGroup);
    m_icon = new QComboBox(iconGroup);
    m_icon->addItem(QStringLiteral("默认"), QStringLiteral("default"));
    m_icon->addItem(QStringLiteral("系统：计算机"), QStringLiteral("std:computer"));
    m_icon->addItem(QStringLiteral("系统：网络"), QStringLiteral("std:network"));
    m_icon->addItem(QStringLiteral("系统：文件夹"), QStringLiteral("std:folder"));
    m_icon->addItem(QStringLiteral("系统：文件"), QStringLiteral("std:file"));
    m_icon->addItem(QStringLiteral("系统：回收站"), QStringLiteral("std:trash"));
    const QString curIcon = cfg.value(QStringLiteral("app_icon")).toString(QStringLiteral("default"));
    int iconIdx = m_icon->findData(curIcon);
    if (iconIdx < 0 && !curIcon.isEmpty() && curIcon != QStringLiteral("default")) {
        m_icon->addItem(QStringLiteral("自定义"), curIcon);
        iconIdx = m_icon->count() - 1;
    }
    m_icon->setCurrentIndex(iconIdx >= 0 ? iconIdx : 0);
    auto* pick = new QPushButton(QStringLiteral("自定义图标…"), iconGroup);
    ih->addWidget(m_icon);
    ih->addWidget(pick);
    lay->addWidget(iconGroup);
    lay->addStretch(1);

    connect(startOnBoot, &QCheckBox::toggled, this, [this](bool on) {
        QJsonObject c = m_win->currentConfig();
        c[QStringLiteral("start_on_boot")] = on;
        m_win->applyConfig(c);
    });
    connect(m_icon, &QComboBox::currentIndexChanged, this, [this](int) {
        QJsonObject c = m_win->currentConfig();
        c[QStringLiteral("app_icon")] = m_icon->currentData().toString();
        m_win->applyConfig(c);
    });
    connect(pick, &QPushButton::clicked, this, &SettingsDialog::pickIcon);
    return page;
}

void SettingsDialog::pickForeground() {
    const QColor c = QColorDialog::getColor(Qt::white, this, QStringLiteral("选择文字颜色"));
    if (!c.isValid()) return;
    QJsonObject cfg = m_win->currentConfig();
    cfg[QStringLiteral("fg")] = c.name(QColor::HexRgb);
    m_win->applyConfig(cfg);
}

void SettingsDialog::pickBackground() {
    const QColor c = QColorDialog::getColor(Qt::black, this, QStringLiteral("选择背景颜色"));
    if (!c.isValid()) return;
    QJsonObject cfg = m_win->currentConfig();
    QJsonObject bg = cfg.value(QStringLiteral("bg")).toObject();
    bg[QStringLiteral("r")] = c.red();
    bg[QStringLiteral("g")] = c.green();
    bg[QStringLiteral("b")] = c.blue();
    cfg[QStringLiteral("bg")] = bg;
    m_win->applyConfig(cfg);
}

void SettingsDialog::pickIcon() {
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("选择图标文件"),
                                                      QString(), QStringLiteral("图标文件 (*.ico)"));
    if (path.isEmpty()) return;
    m_icon->addItem(QStringLiteral("自定义"), path);
    m_icon->setCurrentIndex(m_icon->count() - 1);
}
