# 名称全称 / 自定义映射 + 表头排序 设计

- 日期：2026-09-17
- 状态：待用户审阅
- 目标：浮窗「名称」列支持不截断与用户自定义别名；支持点击表头按任意指标排序（实时重排）
- 前置：本项目 Qt/C++ 重构已完成（见 `2026-09-11-stockwidget-qt-refactor-design.md`）

---

## 1. 背景与问题

当前（重构完成后的代码）存在三个缺口：

1. `QuoteParser` 支持 `name_length` 截断（`0` = 不截断），但**设置面板没有对应控件**，该键在 UI 上不可达。
2. 名称只能来自新浪行情（简称，如「浦发银行」），用户**无法给某个代码起自己的名字**（例如区分同名股票、标注持仓）。
3. `QuoteModel` 是纯 `QAbstractTableModel`，**未实现 `sort()`**，浮窗也未开启排序；行序恒等于自选列表顺序，**无法按涨跌幅排行**。

需求原文（用户）：

> 名称支持全称，也可以自定义映射
> 支持按涨跌幅进行排序

澄清结论（逐条确认）：

| 问题 | 结论 |
|---|---|
| 「全称」含义 | 方案 C：行情名称**不截断**（并补上 `name_length` 的 UI），核心是**自定义别名映射** |
| 映射编辑入口 | 方案 D：设置面板自选列表（双击对话框）+ 浮窗行内右键，**两者都要**，不做多行文本框 |
| 排序入口 | 方案 A：**点击表头**三态循环（降 → 升 → 取消），状态**持久化**，且**不改动** `codes` 数组顺序 |
| 刷新时是否重排 | 方案 A：**每次刷新实时重排**，不做「锁定排序」开关 |

---

## 2. 范围与约束

**做**

- 设置面板补「名称显示」下拉（全称 / 1~4 字）
- `name_map` 别名映射：设置面板双击对话框 + 浮窗行内右键两个入口
- 表头点击排序（三态、持久化、实时重排、`QHeaderView` 指示器）
- `ConfigStore::normalize()` 对新增键的容错
- 单元测试 + README 更新

**不做（YAGNI）**

- 真正的公司全称（需要额外数据源，本次不做）
- 多行文本形式的映射批量编辑器
- 拼音排序、多级排序、自定义排序键
- 「锁定排序」开关（用户选择了纯实时重排）
- 右键菜单里的「排序」子菜单（表头隐藏时的备选入口）

**约束**

- 向后兼容：老配置缺新键 ⇒ 行为与现在完全一致
- 无第三方依赖；仅 Qt（`Core Gui Widgets Network Core5Compat`，测试加 `Test`）
- 不改动 `QuoteModel` 的 diff 缓存结构与 `KLineDelegate`

---

## 3. 配置 schema

配置文件 `%APPDATA%\StockWidget\SW_config.json`。

| 键 | 类型 | 默认 | 含义 |
|---|---|---|---|
| `name_map` | object | `{}` | 别名映射，如 `{"sh600000": "浦发(老仓)"}`；**值空字符串视为删除该项** |
| `name_length` | int | `0` | 已有键，本次补 UI；`0` = 全称（不截断），`1~4` = 截断字数 |
| `sort_key` | string | `""` | 排序键（见 §4.1）；`""` = 不排序，回到自选顺序 |
| `sort_asc` | bool | `false` | `false` = 降序（涨跌幅排行榜的默认预期），`true` = 升序 |

**`ConfigStore::normalize()` 兜底规则**

- `name_map` 非 object ⇒ 整键丢弃（回到 `{}`）
- `name_map` 的 key 经 `StockCode::normalize()` 归一化（`600000` → `sh600000`）；归一化失败或 key 重复 ⇒ 丢弃后者；值 trim 后为空 ⇒ 删除该项
- `name_map` 中**不在 `codes` 里的条目保留**（用户可能只是暂时取消勾选）
- `sort_key` 不在 `QuoteSort::sortableKeys()` 白名单内 ⇒ 置为 `""`（视同不排序）
- `sort_asc` 归一化为 bool；`sort_key` 为空时其值无意义但保留

---

## 4. 模块设计

### 4.1 排序键（独立命名空间）

**不能复用列 config key**：买一与卖一共用 `b1s1_visible`，无法区分。因此引入独立排序键（11 个，无 K 线）：

