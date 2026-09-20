# 自选列表两列 + 非交易时段提示 + 托盘图标即时生效 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 设置面板「自选列表」改成代码 / 名称两列；浮窗在「非请求时段且无任何数据」时显示「非交易时段」；改程序图标（以及快捷键、开机启动）在设置里改完立即生效，不再等下一次事件。

**Architecture:** 三个改动彼此独立，均落在既有文件内，**不新增配置键、不改 schema**。①`SettingsDialog` 的 `m_codeList` 由 `QListWidget` 换成 `QTreeWidget`（两列，objectName 保持 `codeList`），名称列数据来自 `FloatWindow` 新暴露的 `quoteNameFor()`；`FloatWindow` 新增 `quotesUpdated()` 信号供对话框在行情到达时回填名称列（**名称列只读**，别名仍由既有双击对话框写入 `name_map`）。②`FloatWindow` 新增灰色 `infoLabel`，只在 `refreshNow()` 判定「非请求时段 + `m_rawQuotes` 为空」时显示。③`FloatWindow::applyConfig()` 补齐 `app_icon` / `hotkey` / `start_on_boot` 三个旧值快照，有变化就 `notifyChanged()`，经既有 `configChanged → Application::saveConfig()` 链路立即落地。

**Tech Stack:** Qt 6.8+（Core/Gui/Widgets/Network/Core5Compat/Test，MSVC 2022 + CMake + Ninja），QTest 单元测试（`tests/test_ui.cpp`），无第三方依赖。

**规格来源：** `docs/superpowers/specs/2026-09-20-watchlist-columns-info-label-tray-icon-design.md`

**统一命令（本文所有构建/测试步骤都用这两条，不必手工 vcvars）：**

```
scripts\build.cmd        # 配置 + 编译（含全部测试目标）
scripts\test.cmd         # 运行全部 ctest
```

只想跑 `test_ui` 里的单个用例时（`test_ui` 需要真实窗口，本机直接跑即可）：

```
scripts\build.cmd
set "PATH=C:/1/Qt/6.11.1/msvc2022_64/bin;%PATH%"
build\test_ui.exe -v1 infoLabelShowsNonTradingHintOutsideFetchWindow
```

> 若本机 Qt 路径不同，先 `set QT_DIR=D:/Qt/...`（CI 里是 `C:/Qt/6.8.3/msvc2022_64`）。

---

### Task 1: FloatWindow 暴露行情名 + 行情到达信号

**Files:**
- Modify: `src/ui/FloatWindow.h`（public 方法 + signals）
- Modify: `src/ui/FloatWindow.cpp`（`quoteNameFor()` 实现、`onQuotesReady()` 末尾 emit）
- Test: `tests/test_ui.cpp`

- [ ] **Step 1: 写失败测试**

`tests/test_ui.cpp` 顶部 include 区追加一行（与 `tests/test_model.cpp` 的写法一致）：

```cpp
#include <QSignalSpy>
```

在 `setAliasUpdatesModelImmediately()` 用例函数之后追加两个用例：

```cpp
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
```

- [ ] **Step 2: 构建并确认失败**

Run: `scripts\build.cmd`
Expected: 编译失败，错误形如 `error C2039: "quoteNameFor": 不是 "FloatWindow" 的成员` 与 `"quotesUpdated": 不是 "FloatWindow" 的成员`

- [ ] **Step 3: 实现**

`src/ui/FloatWindow.h`：在 `void openCustomConfig();` 之后、`signals:` 之前插入声明，并在 `signals:` 段加信号：

```cpp
    void openCustomConfig();  // 右下角配置按钮/测试入口

    // 该代码当前的行情名称（原始名，不含用户别名）；无数据或未命中返回空串。
    QString quoteNameFor(const QString& code) const;

signals:
    void configChanged();
    void quotesUpdated();  // 行情刷新完成（供设置面板回填名称列）
```

`src/ui/FloatWindow.cpp`：在 `void FloatWindow::refreshNow()` 之前插入实现：

```cpp
QString FloatWindow::quoteNameFor(const QString& code) const {
    const auto n = StockCode::normalize(code);
    if (!n) return QString();
    for (const Quote& q : m_rawQuotes)
        if (q.code == *n) return q.name;
    return QString();
}

```

