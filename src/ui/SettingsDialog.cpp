#include "ui/SettingsDialog.h"
#include "data/QuoteColumns.h"
#include "data/StockCode.h"
#include "data/StockSuggestSource.h"
#include "ui/FloatWindow.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QKeySequence>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QListWidget>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QTabWidget>
#include <QTime>
#include <QTimeEdit>
#include <QTimer>
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
    auto* outer = new QVBoxLayout(group);

    // 搜索行：输入代码（可不带前缀）或名称模糊搜索
    auto* searchRow = new QHBoxLayout();
    m_searchEdit = new QLineEdit(group);
    m_searchEdit->setObjectName(QStringLiteral("searchEdit"));
    m_searchEdit->setPlaceholderText(QStringLiteral("输入代码（可不带前缀）或名称模糊搜索…"));
    auto* btnAddSearch = new QPushButton(QStringLiteral("添加"), group);
    btnAddSearch->setFixedWidth(60);
    searchRow->addWidget(m_searchEdit, 1);
    searchRow->addWidget(btnAddSearch);
    outer->addLayout(searchRow);

    // 联想结果（选中即加入）
    m_suggestList = new QListWidget(group);
    m_suggestList->setMaximumHeight(120);
    m_suggestList->setVisible(false);
    outer->addWidget(m_suggestList);

    auto* h = new QHBoxLayout();
    m_codeList = new QListWidget(group);
    m_codeList->setObjectName(QStringLiteral("codeList"));
    m_codeList->setFixedWidth(150);

    const QJsonObject cfg = m_win->currentConfig();
    const QJsonArray codes = cfg.value(QStringLiteral("codes")).toArray();
    const QJsonArray checked = cfg.value(QStringLiteral("checked_codes")).toArray();
    const QJsonObject nameMap = cfg.value(QStringLiteral("name_map")).toObject();
    for (const QJsonValue& v : codes) {
        const QString code = v.toString();
        const QString alias = nameMap.value(code).toString();
        auto* item = new QListWidgetItem(
            alias.isEmpty() ? code : QStringLiteral("%1  %2").arg(code, alias), m_codeList);
        item->setFlags((item->flags() | Qt::ItemIsUserCheckable) & ~Qt::ItemIsEditable);
        item->setData(Qt::UserRole, code);
        item->setData(Qt::UserRole + 1, alias);
        item->setCheckState(checked.contains(v) ? Qt::Checked : Qt::Unchecked);
    }

    auto commit = [this] { commitCodes(); };
    auto addCode = [this, commit](const QString& codeIn) -> bool {
        const auto n = StockCode::normalize(codeIn);
        if (!n) return false;
        for (int i = 0; i < m_codeList->count(); ++i) {
            if (m_codeList->item(i)->data(Qt::UserRole).toString() == *n) {
                m_codeList->item(i)->setCheckState(Qt::Checked);
                m_codeList->setCurrentRow(i);
                commit();
                return true;
            }
        }
        auto* it = new QListWidgetItem(*n, m_codeList);
        it->setFlags((it->flags() | Qt::ItemIsUserCheckable) & ~Qt::ItemIsEditable);
        it->setData(Qt::UserRole, *n);
        it->setCheckState(Qt::Checked);
        m_codeList->setCurrentItem(it);
        commit();
        return true;
    };
    auto submitSearch = [this, addCode] {
        const QString text = m_searchEdit->text();
        bool ok = false;
        if (StockCode::normalize(text)) {
            ok = addCode(text);
        } else {
            const QListWidgetItem* sel = m_suggestList->currentItem();
            if (!sel && m_suggestList->count() > 0) sel = m_suggestList->item(0);
            if (sel) ok = addCode(sel->data(Qt::UserRole).toString());
        }
        if (ok) {
            m_searchEdit->clear();
            m_suggestList->clear();
            m_suggestList->setVisible(false);
        }
    };

    m_suggest = new StockSuggestSource(this);
    m_suggestDebounce = new QTimer(this);
    m_suggestDebounce->setSingleShot(true);
    m_suggestDebounce->setInterval(300);
    connect(m_suggestDebounce, &QTimer::timeout, this,
            [this] { m_suggest->query(m_searchEdit->text()); });
    connect(m_searchEdit, &QLineEdit::textEdited, this, [this](const QString& t) {
        m_suggestDebounce->stop();
        if (t.trimmed().isEmpty() || StockCode::normalize(t)) {
            m_suggestList->clear();
            m_suggestList->setVisible(false);
            return;
        }
        m_suggestDebounce->start();
    });
    connect(m_suggest, &StockSuggestSource::suggestionsReady, this,
            [this](const QVector<StockSuggestion>& items) {
                m_suggestList->clear();
                for (const StockSuggestion& s : items) {
                    auto* it = new QListWidgetItem(
                        QStringLiteral("%1  %2").arg(s.name, s.code), m_suggestList);
                    it->setData(Qt::UserRole, s.code);
                }
                m_suggestList->setVisible(!items.isEmpty());
                if (items.size() > 0) m_suggestList->setCurrentRow(0);
            });
    connect(m_suggest, &StockSuggestSource::error, this, [this](const QString&) {
        m_suggestList->clear();
        m_suggestList->setVisible(false);
    });
    connect(m_searchEdit, &QLineEdit::returnPressed, this, [submitSearch] { submitSearch(); });
    connect(btnAddSearch, &QPushButton::clicked, this, [submitSearch] { submitSearch(); });
    connect(m_suggestList, &QListWidget::itemActivated, this,
            [this, addCode](QListWidgetItem* it) {
                if (!it) return;
                if (addCode(it->data(Qt::UserRole).toString())) {
                    m_searchEdit->clear();
                    m_suggestList->clear();
                    m_suggestList->setVisible(false);
                }
            });

    auto* btnCol = new QVBoxLayout();
    auto addBtn = [&](const QString& text, std::function<void()> fn) {
        auto* b = new QPushButton(text, group);
        b->setFixedWidth(60);
        connect(b, &QPushButton::clicked, this, fn);
        btnCol->addWidget(b);
    };
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
    outer->addLayout(h, 1);
    lay->addWidget(group);
    connect(m_codeList, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem* it) { editCodeItem(it); });
    connect(m_codeList, &QListWidget::itemChanged, this, [commit](QListWidgetItem*) { commit(); });
    return page;
}

