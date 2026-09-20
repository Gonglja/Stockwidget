# 设置面板自选列表「名称」自主取数 设计

- 日期：2026-09-20
- 状态：待用户审阅（设计已口头确认）
- 目标：设置 →「自选列表」的名称列**不再依赖浮窗的请求时段**；打开时/添加时各请求一次，联想结果自带的名称直接复用
- 前置：`2026-09-20-watchlist-columns-info-label-tray-icon-design.md`（两列改造已完成）

---

## 1. 背景与问题

两列改造后，名称列的数据来源只有一个：`FloatWindow::quoteNameFor()`。这带来两个问题：

1. **受请求时段限制**：`fetch_mode = "market"`（09:15–15:00）时盘外浮窗根本不发请求 → 设置面板名称列全空（用户实测反馈）。
2. **新加代码拿不到名字**：`m_rawQuotes` 里没有刚添加的代码，要等浮窗下一次刷新（且必须处于请求时段）才会出现。

另外发现一处现成的浪费：搜索联想接口 `StockSuggestSource` 返回的 `StockSuggestion` **已经带名称**，但 `SettingsDialog` 只取了 `code`，名称被丢掉。

## 2. 范围与约束

**做**

- 设置面板自带 `SinaQuoteSource`（独立于浮窗、**不看请求时段**），用默认格式选项 ⇒ 拿到**全称**（不受「名称显示长度」截断）
- 触发点：打开设置（`showEvent`）批量拉一次；添加代码时拉一次；切回「自选列表」页再补一次
- 会话内名称缓存 `QHash<code, name>`；优先级：**别名 > 本页抓到的行情名 > 浮窗已有行情名**
- 联想结果里自带的名称直接复用（**零请求**）
- 请求队列：请求进行中新增的代码排队，上一批返回后立刻再发
- `SinaQuoteSource` 支持注入 `QNetworkAccessManager`（测试用不联网替身）

**不做（YAGNI）**

- 不改浮窗的请求时段/取数逻辑，浮窗依旧只看「请求时段」
- 不把设置面板抓到的名字回写给浮窗或 `name_map`
- 不新增配置键、不改 schema
- 不做失败弹窗、不做后台重试轰炸、不落盘缓存名称

**约束**

- 向后兼容：老配置行为不变
- 仅用 Qt；测试不联网（注入替身）
- 名称请求失败静默（名称列留空，下次切页/添加时自然重试）

## 3. 配置 schema

**无变更**。

## 4. 模块设计

### 4.1 `data/SinaQuoteSource`（可注入 NAM，测试用）

```cpp
explicit SinaQuoteSource(QObject* parent = nullptr);
void setNetworkAccessManager(QNetworkAccessManager* nam);  // 不拥有；nullptr = 恢复内置
```

- 内部 `QNetworkAccessManager m_nam;`（值成员）改为 `QNetworkAccessManager* m_nam` + 自建 `m_ownedNam`（父对象为自己）。
- 生产代码零行为变化（`FloatWindow` 仍 `new SinaQuoteSource(this)`）。

### 4.2 `ui/SettingsDialog`（名称解析与缓存）

```cpp
QHash<QString, QString> m_nameCache;    // code -> 本页抓到的行情名
SinaQuoteSource* m_names = nullptr;     // 构造函数里 new（不是 buildCodesTab），供测试替换 NAM
QStringList m_nameQueue;                // 待请求代码
bool m_nameFetching = false;

QString nameFor(const QString& code) const;              // 缓存 → 浮窗 quoteNameFor() → 空
void refreshItemName(QTreeWidgetItem* it);               // 别名 > nameFor()，只改第 1 列
QStringList currentCodes() const;                        // 树里的全部代码
SinaQuoteSource* nameSource() const;                     // 测试注入点
```

- `refreshNameColumn()`（既有的 `quotesUpdated` 回填）改为遍历调用 `refreshItemName()`，语义不变。
- 名称列的全部写入点（初始填充、`addCode`、`applyCodeEdit`、`refreshNameColumn`）统一走 `refreshItemName()`。

### 4.3 请求队列与触发点

```cpp
void ensureNamesFor(const QStringList& codes);  // 过滤：非法 / 别名 / 已有名（缓存或浮窗）/ 已在队列 → 入队
void pumpNameQueue();                           // 忙或空则返回；否则整批 fetch() 一次
void applyNames(const QVector<Quote>& quotes);  // 写入缓存 → refreshNameColumn()
```

- `m_nameFetching` 标志规避 `SinaQuoteSource::fetch()` 的「忙则静默丢弃」（那会把新代码吞掉）。
- `error` 回调：`m_nameFetching = false`，**丢弃该批**（静默），继续 pump 队列中剩下的。
- 触发点：

| 时机 | 位置 |
|---|---|
| 打开设置 | `SettingsDialog::showEvent()` → `ensureNamesFor(currentCodes())` |
| 切回「自选列表」页 | `QTabWidget::currentChanged` 的 lambda：`index == 0 && isVisible()` 时补一次 |
| 添加/改代码 | `addCode()` / `applyCodeEdit()` 末尾 `ensureNamesFor({code})` |