并把 `onQuotesReady()` 改为（只加最后一行）：

```cpp
void FloatWindow::onQuotesReady(const QVector<Quote>& quotes) {
    m_errorLabel->setVisible(false);
    m_rawQuotes = quotes;
    redisplayLastQuotes();
    emit quotesUpdated();
}
```

> 注意：`m_rawQuotes` 存的是**原始行情名**（别名是在 `redisplayLastQuotes()` 的拷贝上叠加的），所以 `quoteNameFor()` 不会被别名污染。

- [ ] **Step 4: 运行测试确认通过**

Run:
```
scripts\build.cmd
set "PATH=C:/1/Qt/6.11.1/msvc2022_64/bin;%PATH%"
build\test_ui.exe -v1 quoteNameForReturnsRawNameOnly quotesReadyEmitsQuotesUpdated
```
Expected: `Totals: 2 passed, 0 failed, 0 skipped`

- [ ] **Step 5: 提交**

```
git add src/ui/FloatWindow.h src/ui/FloatWindow.cpp tests/test_ui.cpp
git commit -m "feat(ui): FloatWindow 暴露 quoteNameFor() 与 quotesUpdated() 信号"
```

---

### Task 2: 浮窗「非交易时段」提示

**Files:**
- Modify: `src/ui/FloatWindow.h`（`m_infoLabel` + `setInfoText()`）
- Modify: `src/ui/FloatWindow.cpp`（构造、事件过滤列表、`refreshNow()`、`onQuotesReady()`、`setInfoText()`）
- Test: `tests/test_ui.cpp`

- [ ] **Step 1: 写失败测试**

`tests/test_ui.cpp` 顶部 include 区追加 `#include <QLabel>`。

在 Task 1 追加的 `quotesReadyEmitsQuotesUpdated()` 之后追加：

```cpp
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
```

- [ ] **Step 2: 构建并确认失败**

Run: `scripts\build.cmd`
Expected: 构建通过但用例失败 —— `build\test_ui.exe -v1 infoLabelShowsNonTradingHintOutsideFetchWindow` 报 `info` 为空指针（`QLabel*` 找不到 `infoLabel`）

- [ ] **Step 3: 实现**

`src/ui/FloatWindow.h`：

1) 私有方法区追加一行：

```cpp
    void setInfoText(const QString& text);
```

2) 成员区 `QLabel* m_errorLabel = nullptr;` 之后追加：

```cpp
    QLabel* m_infoLabel = nullptr;
```

`src/ui/FloatWindow.cpp` 构造函数：在 `m_vbox->addWidget(m_errorLabel);` 之后插入：

```cpp
    m_infoLabel = new QLabel(QString(), m_panel);
    m_infoLabel->setObjectName(QStringLiteral("infoLabel"));
    m_infoLabel->setStyleSheet(QStringLiteral("color: #9aa0a6; padding: 2px 4px;"));
    m_infoLabel->setVisible(false);
    m_vbox->addWidget(m_infoLabel);
```

并把事件过滤列表改为（把 `m_infoLabel` 也纳入，保持一致的可拖动/单击隐藏行为）：

```cpp
    QWidget* filtered[] = {m_panel, m_table, m_errorLabel, m_infoLabel, m_table->viewport(),
                           m_table->horizontalHeader()};
```

在 `void FloatWindow::refreshNow()` 之前（紧跟 Task 1 的 `quoteNameFor()` 之后）插入实现：

```cpp
void FloatWindow::setInfoText(const QString& text) {
    if (!m_infoLabel) return;
    const bool wasVisible = m_infoLabel->isVisible();
    m_infoLabel->setText(text);
    m_infoLabel->setVisible(!text.isEmpty());
    if (wasVisible != m_infoLabel->isVisible()) refitSize();
}

```

把 `refreshNow()` 整体替换为：

