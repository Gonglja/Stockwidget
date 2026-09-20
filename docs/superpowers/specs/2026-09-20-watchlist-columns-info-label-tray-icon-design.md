# 自选列表两列 + 非交易时段提示 + 托盘图标即时生效 设计

- 日期：2026-09-20
- 状态：待用户审阅
- 目标：修 3 个问题 —— ①设置面板自选列表分两列（代码 / 名称）；②非交易时段无数据时给出提示；③改程序图标后托盘立即生效
- 前置：Qt/C++ 重构 + `name_map`/排序功能已完成（见 `2026-09-11-...`、`2026-09-17-name-map-and-sort-design.md`）

---

## 1. 背景与问题

需求原文（用户）：

> 1. 在自选列表中展示两列，一列是代码，另一列是名称
> 2. 显示「非交易时段」提示
> 3. 托盘图标改了后一段时间才生效不是立即生效

澄清结论（逐条确认）：

| 问题 | 结论 |
|---|---|
| 哪里的「自选列表」 | **设置对话框 →「自选列表」页**，不是浮窗 |
| 两列如何控制 | 列表**固定两列**，不需要开关 |
| 名称列内容 | **别名优先，无别名则用浮窗当前已抓到的行情名**，都没有则留空（不发新请求） |
| 浮窗列默认值 | **不改**，浮窗继续受「显示指标」开关控制 |

### 1.1 根因定位（已核对代码）

**问题 2：非交易时段空白窗口**

- `FloatWindow::refreshNow()`：`if (!inFetchWindow()) return;` —— 非请求时段**静默跳过**，不请求也不提示。
- `FloatWindow::redisplayLastQuotes()`：`if (m_rawQuotes.isEmpty()) return;` —— 从没拿到过数据时什么都不画。
- ⇒ `fetch_mode = "market"`（09:15–15:00）时盘外启动，用户看到的是一个**空的圆角框**（连列都没有，因为 `QuoteModel` 行数为 0）。

**问题 3：托盘图标延迟生效**

- `SettingsDialog` 改「程序图标」→ `m_win->applyConfig(c)`；`FloatWindow::applyConfig()` 只更新了成员 `m_appIcon`，**没有 `notifyChanged()`**。
- 真正调 `Application::applyIcon()` 的唯一入口是 `Application::saveConfig()`，它只由 `FloatWindow::configChanged` 驱动。
- ⇒ 图标要等**下一次任意事件**（`hideEvent`、拖拽结束、表头排序、列开关…）触发 `notifyChanged()` 才更新，表现为「过一会儿才生效」。
- 同根因的还有**快捷键**与**开机启动**（都只在 `saveConfig()` 里落地）。

## 2. 范围与约束

**做**

- 设置「自选列表」页：`QListWidget` → `QTreeWidget` 两列（代码 / 名称）
- `FloatWindow` 暴露「当前行情名」查询 + 行情到达信号（供名称列回填）
- `FloatWindow` 新增非交易时段提示标签
- `FloatWindow::applyConfig()` 对 `app_icon` / `hotkey` / `start_on_boot` 变化立即 `notifyChanged()`
- 更新 `tests/test_ui.cpp` 既有 2 个用例 + 新增 5 个用例
- README / TODO 同步

**不做（YAGNI）**

- 不新增任何配置键（不动 schema、不动默认值）
- 不改浮窗列默认开关、不改浮窗表格结构
- 名称列不发起新的网络请求、不做缓存落盘
- 不改「非交易时段是否继续显示上次数据」的既有行为（有数据就继续显示）
- 不改 `CustomConfigDialog`

**约束**

- 向后兼容：老配置行为不变（本次无 schema 变更）
- 仅用 Qt（`Core Gui Widgets Network Core5Compat`，测试加 `Test`）
- 不引入第三方依赖

## 3. 配置 schema

**无变更**。仍然读写现有键：`codes`、`checked_codes`、`name_map`、`app_icon`、`hotkey`、`start_on_boot`、`fetch_mode`、`fetch_start`、`fetch_end`。

## 4. 模块设计

### 4.1 `ui/SettingsDialog` · 自选列表改两列