> 首次请求刻意放在 `showEvent` 而不是构造函数：构造期发请求会让测试来不及替换 NAM（且会让单元测试真的联网）。

### 4.4 联想名称复用 + `addCode` 抽成成员方法

- 现在 `addCode` 是 `buildCodesTab()` 里的 lambda；抽成私有成员方法以便测试直接调用：

```cpp
bool addCode(const QString& codeIn, const QString& knownName = QString());
```

- 联想列表项同时存名称：`it->setData(Qt::UserRole + 1, s.name)`。
- `submitSearch()` / `itemActivated` 调用 `addCode(code, name)`；`knownName` 非空 ⇒ 直接写进 `m_nameCache`，名称立即显示且**不产生请求**。

## 5. UI 交互

- 打开设置：名称列先用「别名 / 浮窗已抓到的名字」立刻填充，随后（联网返回）把仍为空的行补上。
- 手输 `600519` 回车添加：名称列先空，一次批量请求返回后显示「贵州茅台」。
- 从联想列表点选/回车添加：名称**立即**显示（来自联想结果，无请求）。
- 请求失败：名称列留空，无任何弹窗；切回该标签页或再次添加会重试。

## 6. 性能与回归面

- 请求次数：打开设置最多 1 次（仅当存在无名代码）；切页/添加时才增量 1 次；都只请求缺失项。
- 新浪接口是批量接口（`list=a,b,c`），10 个代码仍是 1 次请求。
- 回归面：浮窗取数、请求时段、`name_map` 写回、两列布局、`CustomConfigDialog` 均不改动。
- 测试期网络：新用例注入 `StubNam`（不发真实请求）；既有 `test_ui` 用例不触发名称请求（对话框不 show 或不含无名代码时 `ensureNamesFor` 为空操作）——注意 `showEvent` 触发点在既有用例里也会跑，因此**所有**涉及 `dlg.show()` 的既有用例都必须注入替身。

> ⚠️ 这是本设计最大的测试风险：`settingsListShowsAliasAndApplyCodeEditWritesMap` 等既有用例会 `dlg.show()`。统一在测试里给 `SettingsDialog` 注入 `StubNam`（封装一个 `makeDialog()` 测试辅助函数）。

## 7. 测试（`tests/test_ui.cpp`）

测试替身（新增，约 40 行）：

```cpp
class StubReply : public QNetworkReply { ... };   // 内存 body，autoFinish 可控
class StubNam : public QNetworkAccessManager {
public:
    QByteArray body;       // GBK 字节
    QStringList urls;      // 记录请求 URL
    bool manual = false;   // true = 不自动 finished，等 flush()
    void flush();
protected:
    QNetworkReply* createRequest(Operation, const QNetworkRequest&, QIODevice*) override;
};
```

新增/改造用例：

1. `settingsNamesFetchedOnOpen`：无别名代码在打开设置后被**一次批量**请求（URL 同时含多个代码），返回后名称列显示全称。
2. `settingsNamesSkipAliasedRows`：设了 `name_map` 的代码不出现在请求 URL 里。
3. `settingsNameRequestedOnManualAdd`：`applyCodeEdit("600519", "")` 触发一次请求该代码，返回后名称列显示。
4. `settingsNameFromSuggestionNeedsNoRequest`：`addCode("sh600519", "贵州茅台")` 后名称立即显示，且 `urls` 数量不增加。
5. `settingsNamesQueuedWhileFetching`：`manual = true` 时打开设置（第一次请求挂起）→ 添加新代码 → `flush()` 后应发出**第二次**请求且包含新代码。
6. 既有用例改造：统一通过 `makeDialog(&w, &nam)` 注入替身（否则会真联网）。

## 8. 收尾

- `README.md`：设置面板/名称列说明补「打开设置即拉一次名称，不受请求时段限制；联想结果自带名称即时显示」
- `TODO.md`：执行记录 + 偏差
- 全量 `ctest` + 视觉/手工验证（盘外打开设置，名称列应能显示）

## 9. 风险与对策

| 风险 | 对策 |
|---|---|
| 既有用例 `dlg.show()` 会真联网 | 统一 `makeDialog()` 辅助注入 `StubNam`；`ensureNamesFor` 对空队列是空操作 |
| `QNetworkReply` 替身写不对（`bytesAvailable`/`readAll` 拿不到 body） | 替身实现 `readData` + `bytesAvailable`；若 `readAll()` 为空则改为先 `setFinished` 再投递 `finished` 的写法（实现阶段用测试快速迭代） |
| 队列被 `fetch()` 的「忙则丢弃」吞掉 | 用 `m_nameFetching` 在应用层拦截，忙时不 fetch |
| 盘外启动时打开设置会多发一次请求 | 用户明确要求「不受请求时段限制」；只请求缺失项且结果缓存到会话结束 |
| 名称突发变化（如改名）不刷新 | 会话内缓存 + 切页触发可刷新；不追求实时 |