```cpp
void FloatWindow::refreshNow() {
    if (!isVisible()) return;
    if (!inFetchWindow()) {
        // 非请求时段：保留上次数据；从没拿到过数据时给提示，避免出现一个空框。
        if (m_rawQuotes.isEmpty()) setInfoText(QStringLiteral("非交易时段"));
        return;
    }
    setInfoText(QString());  // 进入请求时段：先撤掉提示
    m_source->fetch(m_checkedCodes);
}
```

把 `onQuotesReady()` 改为：

```cpp
void FloatWindow::onQuotesReady(const QVector<Quote>& quotes) {
    m_errorLabel->setVisible(false);
    setInfoText(QString());
    m_rawQuotes = quotes;
    redisplayLastQuotes();
    emit quotesUpdated();
}
```

- [ ] **Step 4: 运行测试确认通过**

Run:
```
scripts\build.cmd
set "PATH=C:/1/Qt/6.11.1/msvc2022_64/bin;%PATH%"
build\test_ui.exe -v1 infoLabelShowsNonTradingHintOutsideFetchWindow infoLabelSilentInsideFetchWindow
```
Expected: `Totals: 2 passed, 0 failed, 0 skipped`

- [ ] **Step 5: 提交**

```
git add src/ui/FloatWindow.h src/ui/FloatWindow.cpp tests/test_ui.cpp
git commit -m "feat(ui): 非请求时段且无数据时显示「非交易时段」提示"
```

---

### Task 3: 改程序图标/快捷键/开机启动立即生效

**Files:**
- Modify: `src/ui/FloatWindow.cpp`（`applyConfig()`）
- Test: `tests/test_ui.cpp`

- [ ] **Step 1: 写失败测试**

在 `infoLabelSilentInsideFetchWindow()` 之后追加：

```cpp
    void applicationLevelConfigChangesEmitImmediately() {
        ProbeWindow w(baseConfig());
        w.show();
        QTest::qWait(150);
        QSignalSpy spy(&w, &FloatWindow::configChanged);

        QJsonObject c = w.currentConfig();
        c["app_icon"] = "std:file";
        w.applyConfig(c);
        QTest::qWait(30);
        QCOMPARE(spy.count(), 1);  // 改图标要立即通知 Application 落地

        w.applyConfig(w.currentConfig());  // 同值：不应再 emit
        QTest::qWait(30);
        QCOMPARE(spy.count(), 1);

        QJsonObject h = w.currentConfig();
        h["hotkey"] = "Ctrl+Alt+G";
        w.applyConfig(h);
        QTest::qWait(30);
        QCOMPARE(spy.count(), 2);

        QJsonObject b = w.currentConfig();
        b["start_on_boot"] = true;
        w.applyConfig(b);
        QTest::qWait(30);
        QCOMPARE(spy.count(), 3);
    }
```

- [ ] **Step 2: 构建并确认失败**

Run:
```
scripts\build.cmd
set "PATH=C:/1/Qt/6.11.1/msvc2022_64/bin;%PATH%"
build\test_ui.exe -v1 applicationLevelConfigChangesEmitImmediately
```
Expected: 失败，`Compared values are not the same` / `Actual: 0, Expected: 1`

- [ ] **Step 3: 实现**

`src/ui/FloatWindow.cpp::applyConfig()`：在既有 `const bool oldGearVisible = m_gearVisible;` 这一行**之后**插入三个快照：

```cpp
    const QString oldAppIcon = m_appIcon;
    const QString oldHotkey = m_hotkey;
    const bool oldStartOnBoot = m_startOnBoot;
```

在 `applyConfig()` 末尾（既有 `m_gearVisible != oldGearVisible` 那一整块**之后**）追加：

```cpp
    // 图标/快捷键/开机启动的落地入口在 Application::saveConfig()，
    // 这里必须立即 notifyChanged()，否则用户改完要等下一次任意事件才生效。
    if (m_appIcon != oldAppIcon || m_hotkey != oldHotkey || m_startOnBoot != oldStartOnBoot)
        notifyChanged();
```

> 无递归风险：`notifyChanged()` → `configChanged` → `Application::saveConfig()`，而 `saveConfig()` 不会回调 `applyConfig()`。

- [ ] **Step 4: 运行测试确认通过**