```
code, name, price, change, change_pct, buy1, sell1, commi, vol, amount, avg
```

列 → 键的映射加在 `QuoteColumns`（单一事实来源）：

```cpp
struct Entry { const char* header; const char* key; const char* sortKey; bool right; bool colored; };
QString QuoteColumns::sortKeyFor(const QString& header);   // 新增；K 线返回空
```

调整后 `kEntries` 对应关系：

| 表头 | config key | sortKey |
|---|---|---|
| 代码 | `code_visible` | `code` |
| 名称 | `name_visible` | `name` |
| 现价 | `price_visible` | `price` |
| 涨跌值 | `change_visible` | `change` |
| 涨跌幅 | `change_pct_visible` | `change_pct` |
| 买一 | `b1s1_visible` | `buy1` |
| 卖一 | `b1s1_visible` | `sell1` |
| 委比 | `commi_visible` | `commi` |
| 成交量 | `vol_visible` | `vol` |
| 成交额 | `amount_visible` | `amount` |
| 均价 | `avg_visible` | `avg` |
| K线 | `kline_visible` | *(空 → 不可排序)* |

### 4.2 `data/QuoteSort.{h,cpp}`（新，纯函数，无 UI 依赖）

```cpp
namespace QuoteSort {
QStringList sortableKeys();                                   // 白名单
void sortQuotes(QVector<Quote>& quotes, const QString& key, bool asc);  // key 非法时原序返回
}
```

- 数值列按 `Quote` 的 `double` 字段比较；`code`/`name` 用 `QString::compare`（确定性、可单测；不做拼音排序）
- `std::stable_sort`：**同值行永远保持输入顺序（= 自选顺序）**，避免相同涨跌幅的行随机跳动
- `0` / 缺失值**正常参与**比较（例如早盘买一为 0，降序时落在末尾），不做特殊处理 —— 行为可预测
- 非法/未知 key ⇒ 不排序（返回原序）

### 4.3 `data/NameAlias.{h,cpp}`（新，纯函数）

```cpp
namespace NameAlias {
QString aliasFor(const QJsonObject& nameMap, const QString& code);   // 无别名返回空串
void applyAliases(QVector<Quote>& quotes, const QJsonObject& nameMap);
}
```

规则：

- **别名优先于行情名称**：命中且非空 ⇒ 覆盖 `q.name`
- **别名不受 `name_length` 截断**：截断只作用于行情名称（别名是用户自己写的，砍掉不合理）
- 别名缺失/为空 ⇒ 用行情名称
- 查表键：先对 `code` 做 `StockCode::normalize()`，归一化失败则用原样字符串（容错，兼容手改配置写入的裸代码）

### 4.4 数据流（顺序固定）

```
SinaQuoteSource → QuoteParser（截断行情名称）
                → NameAlias::applyAliases()
                → QuoteSort::sortQuotes()      ← FloatWindow::onQuotesReady
                → QuoteModel::setQuotes()
```

`FloatWindow::onQuotesReady(const QVector<Quote>& quotes)` 内做一次值拷贝（排序需要可变版本），依次调用 `applyAliases()` 与 `sortQuotes()`，再交给模型。保证排序用的是**用户最终看到的值** —— 即按「名称」列排序时比较的是**别名**（而非行情名称）。

---

## 5. UI 交互

### 5.1 设置面板 ·「显示数据」页

「显示指标」组下方新增一行 `名称显示` 下拉：`全称 / 1 字 / 2 字 / 3 字 / 4 字` ⇒ 写 `name_length`（0~4），改动即生效（沿用现有「即改即存即生效」模式）。

### 5.2 设置面板 ·「自选列表」

- 列表项文本改为 `sh600000  浦发(老仓)`；无别名时仍为 `sh600000`。勾选框、上移、下移、删除行为不变。
- **双击**任意项 ⇒ 弹出「股票」小对话框：
  - `代码`：预填，确定时经 `StockCode::normalize()` 校验；非法则提示且**不关闭**对话框
  - `自定义名称`：预填当前别名，留空 ⇒ 删除别名
- 相应**去掉 `ItemIsEditable`**（避免 QListWidget 原生就地编辑与双击对话框抢事件）；代码仍可修改，只是走对话框
- 确定后写 `codes` / `name_map` 并 `applyConfig()`