void SettingsDialog::commitCodes() {
    if (!m_codeList) return;
    QStringList list, checkedList;
    QJsonObject nameMap = m_win->currentConfig().value(QStringLiteral("name_map")).toObject();
    for (int i = 0; i < m_codeList->count(); ++i) {
        const QListWidgetItem* it = m_codeList->item(i);
        const auto n = StockCode::normalize(it->data(Qt::UserRole).toString());
        if (!n) continue;
        list << *n;
        if (it->checkState() == Qt::Checked) checkedList << *n;
        const QString alias = it->data(Qt::UserRole + 1).toString().trimmed();
        if (alias.isEmpty()) nameMap.remove(*n);
        else nameMap.insert(*n, alias);
    }
    QJsonObject c = m_win->currentConfig();
    c[QStringLiteral("codes")] = QJsonArray::fromStringList(list);
    c[QStringLiteral("checked_codes")] = QJsonArray::fromStringList(checkedList);
    c[QStringLiteral("name_map")] = nameMap;
    m_win->applyConfig(c);
}

bool SettingsDialog::applyCodeEdit(const QString& code, const QString& alias, QListWidgetItem* item) {
    if (!m_codeList) return false;
    const auto n = StockCode::normalize(code);
    if (!n) return false;

    if (item) item->setData(Qt::UserRole, *n);
    if (!item) {
        for (int i = 0; i < m_codeList->count(); ++i) {
            if (m_codeList->item(i)->data(Qt::UserRole).toString() == *n) {
                item = m_codeList->item(i);
                break;
            }
        }
    }
    const QSignalBlocker blocker(m_codeList);  // 避免 setText 触发 itemChanged 递归提交
    if (!item) {
        item = new QListWidgetItem(*n, m_codeList);
        item->setFlags((item->flags() | Qt::ItemIsUserCheckable) & ~Qt::ItemIsEditable);
        item->setCheckState(Qt::Checked);
        item->setData(Qt::UserRole, *n);
    }
    const QString value = alias.trimmed();
    item->setData(Qt::UserRole + 1, value);
    item->setText(value.isEmpty() ? *n : QStringLiteral("%1  %2").arg(*n, value));
    m_codeList->setCurrentItem(item);
    commitCodes();
    return true;
}