Run:
```
scripts\build.cmd
set "PATH=C:/1/Qt/6.11.1/msvc2022_64/bin;%PATH%"
build\test_ui.exe -v1 applicationLevelConfigChangesEmitImmediately
```
Expected: `Totals: 1 passed, 0 failed, 0 skipped`

- [ ] **Step 5: 提交**

```
git add src/ui/FloatWindow.cpp tests/test_ui.cpp
git commit -m "fix(ui): 图标/快捷键/开机启动改动立即落地（补 notifyChanged）"
```

---

### Task 4: 设置面板「自选列表」改两列（代码 / 名称）

**Files:**
- Modify: `src/ui/SettingsDialog.h`
- Modify: `src/ui/SettingsDialog.cpp`（`buildCodesTab()`、`commitCodes()`、`applyCodeEdit()`、`editCodeItem()`）
- Test: `tests/test_ui.cpp`（改造 2 个既有用例 + 新增 1 个）

- [ ] **Step 1: 写失败测试（含改造既有 2 个用例）**

`tests/test_ui.cpp` 顶部 include 区追加 `#include <QTreeWidget>`（`#include <QListWidget>` 保留，联想列表仍是 `QListWidget`）。

**(a)** 把 `settingsListShowsAliasAndApplyCodeEditWritesMap()` 整体替换为：

```cpp
    void settingsListShowsAliasAndApplyCodeEditWritesMap() {
        ProbeWindow w(baseConfig());
        w.show();
        QTest::qWait(150);

        SettingsDialog dlg(&w, &w);
        dlg.show();
        QTest::qWait(50);

        auto* tree = dlg.findChild<QTreeWidget*>("codeList");
        QVERIFY(tree);
        QCOMPARE(tree->columnCount(), 2);
        QCOMPARE(tree->headerItem()->text(0), QString("代码"));
        QCOMPARE(tree->headerItem()->text(1), QString("名称"));
        QCOMPARE(tree->topLevelItemCount(), 1);
        QCOMPARE(tree->topLevelItem(0)->text(0), QString("sh600000"));
        QVERIFY2(tree->topLevelItem(0)->text(1).isEmpty(), "无别名且无行情时名称列应为空");

        QVERIFY(dlg.applyCodeEdit("600000", "浦发(老仓)"));
        QTest::qWait(30);
        QCOMPARE(tree->topLevelItem(0)->text(1), QString("浦发(老仓)"));
        QCOMPARE(w.currentConfig().value("name_map").toObject().value("sh600000").toString(),
                 QString("浦发(老仓)"));

        QVERIFY(dlg.applyCodeEdit("600000", ""));
        QTest::qWait(30);
        QVERIFY(tree->topLevelItem(0)->text(1).isEmpty());
        QCOMPARE(w.currentConfig().value("name_map").toObject().size(), 0);

        QVERIFY(!dlg.applyCodeEdit("not-a-code", "x"));  // 非法代码被拒
        dlg.close();
    }
```

**(b)** 把 `doubleClickListItemEditsAlias()` 里的列表定位与断言替换（其余不变）：

```cpp
        auto* tree = dlg.findChild<QTreeWidget*>("codeList");
        QVERIFY(tree);
        QVERIFY(tree->topLevelItemCount() == 1);
```

双击的坐标改为：

```cpp
        const QRect rect = tree->visualItemRect(tree->topLevelItem(0), 0);
        QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier, rect.center());
        QTest::mouseDClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier, rect.center());
        QTest::qWait(50);

        QVERIFY(sawDialog);
        QCOMPARE(tree->topLevelItem(0)->text(1), QString("浦发(老仓)"));
        QCOMPARE(w.currentConfig().value("name_map").toObject().value("sh600000").toString(),
                 QString("浦发(老仓)"));
        dlg.close();
```

**(c)** 新增名称回落到行情名的用例：