- `m_codeList` 类型 `QListWidget*` → `QTreeWidget*`，**objectName 保持 `codeList`**（测试与既有引用靠它定位）。
- 列定义：`代码` / `名称`；`setRootIsDecorated(false)`、`setUniformRowHeights(true)`、`setAllColumnsShowFocus(false)`、`header()->setStretchLastSection(true)`。
- 宽度：不再 `setFixedWidth(150)`；第一列 `resizeColumnToContents` 基准 110px，第二列 stretch（对话框 460px 宽下不挤压）。
- item 数据槽沿用：`Qt::UserRole` = 带前缀代码，`Qt::UserRole + 1` = 别名（`commitCodes()` 语义零改动）。
- checkbox 挂在第 0 列（`ItemIsUserCheckable`，沿用现有 flags 计算）。
- 涉及方法：`buildCodesTab()`（构造 + 6 处 `takeItem/insertItem` 改 `takeTopLevelItem/insertTopLevelItem`）、`commitCodes()`、`applyCodeEdit()`、`editCodeItem()`。
- 交互等价：单击勾选 → `itemChanged` → `commitCodes()`；双击任意列 → `editCodeItem()`；上移/下移/删除按钮语义不变。

### 4.2 `ui/FloatWindow` · 暴露行情名给设置面板

新增两个公开成员（`FloatWindow.h`）：

```cpp
QString quoteNameFor(const QString& code) const;   // 无数据/未命中 → 空串
signals: void quotesUpdated();
```

- `quoteNameFor()`：在 `m_rawQuotes` 里按 `code` 精确匹配返回 `name`。注意 `m_rawQuotes` 存的是**原始行情名**（别名是在 `redisplayLastQuotes()` 的拷贝上叠加的），所以别名不会污染这里。
- `quotesUpdated()`：在 `onQuotesReady()` 末尾 emit（在该函数已清错误标签、写完 `m_rawQuotes` 之后）。
- `SettingsDialog::buildCodesTab()` 连接一次 `quotesUpdated()` → 刷新名称列（`Qt::UniqueConnection`，`m_builtTabs` 保证该页只建一次）。

### 4.3 `ui/FloatWindow` · 非交易时段提示

- 新增 `QLabel* m_infoLabel`（objectName `infoLabel`），样式灰色 `#9aa0a6`，布局位置同 `m_errorLabel`（表格上方），默认隐藏。
- 新增私有方法 `void setInfoText(const QString& text)`：文本/可见性变化时 `setText` + `setVisible` + `refitSize()`（只在可见性真的变化时 `refitSize()`，避免每次刷新都重算列宽）。
- `refreshNow()` 改为：

  ```
  if (!isVisible()) return;
  if (!inFetchWindow()) {
      if (m_rawQuotes.isEmpty()) setInfoText(tr("非交易时段"));  // 有上次数据就不提示
      return;
  }
  setInfoText(QString());            // 进入请求时段：清掉提示
  m_source->fetch(m_checkedCodes);
  ```

- `onQuotesReady()`：`setInfoText(QString())`（数据到了就撤提示）。
- 请求时段内但首次数据还没回来 ⇒ **不提示**（避免刚启动误报「非交易时段」）。
- 网络失败仍走既有 `m_errorLabel`（红色），两者互不影响。

### 4.4 `ui/FloatWindow` · `applyConfig()` 立即落地「Application 级」配置

在 `applyConfig()` 里沿用文件既有的 `oldXxx` 比较写法，加三个快照：

```cpp
const QString oldAppIcon = m_appIcon;
const QString oldHotkey = m_hotkey;
const bool oldStartOnBoot = m_startOnBoot;
...
if (m_appIcon != oldAppIcon || m_hotkey != oldHotkey || m_startOnBoot != oldStartOnBoot)
    notifyChanged();
```

- 数据流：`notifyChanged()` → `configChanged` → `Application::saveConfig()` → `applyIcon()` / `applyHotkey()` / `AutoStart::setEnabled()`。**无递归**（`saveConfig()` 不会回调 `applyConfig()`）。
- `Application::saveConfig()` 里 `cfg["app_icon"] = m_iconChoice;` 保留不动（幂等，且能固化「自定义图标路径失效时回退」的既有语义）。

## 5. UI 交互

```
设置 → 自选列表
┌ 自选列表 ──────────────────────────────┐
│ [输入代码或名称模糊搜索…      ] [添加] │
│ (联想结果列表，选中即加入)              │
│ ┌───────────┬───────────────────────┐ │
│ │ 代码      │ 名称                  │ │
│ │ ☑ sh600030│ 浦发(老仓)            │ │  ← 别名优先
│ │ ☑ sz000001│ 平安银行              │ │  ← 回落行情名
│ │ ☑ sh601318│                       │ │  ← 两者都没有 → 空
│ └───────────┴───────────────────────┘ │
│              [删除] [上移] [下移]      │
└────────────────────────────────────────┘
```

