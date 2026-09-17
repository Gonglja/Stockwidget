#include "ui/CustomConfigDialog.h"
#include "data/QuoteColumns.h"
#include "data/QuoteSort.h"
#include "data/StockCode.h"
#include "ui/FloatWindow.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QPointer>
#include <QShowEvent>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {

QPointer<CustomConfigDialog> g_instance;

QString labelForSortKey(const QString& key) {
    for (const QString& header : QuoteColumns::allHeaders())
        if (QuoteColumns::sortKeyFor(header) == key) return header;
    return key;
}

}  // namespace

CustomConfigDialog::CustomConfigDialog(FloatWindow* win, QWidget* parent)
    : QDialog(parent), m_win(win) {
    setObjectName(QStringLiteral("customConfigDialog"));
    setWindowTitle(QStringLiteral("自定义配置"));
    setModal(false);

    auto* root = new QVBoxLayout(this);

    auto* sortGroup = new QGroupBox(QStringLiteral("排序"), this);
    auto* sortLay = new QHBoxLayout(sortGroup);
    sortLay->addWidget(new QLabel(QStringLiteral("指标"), sortGroup));
    m_sortKey = new QComboBox(sortGroup);
    m_sortKey->setObjectName(QStringLiteral("sortKeyCombo"));
    m_sortKey->addItem(QStringLiteral("不排序（自选顺序）"), QString());
    for (const QString& key : QuoteSort::sortableKeys())
        m_sortKey->addItem(labelForSortKey(key), key);
    sortLay->addWidget(m_sortKey);
    sortLay->addWidget(new QLabel(QStringLiteral("方向"), sortGroup));
    m_sortDir = new QComboBox(sortGroup);
    m_sortDir->setObjectName(QStringLiteral("sortDirCombo"));
    m_sortDir->addItem(QStringLiteral("降序"), false);
    m_sortDir->addItem(QStringLiteral("升序"), true);
    sortLay->addWidget(m_sortDir);
    sortLay->addStretch(1);
    root->addWidget(sortGroup);

    auto* aliasGroup = new QGroupBox(QStringLiteral("自定义名称（留空 = 使用行情名称）"), this);
    auto* aliasLay = new QVBoxLayout(aliasGroup);
    m_aliasTable = new QTableWidget(0, 2, aliasGroup);
    m_aliasTable->setObjectName(QStringLiteral("aliasTable"));
    m_aliasTable->setHorizontalHeaderLabels({QStringLiteral("代码"), QStringLiteral("自定义名称")});
    m_aliasTable->verticalHeader()->setVisible(false);
    m_aliasTable->horizontalHeader()->setStretchLastSection(true);
    m_aliasTable->setColumnWidth(0, 120);
    aliasLay->addWidget(m_aliasTable);
    root->addWidget(aliasGroup, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    root->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);

    connect(m_sortKey, &QComboBox::currentIndexChanged, this, [this](int) { commitSort(); });
    connect(m_sortDir, &QComboBox::currentIndexChanged, this, [this](int) { commitSort(); });
    connect(m_aliasTable, &QTableWidget::itemChanged, this, [this](QTableWidgetItem*) {
        if (!m_loading) commitAliases();
    });

    resize(420, 340);
    rebuildSortRow();
    rebuildAliasTable();
}

CustomConfigDialog* CustomConfigDialog::showFor(FloatWindow* win, QWidget* parent) {
    if (!g_instance) g_instance = new CustomConfigDialog(win, parent);
    g_instance->show();
    g_instance->raise();
    g_instance->activateWindow();
    return g_instance;
}

void CustomConfigDialog::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
    rebuildSortRow();
    rebuildAliasTable();
}

void CustomConfigDialog::rebuildSortRow() {
    if (!m_win) return;
    const QJsonObject cfg = m_win->currentConfig();
    const QString key = cfg.value(QStringLiteral("sort_key")).toString();
    m_loading = true;
    const int keyIndex = m_sortKey->findData(key);
    m_sortKey->setCurrentIndex(keyIndex >= 0 ? keyIndex : 0);
    const int dirIndex = m_sortDir->findData(cfg.value(QStringLiteral("sort_asc")).toBool(false));
    m_sortDir->setCurrentIndex(dirIndex >= 0 ? dirIndex : 0);
    m_sortDir->setEnabled(!key.isEmpty());
    m_loading = false;
}

void CustomConfigDialog::rebuildAliasTable() {
    if (!m_win) return;
    const QJsonObject cfg = m_win->currentConfig();
    QStringList codes;
    for (const QJsonValue& v : cfg.value(QStringLiteral("codes")).toArray()) {
        const auto code = StockCode::normalize(v.toString());
        if (code && !codes.contains(*code)) codes << *code;
    }
    const QJsonObject nameMap = cfg.value(QStringLiteral("name_map")).toObject();

    m_loading = true;
    m_aliasTable->setRowCount(0);
    for (const QString& code : codes) {
        const int row = m_aliasTable->rowCount();
        m_aliasTable->insertRow(row);
        auto* codeItem = new QTableWidgetItem(code);
        codeItem->setFlags(codeItem->flags() & ~Qt::ItemIsEditable);
        m_aliasTable->setItem(row, 0, codeItem);
        m_aliasTable->setItem(row, 1, new QTableWidgetItem(nameMap.value(code).toString()));
    }
    m_loading = false;
}

void CustomConfigDialog::commitSort() {
    if (m_loading || !m_win) return;
    const QString key = m_sortKey->currentData().toString();
    m_sortDir->setEnabled(!key.isEmpty());
    QJsonObject c = m_win->currentConfig();
    c[QStringLiteral("sort_key")] = key;
    c[QStringLiteral("sort_asc")] = m_sortDir->currentData().toBool();
    m_win->applyConfig(c);
}

void CustomConfigDialog::commitAliases() {
    if (m_loading || !m_win) return;
    // 与既有映射合并：只改表格里出现的代码，其余条目（例如已不在自选列表中的）保留
    QJsonObject nameMap = m_win->currentConfig().value(QStringLiteral("name_map")).toObject();
    for (int row = 0; row < m_aliasTable->rowCount(); ++row) {
        const QTableWidgetItem* codeItem = m_aliasTable->item(row, 0);
        if (!codeItem) continue;
        const auto code = StockCode::normalize(codeItem->text());
        if (!code) continue;
        const QTableWidgetItem* aliasItem = m_aliasTable->item(row, 1);
        const QString alias = aliasItem ? aliasItem->text().trimmed() : QString();
        if (alias.isEmpty()) nameMap.remove(*code);
        else nameMap.insert(*code, alias);
    }
    QJsonObject c = m_win->currentConfig();
    c[QStringLiteral("name_map")] = nameMap;
    m_win->applyConfig(c);
}