```cpp
    void settingsListNameColumnFallsBackToQuoteName() {
        ProbeWindow w(baseConfig());
        w.show();
        QTest::qWait(150);
        w.pushQuotes(twoQuotes());  // sh600000 → 浦发银行
        QTest::qWait(30);

        SettingsDialog dlg(&w, &w);
        dlg.show();
        QTest::qWait(50);
        auto* tree = dlg.findChild<QTreeWidget*>("codeList");
        QVERIFY(tree);
        QCOMPARE(tree->topLevelItem(0)->text(1), QString("浦发银行"));  // 无别名 → 行情名

        QVERIFY(dlg.applyCodeEdit("600000", "老仓"));
        QTest::qWait(30);
        QCOMPARE(tree->topLevelItem(0)->text(1), QString("老仓"));      // 别名优先

        QVERIFY(dlg.applyCodeEdit("600000", ""));
        QTest::qWait(30);
        QCOMPARE(tree->topLevelItem(0)->text(1), QString("浦发银行"));  // 清别名 → 回落行情名
        dlg.close();
    }
```

- [ ] **Step 2: 构建并确认失败**

Run: `scripts\build.cmd`
Expected: 构建失败，错误形如 `error C2039: "topLevelItem": 不是 "QListWidget" 的成员`

- [ ] **Step 3: 实现**

`src/ui/SettingsDialog.h`：

1) 前置声明：`class QListWidgetItem;` 之后加：

```cpp
class QTreeWidget;
class QTreeWidgetItem;
```

2) `applyCodeEdit` 签名与 `editCodeItem`、成员类型改为：

```cpp
    // 校验并落库一条「代码 + 别名」；item 为空则按代码查找/新建。非法代码返回 false
    bool applyCodeEdit(const QString& code, const QString& alias, QTreeWidgetItem* item = nullptr);
```

```cpp
    void editCodeItem(QTreeWidgetItem* item);
```

```cpp
    QTreeWidget* m_codeList = nullptr;
```

（`m_suggestList` 仍是 `QListWidget*`，`class QListWidget;` 声明保留。）

`src/ui/SettingsDialog.cpp`：

1) include 区把 `#include <QListWidget>` 之后补 `#include <QHeaderView>` 与 `#include <QTreeWidget>`（`QListWidget` 仍需保留，联想列表在用）。

2) `buildCodesTab()` 里列表构造替换为：

```cpp
    m_codeList = new QTreeWidget(group);
    m_codeList->setObjectName(QStringLiteral("codeList"));
    m_codeList->setColumnCount(2);
    m_codeList->setHeaderLabels({QStringLiteral("代码"), QStringLiteral("名称")});
    m_codeList->setRootIsDecorated(false);
    m_codeList->setUniformRowHeights(true);
    m_codeList->setAllColumnsShowFocus(false);
    m_codeList->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    m_codeList->setColumnWidth(0, 110);
    m_codeList->header()->setSectionResizeMode(1, QHeaderView::Stretch);
```

3) 初始填充循环替换为：

```cpp
    for (const QJsonValue& v : codes) {
        const QString code = v.toString();
        const QString alias = nameMap.value(code).toString();
        auto* item = new QTreeWidgetItem(m_codeList);
        item->setFlags((item->flags() | Qt::ItemIsUserCheckable) & ~Qt::ItemIsEditable);
        item->setData(0, Qt::UserRole, code);
        item->setData(0, Qt::UserRole + 1, alias);
        item->setText(0, code);
        item->setText(1, alias.isEmpty() ? m_win->quoteNameFor(code) : alias);
        item->setCheckState(0, checked.contains(v) ? Qt::Checked : Qt::Unchecked);
    }
```

4) `addCode` lambda 里「已存在」与「新建」两段替换为：

```cpp
        for (int i = 0; i < m_codeList->topLevelItemCount(); ++i) {
            if (m_codeList->topLevelItem(i)->data(0, Qt::UserRole).toString() == *n) {
                m_codeList->topLevelItem(i)->setCheckState(0, Qt::Checked);
                m_codeList->setCurrentItem(m_codeList->topLevelItem(i));
                commit();
                return true;
            }
        }
        auto* it = new QTreeWidgetItem(m_codeList);
        it->setFlags((it->flags() | Qt::ItemIsUserCheckable) & ~Qt::ItemIsEditable);
        it->setData(0, Qt::UserRole, *n);
        it->setText(0, *n);
        it->setText(1, m_win->quoteNameFor(*n));
        it->setCheckState(0, Qt::Checked);
        m_codeList->setCurrentItem(it);
        commit();
        return true;
```