- 名称列**只读**；改别名仍走「双击 → 股票对话框（代码 / 自定义名称）」。
- 勾选/取消勾选写 `checked_codes`；顺序调整写 `codes`；别名写 `name_map`（全部即时保存）。

## 6. 性能与回归面

- 名称列刷新只在 `quotesUpdated()`（最多每次刷新一次）触发，遍历 `codes` × `m_rawQuotes`（十数量级），无感。
- 刷新名称列时用 `QSignalBlocker(m_codeList)` 防止 `itemChanged` 递归 `commitCodes()`（`applyCodeEdit()` 已有同样处理）。
- `setInfoText()` 只在文本/可见性变化时 `refitSize()`；非请求时段每 2s 一次的 `refreshNow()` 不会反复重算列宽。
- 回归面：浮窗列开关、排序、别名、`CustomConfigDialog`、贴边隐藏均不涉及；`commitCodes()` 逻辑零改动。

## 7. 测试（`tests/test_ui.cpp`）

既有用例改造：

1. `settingsListShowsAliasAndApplyCodeEditWritesMap` → 改 `findChild<QTreeWidget*>("codeList")`，断言 `topLevelItemCount() == 1`、第 0 列 `sh600000`、第 1 列先空后为别名、清空别名后回落。
2. `doubleClickListItemEditsAlias` → 定位方式由 `list->visualItemRect(list->item(0))` 改为 `tree->visualItemRect(tree->topLevelItem(0), 0)`；双击后仍应写入 `name_map`。

新增用例：

3. `settingsListNameColumnFallsBackToQuoteName`：`pushQuotes(twoQuotes())` → 开设置 → 第 1 列 = `浦发银行`；设别名 → 第 1 列 = 别名；清别名 → 回落 `浦发银行`。
4. `settingsListNameColumnUpdatesWhenQuotesArrive`：先开设置（无数据 ⇒ 第 1 列空）→ `pushQuotes` → 第 1 列自动变 `浦发银行`（覆盖 `quotesUpdated()`）。
5. `infoLabelShowsNonTradingHint`：非请求时段（`baseConfig()` 的 `00:00–00:00`）→ `QLabel#infoLabel` 可见且文本为 `非交易时段`；`pushQuotes` 后隐藏。
6. `infoLabelHiddenInsideFetchWindow`：`fetch_mode = "always"` 且无数据 ⇒ 标签不可见（不误报）。
7. `appIconChangeEmitsConfigChangedImmediately`：`QSignalSpy(&w, &FloatWindow::configChanged)`；改 `app_icon` ⇒ 立即 emit 一次；写同值 ⇒ 不再 emit；`hotkey` / `start_on_boot` 同理。

约定：所有用例沿用 `baseConfig()`（`fetch_mode=custom, 00:00–00:00`）避免真实网络。

## 8. 收尾

- `README.md`：设置说明补「自选列表两列」「非交易时段提示」「图标即改即生效」
- `TODO.md`：追加本次执行记录与偏差
- `docs/superpowers/plans/2026-09-20-<topic>.md`：实现计划（批次 + 检查点）

## 9. 风险与对策

| 风险 | 对策 |
|---|---|
| 名称列依赖浮窗数据，盘外/未请求时为空 | 用户已确认接受；别名仍正常显示 |
| `QTreeWidget` 两列在 460px 对话框内挤压 | 第一列固定 ~110px，第二列 stretch；必要时缩短第一列 |
| 提示标签出现/消失改变窗口尺寸，贴边隐藏（收起态）时位置可能偏 | 收起态下 `collapseToEdge()` 用的是 `m_flushPos`（展开尺寸）语义，仅 `refitSize()` 不重算收起坐标；本次只保证展开态正确，收起态由贴边轮询在恢复时自愈（不新增逻辑） |
| `quotesUpdated` 连接泄漏/重复 | `Qt::UniqueConnection` + `m_builtTabs` 保证单次连接；对话框为进程内单例 |
| 名称列刷新触发 `commitCodes()` 递归 | 刷新时 `QSignalBlocker(m_codeList)` |