void SettingsDialog::editCodeItem(QListWidgetItem* item) {
    if (!item || !m_codeList) return;

    QDialog dlg(this);
    dlg.setObjectName(QStringLiteral("codeEditDialog"));
    dlg.setWindowTitle(QStringLiteral("股票"));
    auto* form = new QFormLayout(&dlg);
    auto* codeEdit = new QLineEdit(item->data(Qt::UserRole).toString(), &dlg);
    codeEdit->setObjectName(QStringLiteral("codeEdit"));
    auto* aliasEdit = new QLineEdit(item->data(Qt::UserRole + 1).toString(), &dlg);
    aliasEdit->setObjectName(QStringLiteral("aliasEdit"));
    aliasEdit->setPlaceholderText(QStringLiteral("留空 = 使用行情名称"));
    form->addRow(QStringLiteral("代码"), codeEdit);
    form->addRow(QStringLiteral("自定义名称"), aliasEdit);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, [&dlg, codeEdit] {
        if (!StockCode::normalize(codeEdit->text())) {
            QMessageBox::warning(&dlg, QStringLiteral("代码无效"),
                                 QStringLiteral("请输入 6 位代码或带前缀代码（如 sh600000）"));
            return;
        }
        dlg.accept();
    });

    if (dlg.exec() != QDialog::Accepted) return;
    applyCodeEdit(codeEdit->text(), aliasEdit->text(), item);
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

    auto* nameRow = new QWidget(page);
    auto* nameLay = new QHBoxLayout(nameRow);
    nameLay->addWidget(new QLabel(QStringLiteral("名称显示"), nameRow));
    m_nameLength = new QComboBox(nameRow);
    m_nameLength->setObjectName(QStringLiteral("nameLengthCombo"));
    m_nameLength->addItem(QStringLiteral("全称"), 0);
    for (int n = 1; n <= 4; ++n)
        m_nameLength->addItem(QStringLiteral("%1 字").arg(n), n);
    const int nameIdx = m_nameLength->findData(cfg.value(QStringLiteral("name_length")).toInt(0));
    m_nameLength->setCurrentIndex(nameIdx >= 0 ? nameIdx : 0);
    nameLay->addWidget(m_nameLength);
    nameLay->addStretch(1);
    lay->addWidget(nameRow);

    connect(m_nameLength, &QComboBox::currentIndexChanged, this, [this](int) {
        QJsonObject c = m_win->currentConfig();
        c[QStringLiteral("name_length")] = m_nameLength->currentData().toInt();
        m_win->applyConfig(c);
    });

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

    auto* edgeRow = new QWidget(page);
    auto* edgeLay = new QHBoxLayout(edgeRow);
    auto* edgeHide = new QCheckBox(QStringLiteral("贴边隐藏（鼠标划过显示）"), edgeRow);
    edgeHide->setChecked(cfg.value(QStringLiteral("edge_hide")).toBool(false));
    m_edgeSide = new QComboBox(edgeRow);
    m_edgeSide->addItem(QStringLiteral("方向：自动（就近）"), QStringLiteral("auto"));
    m_edgeSide->addItem(QStringLiteral("方向：左"), QStringLiteral("left"));
    m_edgeSide->addItem(QStringLiteral("方向：右"), QStringLiteral("right"));
    m_edgeSide->addItem(QStringLiteral("方向：上"), QStringLiteral("top"));
    m_edgeSide->addItem(QStringLiteral("方向：下"), QStringLiteral("bottom"));
    const int sideIdx =
        m_edgeSide->findData(cfg.value(QStringLiteral("edge_side")).toString(QStringLiteral("auto")));
    m_edgeSide->setCurrentIndex(sideIdx >= 0 ? sideIdx : 0);
    m_edgeSide->setEnabled(edgeHide->isChecked());
    edgeLay->addWidget(edgeHide);
    edgeLay->addWidget(m_edgeSide);
    edgeLay->addStretch(1);
    lay->addWidget(edgeRow);
    connect(edgeHide, &QCheckBox::toggled, this, [this](bool on) {
        if (m_edgeSide) m_edgeSide->setEnabled(on);
        QJsonObject c = m_win->currentConfig();
        c[QStringLiteral("edge_hide")] = on;
        m_win->applyConfig(c);
    });
    connect(m_edgeSide, &QComboBox::currentIndexChanged, this, [this](int) {
        QJsonObject c = m_win->currentConfig();
        c[QStringLiteral("edge_side")] = m_edgeSide->currentData().toString();
        m_win->applyConfig(c);
    });

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