5) 三个按钮闭包替换为（`takeItem` → `takeTopLevelItem`，`insertItem` → `insertTopLevelItem`）：

```cpp
    addBtn(QStringLiteral("删除"), [this, commit] {
        const int r = m_codeList->indexOfTopLevelItem(m_codeList->currentItem());
        if (r >= 0) delete m_codeList->takeTopLevelItem(r);
        commit();
    });
    addBtn(QStringLiteral("上移"), [this, commit] {
        const int r = m_codeList->indexOfTopLevelItem(m_codeList->currentItem());
        if (r > 0) {
            auto* it = m_codeList->takeTopLevelItem(r);
            m_codeList->insertTopLevelItem(r - 1, it);
            m_codeList->setCurrentItem(it);
            commit();
        }
    });
    addBtn(QStringLiteral("下移"), [this, commit] {
        const int r = m_codeList->indexOfTopLevelItem(m_codeList->currentItem());
        if (r >= 0 && r < m_codeList->topLevelItemCount() - 1) {
            auto* it = m_codeList->takeTopLevelItem(r);
            m_codeList->insertTopLevelItem(r + 1, it);
            m_codeList->setCurrentItem(it);
            commit();
        }
    });
```

6) 两个 connect 替换为（`itemChanged`/`itemDoubleClicked` 都多一个 column 形参）：

```cpp
    connect(m_codeList, &QTreeWidget::itemDoubleClicked, this,
            [this](QTreeWidgetItem* it, int) { editCodeItem(it); });
    connect(m_codeList, &QTreeWidget::itemChanged, this,
            [commit](QTreeWidgetItem*, int) { commit(); });
```

7) `commitCodes()` 改为：

```cpp
void SettingsDialog::commitCodes() {
    if (!m_codeList) return;
    QStringList list, checkedList;
    QJsonObject nameMap = m_win->currentConfig().value(QStringLiteral("name_map")).toObject();
    for (int i = 0; i < m_codeList->topLevelItemCount(); ++i) {
        const QTreeWidgetItem* it = m_codeList->topLevelItem(i);
        const auto n = StockCode::normalize(it->data(0, Qt::UserRole).toString());
        if (!n) continue;
        list << *n;
        if (it->checkState(0) == Qt::Checked) checkedList << *n;
        const QString alias = it->data(0, Qt::UserRole + 1).toString().trimmed();
        if (alias.isEmpty()) nameMap.remove(*n);
        else nameMap.insert(*n, alias);
    }
    QJsonObject c = m_win->currentConfig();
    c[QStringLiteral("codes")] = QJsonArray::fromStringList(list);
    c[QStringLiteral("checked_codes")] = QJsonArray::fromStringList(checkedList);
    c[QStringLiteral("name_map")] = nameMap;
    m_win->applyConfig(c);
}
```

8) `applyCodeEdit()` 改为：

```cpp
bool SettingsDialog::applyCodeEdit(const QString& code, const QString& alias, QTreeWidgetItem* item) {
    if (!m_codeList) return false;
    const auto n = StockCode::normalize(code);
    if (!n) return false;

    if (item) item->setData(0, Qt::UserRole, *n);
    if (!item) {
        for (int i = 0; i < m_codeList->topLevelItemCount(); ++i) {
            if (m_codeList->topLevelItem(i)->data(0, Qt::UserRole).toString() == *n) {
                item = m_codeList->topLevelItem(i);
                break;
            }
        }
    }
    const QSignalBlocker blocker(m_codeList);  // 避免 setText 触发 itemChanged 递归提交
    if (!item) {
        item = new QTreeWidgetItem(m_codeList);
        item->setFlags((item->flags() | Qt::ItemIsUserCheckable) & ~Qt::ItemIsEditable);
        item->setCheckState(0, Qt::Checked);
        item->setData(0, Qt::UserRole, *n);
        item->setText(0, *n);
    }
    const QString value = alias.trimmed();
    item->setData(0, Qt::UserRole + 1, value);
    item->setText(1, value.isEmpty() ? m_win->quoteNameFor(*n) : value);
    m_codeList->setCurrentItem(item);
    commitCodes();
    return true;
}
```