### 5.3 浮窗行内右键

`FloatWindow::showContextMenu(globalPos)` 中：若右键落在表格某一行（按全局坐标反算 `viewport` 坐标 → `indexAt().row() >= 0`），在菜单**顶部**插入「自定义名称…」，其余菜单项原样保留。
点击后 `QInputDialog` 预填当前别名，清空 ⇒ 恢复行情名称；确定后写 `name_map`，**立即**对当前数据重跑 `applyAliases()`（被 `sort_key == "name"` 激活时同时重排）并重绘，不等下一次刷新。

### 5.4 表头点击排序

- `setSectionsClickable(true)`
- 三态循环：**不排序 → 降序 → 升序 → 不排序**；每次写 `sort_key` / `sort_asc`，立即重排 + 更新指示器 + 重绘
- 指示器：处于排序态时 `setSortIndicator(col, asc ? Ascending : Descending)` + `setSortIndicatorShown(true)`；回到「不排序」时 `setSortIndicatorShown(false)`（不用非法的 `-1` section 去清指示器）
- K 线列不可排序：点击后排序状态不变，也不显示指示器
- 表头隐藏（`header_visible=false`）时排序入口一并隐藏，但**排序状态保留**（仍按上次键排序），靠右键「显示表头」恢复入口
- **事件过滤器改动**：在 `horizontalHeader` 上的左键按下/移动/释放不再进入拖动分支，直接放行给 `QHeaderView`（由 `sectionClicked` 处理）；表头区域双击也不触发「单击隐藏」。表头以外的拖动/单击隐藏/双击隐藏/右键转发逻辑**逐条保持原样**

---

## 6. 性能与回归面

- 每次刷新多一次 O(n log n) 排序（自选 < 50 行 ⇒ 微秒级）
- 行序变化时，现有逐单元格 diff 自然退化为**一次整表 `dataChanged`**；无 `beginResetModel`、无 `resizeColumnsToContents`（`m_columnWidthsFrozen` 逻辑不受影响）
- 风险集中在**事件过滤器**：仅新增「表头区域直接放行」一条分支；其余分支不动，并靠现有 `tests/test_ui.cpp` 回归
- 拖动过程中若恰好发生刷新，行会重排（与需求确认的"实时重排"一致），不做特殊处理

---

## 7. 测试

| 文件 | 内容 |
|---|---|
| `tests/test_sorter.cpp`（新） | 各数值列降/升序、`code`/`name` 排序、`stable_sort` 同值保序、`0` 值边界、非法 key 返回原序、`sortableKeys()` 白名单 |
| `tests/test_namealias.cpp`（新） | 别名优先、空值回落、`name_map` 键归一化（`600000` 亦命中 `sh600000`）、**别名不被 `name_length` 截断**（parser + alias 组合用例） |
| `tests/test_config.cpp`（补） | `name_map` 类型/键/空值清理、重复键、`sort_key` 白名单回退、老配置缺键时行为不变 |
| `tests/test_ui.cpp`（补） | 表头点击三态循环 + 指示器状态、右键「自定义名称…」写回 `name_map`、设置列表双击对话框提交路由（沿用现有 offscreen 非阻塞写法） |

---

## 8. 收尾

- README 补三处说明：名称显示长度、自定义名称映射、点击表头排序（含三态与持久化）
- 本设计文档提交 git；实现完成后 `scripts\test.cmd` 全绿、`scripts\build.cmd` 通过
- 发布：沿用 `.github/workflows/release.yml`，推送 `v*` 标签自动出包（本次为功能新增，建议 `v1.6.0`）

---

## 9. 风险与对策

| 风险 | 对策 |
|---|---|
| 表头点击与窗口拖动冲突 | 事件过滤器中表头区域整体放行；`test_ui.cpp` 覆盖「表头点击不进入拖动」 |
| 行序频繁变化导致重绘开销 | 上述 diff 退化仅一次整表 `dataChanged`；`stable_sort` 保证同值不抖 |
| 手改配置写入脏 `name_map` | `normalize()` 归一化 + 丢弃非法项；查表时双路查找 |
| 别名被截断导致困惑 | 明确规则「截断只作用于行情名称」，并在测试中固化 |