9) `editCodeItem()` 里 `item->data(...)` 全部加列号：签名改 `QTreeWidgetItem* item`，两处读取改为 `item->data(0, Qt::UserRole).toString()` 与 `item->data(0, Qt::UserRole + 1).toString()`，`if (!item || !m_codeList) return;` 不变。

- [ ] **Step 4: 运行测试确认通过**

Run:
```
scripts\build.cmd
set "PATH=C:/1/Qt/6.11.1/msvc2022_64/bin;%PATH%"
build\test_ui.exe -v1 settingsListShowsAliasAndApplyCodeEditWritesMap doubleClickListItemEditsAlias settingsListNameColumnFallsBackToQuoteName
```
Expected: `Totals: 3 passed, 0 failed, 0 skipped`

- [ ] **Step 5: 提交**

```
git add src/ui/SettingsDialog.h src/ui/SettingsDialog.cpp tests/test_ui.cpp
git commit -m "feat(ui): 设置面板自选列表改为「代码 / 名称」两列"
```

---

### Task 5: 名称列随行情到达自动回填

**Files:**
- Modify: `src/ui/SettingsDialog.h`（`refreshNameColumn()`）
- Modify: `src/ui/SettingsDialog.cpp`（构造函数连接 `quotesUpdated()` + 实现）
- Test: `tests/test_ui.cpp`

- [ ] **Step 1: 写失败测试**

在 `settingsListNameColumnFallsBackToQuoteName()` 之后追加：

```cpp
    void settingsListNameColumnUpdatesWhenQuotesArrive() {
        ProbeWindow w(baseConfig());
        w.show();
        QTest::qWait(150);

        SettingsDialog dlg(&w, &w);
        dlg.show();
        QTest::qWait(50);
        auto* tree = dlg.findChild<QTreeWidget*>("codeList");
        QVERIFY(tree);
        QVERIFY2(tree->topLevelItem(0)->text(1).isEmpty(), "行情未到时名称列应为空");

        w.pushQuotes(twoQuotes());
        QTest::qWait(30);
        QCOMPARE(tree->topLevelItem(0)->text(1), QString("浦发银行"));  // 无需重开对话框
        dlg.close();
    }
```

- [ ] **Step 2: 构建并确认失败**

Run:
```
scripts\build.cmd
set "PATH=C:/1/Qt/6.11.1/msvc2022_64/bin;%PATH%"
build\test_ui.exe -v1 settingsListNameColumnUpdatesWhenQuotesArrive
```
Expected: 失败，`Compared values are not the same`（实际为空串，期望 `浦发银行`）

- [ ] **Step 3: 实现**

`src/ui/SettingsDialog.h` 私有方法区追加：

```cpp
    void refreshNameColumn();
```

`src/ui/SettingsDialog.cpp`：

1) 构造函数里 `ensureTab(0);` 之前插入连接（成员函数指针才支持 `Qt::UniqueConnection`）：

```cpp
    // 名称列回填：行情到达时刷新（别名优先，无别名用行情名）
    connect(m_win, &FloatWindow::quotesUpdated, this, &SettingsDialog::refreshNameColumn,
            Qt::UniqueConnection);
```

2) 在 `commitCodes()` 之前插入实现：

```cpp
void SettingsDialog::refreshNameColumn() {
    if (!m_codeList) return;
    const QSignalBlocker blocker(m_codeList);  // 只改显示，不能触发 commitCodes()
    for (int i = 0; i < m_codeList->topLevelItemCount(); ++i) {
        QTreeWidgetItem* it = m_codeList->topLevelItem(i);
        if (!it->data(0, Qt::UserRole + 1).toString().trimmed().isEmpty()) continue;  // 别名优先
        it->setText(1, m_win->quoteNameFor(it->data(0, Qt::UserRole).toString()));
    }
}
```

- [ ] **Step 4: 运行测试确认通过**

Run:
```
scripts\build.cmd
set "PATH=C:/1/Qt/6.11.1/msvc2022_64/bin;%PATH%"
build\test_ui.exe -v1 settingsListNameColumnUpdatesWhenQuotesArrive
```
Expected: `Totals: 1 passed, 0 failed, 0 skipped`

- [ ] **Step 5: 提交**

```
git add src/ui/SettingsDialog.h src/ui/SettingsDialog.cpp tests/test_ui.cpp
git commit -m "feat(ui): 设置面板名称列随行情到达自动回填"
```

---

### Task 6: 全量回归 + 文档 + 手工验证

**Files:**
- Modify: `README.md`（设置面板 / 操作速览）
- Modify: `TODO.md`（执行记录）
- Test: 全量 `ctest`

- [ ] **Step 1: 全量测试**

Run: `scripts\test.cmd`
Expected: `100% tests passed, 0 tests failed out of 9`（9 个测试目标，含 `test_ui`）

- [ ] **Step 2: README 更新**

`README.md` 「⚙️ 设置面板」小节的「自选列表」条目替换为：

```markdown
* **自选列表**：**两列显示（代码 / 名称）**，名称列 = 自定义名称优先，没有则显示浮窗当前抓到的行情名（都没有则留空，**只读**）；增/删/改/上移/下移 + 勾选要在浮窗中显示的股票；顶部搜索框支持「不带前缀的代码」直接添加与「名称/代码片段」模糊搜索点选；**双击某项**可同时改代码与该代码的自定义名称（留空 = 使用行情名称）。
```

同小节「常规」条目末尾追加：

```markdown
  * **程序图标、全局快捷键、开机启动改完立即生效**（无需重启、无需等下一次操作）。
```

「🖱️ 操作速览」小节的「系统托盘」条目之后追加一条：

```markdown
* **非交易时段**：若「请求时段」设成 `market`（09:15–15:00），盘外启动且**还没有任何数据**时浮窗显示灰色「非交易时段」提示；一旦请求到数据（或已有上次数据）就只显示数据、不再提示。
```

- [ ] **Step 3: TODO.md 执行记录**

在 `TODO.md` 末尾追加：

```markdown
## 2026-09-20 追加需求

- [x] **1. 探索项目上下文** — 读 FloatWindow/SettingsDialog/Application/ConfigStore + 用户实际配置（`fetch_mode=market`、`code_visible=false`）
- [x] **2. 澄清问题** — ①改的是**设置面板**的自选列表并固定两列；名称列 = 别名优先、回落行情名；②非交易时段提示文案；③「卡顿」= 托盘图标延迟生效；**浮窗继续受「显示指标」开关控制、不改默认值**
- [x] **3-4. 方案与设计** — 三处改动：QTreeWidget 两列 / infoLabel 提示 / applyConfig 补 notifyChanged（已逐节确认）
- [x] **5-7. 设计文档** — docs/superpowers/specs/2026-09-20-watchlist-columns-info-label-tray-icon-design.md；用户确认「可以，实现它」
- [x] **8. 实现计划** — docs/superpowers/plans/2026-09-20-watchlist-columns-info-label-tray-icon.md（6 个 TDD 任务）
- [ ] **9. 实现执行** — 待填写（executing-plans）
```

- [ ] **Step 4: 手工验证（本机，真实窗口）**

Run:
```
scripts\build.cmd
set "PATH=C:/1/Qt/6.11.1/msvc2022_64/bin;%PATH%"
build\StockWidget.exe
```
逐项核对：

1. 设置 →「自选列表」：两列标题为 `代码` / `名称`；有行情的股票名称列显示行情名；双击改名后显示别名；清空别名回落行情名。
2. 把「请求时段」改成 `market` 且当前盘外 → 退出重开程序：浮窗显示灰色「非交易时段」（若已有数据则正常显示数据 + 不提示）。
3. 设置 →「常规」→「程序图标」换一个 →**松开即变**（托盘图标立即刷新，不用点别的地方）。
4. 顺带确认：浮窗「显示指标」里勾上「代码」→ 浮窗出现代码列（默认开关行为未变）。

- [ ] **Step 5: 提交**

```
git add README.md TODO.md
git commit -m "docs: README/TODO 同步（两列自选列表、非交易时段提示、图标即时生效）"
```
