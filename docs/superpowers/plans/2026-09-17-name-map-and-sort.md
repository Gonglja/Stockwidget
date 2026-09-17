# 名称全称 / 自定义映射 + 表头排序 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 浮窗「名称」列支持不截断 + 用户自定义别名映射，并支持点击表头按任意指标排序（实时重排、状态持久化）。

**Architecture:** 排序与别名都是**数据层的纯函数**（`QuoteSort` / `NameAlias`），在 `FloatWindow::onQuotesReady` 里按「解析 → 别名 → 排序 → 交给模型」的固定顺序处理；`QuoteModel` 的 diff 缓存与 `KLineDelegate` 完全不动。UI 侧只在 `FloatWindow` 加表头点击三态循环（事件过滤器中表头区域放行给 `QHeaderView`），在 `SettingsDialog` 加名称下拉与别名编辑入口。

**Tech Stack:** Qt 6.8+（Core/Gui/Widgets/Network/Core5Compat/Test，MSVC 2022 + CMake + Ninja），QTest 单元测试，无第三方依赖。

**规格来源：** `docs/superpowers/specs/2026-09-17-name-map-and-sort-design.md`

**统一命令（本文所有构建/测试步骤都用这两条，不必重复 vcvars 手工设置）：**

```
scripts\build.cmd        # 配置 + 编译（含全部测试目标）
scripts\test.cmd         # 运行全部 ctest
```

只想跑单个测试可执行文件时：

```
scripts\build.cmd
set "PATH=C:/1/Qt/6.11.1/msvc2022_64/bin;%PATH%"
build\test_sorter.exe
```

> 若本机 Qt 路径不同，先 `set QT_DIR=D:/Qt/...`。

---

### Task 1: QuoteColumns 增加排序键映射

**Files:**
- Modify: `src/data/QuoteColumns.h`
- Modify: `src/data/QuoteColumns.cpp`
- Test: `tests/test_model.cpp`

- [ ] **Step 1: 写失败测试**

在 `tests/test_model.cpp` 的 `TestModel` 类里，`colorsBySign()` 之后追加：

```cpp
    void mapsHeadersToSortKeys() {
        QCOMPARE(QuoteColumns::sortKeyFor("代码"), QString("code"));
        QCOMPARE(QuoteColumns::sortKeyFor("名称"), QString("name"));
        QCOMPARE(QuoteColumns::sortKeyFor("现价"), QString("price"));
        QCOMPARE(QuoteColumns::sortKeyFor("涨跌值"), QString("change"));
        QCOMPARE(QuoteColumns::sortKeyFor("涨跌幅"), QString("change_pct"));
        QCOMPARE(QuoteColumns::sortKeyFor("买一"), QString("buy1"));
        QCOMPARE(QuoteColumns::sortKeyFor("卖一"), QString("sell1"));
        QCOMPARE(QuoteColumns::sortKeyFor("委比"), QString("commi"));
        QCOMPARE(QuoteColumns::sortKeyFor("成交量"), QString("vol"));
        QCOMPARE(QuoteColumns::sortKeyFor("成交额"), QString("amount"));
        QCOMPARE(QuoteColumns::sortKeyFor("均价"), QString("avg"));
        QVERIFY(QuoteColumns::sortKeyFor("K线").isEmpty());  // K 线不可排序
        QVERIFY(QuoteColumns::sortKeyFor("不存在").isEmpty());
    }
```

- [ ] **Step 2: 构建并确认失败**

Run: `scripts\build.cmd`
Expected: 编译失败，错误形如 `error C2039: "sortKeyFor": 不是 "QuoteColumns" 的成员`

- [ ] **Step 3: 实现**

`src/data/QuoteColumns.h`：把 `ColumnSpec` 之前的声明区改为

```cpp
struct ColumnSpec {
    QString header;
    bool rightAlign = true;
    bool colored = false;
    bool isKLine = false;
    std::function<QString(const Quote&)> text;
    std::function<int(const Quote&)> sign;
};

namespace QuoteColumns {
QStringList allHeaders();
QString configKeyFor(const QString& header);
QString sortKeyFor(const QString& header);
bool isVisible(const QJsonObject& cfg, const QString& header);
QVector<ColumnSpec> activeColumns(const QJsonObject& cfg);
}
```

`src/data/QuoteColumns.cpp`：`Entry` 增加 `sortKey`，并更新整张表（**12 行全部要改，字段顺序不能错**）：

```cpp
namespace {
struct Entry {
    const char* header;
    const char* key;
    const char* sortKey;
    bool right;
    bool colored;
};
const Entry kEntries[] = {
    {"代码",   "code_visible",       "code",       true,  false},
    {"名称",   "name_visible",       "name",       false, false},
    {"现价",   "price_visible",      "price",      true,  true},
    {"涨跌值", "change_visible",     "change",     true,  true},
    {"涨跌幅", "change_pct_visible", "change_pct", true,  true},
    {"买一",   "b1s1_visible",       "buy1",       true,  true},
    {"卖一",   "b1s1_visible",       "sell1",      false, true},
    {"委比",   "commi_visible",      "commi",      true,  true},
    {"成交量", "vol_visible",        "vol",        true,  false},
    {"成交额", "amount_visible",     "amount",     true,  false},
    {"均价",   "avg_visible",        "avg",        true,  true},
    {"K线",    "kline_visible",      "",           false, false},
};
}  // namespace
```

并在 `configKeyFor` 之后追加：

```cpp
QString QuoteColumns::sortKeyFor(const QString& header) {
    for (const Entry& e : kEntries)
        if (QString::fromUtf8(e.header) == header) return QString::fromUtf8(e.sortKey);
    return QString();
}
```

- [ ] **Step 4: 构建并运行测试**

Run: `scripts\build.cmd` 然后 `build\test_model.exe`（或 `scripts\test.cmd`）
Expected: `Totals: 5 passed, 0 failed`

- [ ] **Step 5: 提交**

```
git add src/data/QuoteColumns.h src/data/QuoteColumns.cpp tests/test_model.cpp
git commit -m "feat(data): QuoteColumns 增加列->排序键映射（买一/卖一可区分）"
```

---

### Task 2: QuoteSort 排序纯函数

**Files:**
- Create: `src/data/QuoteSort.h`
- Create: `src/data/QuoteSort.cpp`
- Modify: `CMakeLists.txt`（`StockWidgetCore` 源文件列表）
- Test: `tests/test_sorter.cpp`（新建）
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: 写失败测试**

新建 `tests/test_sorter.cpp`：

```cpp
#include <QtTest>
#include "data/Quote.h"
#include "data/QuoteColumns.h"
#include "data/QuoteSort.h"

namespace {

Quote makeQuote(const QString& code, const QString& name, double price, double prevClose,
                double volume, double amount = 0.0) {
    Quote q;
    q.code = code;
    q.name = name;
    q.price = price;
    q.prevClose = prevClose;
    q.open = prevClose;
    q.high = qMax(price, prevClose);
    q.low = qMin(price, prevClose);
    q.volume = volume;
    q.amount = amount;
    q.avg = volume > 0 ? amount / volume : prevClose;
    q.committee = 0.0;
    return q;
}

QVector<Quote> threeQuotes() {
    return {makeQuote("sh600000", "浦发银行", 10.2, 10.0, 8000, 81600.0),
            makeQuote("sz000001", "平安银行", 9.5, 10.0, 5000, 47500.0),
            makeQuote("sz300136", "信维通信", 10.5, 10.0, 1000, 10500.0)};
}

}  // namespace

class TestSorter : public QObject {
    Q_OBJECT
private slots:
    void sortableKeysCoverEverySortableColumn() {
        QStringList fromColumns;
        for (const QString& h : QuoteColumns::allHeaders()) {
            const QString k = QuoteColumns::sortKeyFor(h);
            if (!k.isEmpty()) fromColumns << k;
        }
        QCOMPARE(fromColumns, QuoteSort::sortableKeys());
        QVERIFY(QuoteSort::isValidKey("change_pct"));
        QVERIFY(!QuoteSort::isValidKey(""));
        QVERIFY(!QuoteSort::isValidKey("kline_visible"));
    }
    void sortsChangePctDescending() {
        QVector<Quote> q = threeQuotes();
        QuoteSort::sortQuotes(q, "change_pct", false);
        QCOMPARE(q.at(0).code, QString("sz300136"));  // +5.00%
        QCOMPARE(q.at(1).code, QString("sh600000"));  // +2.00%
        QCOMPARE(q.at(2).code, QString("sz000001"));  // -5.00%
    }
    void sortsChangePctAscending() {
        QVector<Quote> q = threeQuotes();
        QuoteSort::sortQuotes(q, "change_pct", true);
        QCOMPARE(q.at(0).code, QString("sz000001"));
        QCOMPARE(q.at(1).code, QString("sh600000"));
        QCOMPARE(q.at(2).code, QString("sz300136"));
    }
    void sortsByAmountAndAveragePrice() {
        QVector<Quote> q = threeQuotes();
        QuoteSort::sortQuotes(q, "amount", false);
        QCOMPARE(q.at(0).code, QString("sh600000"));  // 8.16 万
        QCOMPARE(q.at(1).code, QString("sz000001"));  // 4.75 万
        QCOMPARE(q.at(2).code, QString("sz300136"));  // 1.05 万
        QuoteSort::sortQuotes(q, "avg", true);
        QCOMPARE(q.at(0).code, QString("sz000001"));  // 9.50
        QCOMPARE(q.at(1).code, QString("sh600000"));  // 10.20
        QCOMPARE(q.at(2).code, QString("sz300136"));  // 10.50
    }
    void stableForEqualValues() {
        QVector<Quote> q{makeQuote("sh600000", "A", 10.0, 10.0, 1),
                         makeQuote("sz000001", "B", 10.0, 10.0, 2),
                         makeQuote("sz300136", "C", 10.0, 10.0, 3)};
        QuoteSort::sortQuotes(q, "change_pct", false);
        QCOMPARE(q.at(0).code, QString("sh600000"));
        QCOMPARE(q.at(1).code, QString("sz000001"));
        QCOMPARE(q.at(2).code, QString("sz300136"));
        QuoteSort::sortQuotes(q, "change_pct", true);  // 升序同样保持原序
        QCOMPARE(q.at(0).code, QString("sh600000"));
        QCOMPARE(q.at(2).code, QString("sz300136"));
    }
    void zeroValuesParticipateNaturally() {
        QVector<Quote> q{makeQuote("sh600000", "A", 10.0, 10.0, 8000),
                         makeQuote("sz000001", "B", 10.0, 10.0, 0)};
        QuoteSort::sortQuotes(q, "vol", false);
        QCOMPARE(q.at(0).code, QString("sh600000"));
        QCOMPARE(q.at(1).code, QString("sz000001"));
    }
    void textKeysSortLexically() {
        QVector<Quote> q{makeQuote("sh600001", "乙", 10.0, 10.0, 1),
                         makeQuote("sh600000", "甲", 10.0, 10.0, 1)};
        QuoteSort::sortQuotes(q, "code", true);
        QCOMPARE(q.at(0).code, QString("sh600000"));
        QCOMPARE(q.at(1).code, QString("sh600001"));
        QuoteSort::sortQuotes(q, "name", true);
        // QString::compare 按 UTF-16 码位比较（不做拼音）：乙 U+4E59 < 甲 U+7532
        QCOMPARE(q.at(0).name, QString("乙"));
        QCOMPARE(q.at(1).name, QString("甲"));
    }
    void unknownKeyKeepsOrder() {
        QVector<Quote> q = threeQuotes();
        QuoteSort::sortQuotes(q, "", false);
        QCOMPARE(q.at(0).code, QString("sh600000"));
        QuoteSort::sortQuotes(q, "nope", true);
        QCOMPARE(q.at(1).code, QString("sz000001"));
    }
    void emptyListIsSafe() {
        QVector<Quote> q;
        QuoteSort::sortQuotes(q, "price", false);
        QVERIFY(q.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestSorter)
#include "test_sorter.moc"
```

- [ ] **Step 2: 注册测试目标并确认失败**

`tests/CMakeLists.txt` 末尾（`add_test(NAME test_ui COMMAND test_ui)` 之后）追加：

```cmake
qt_add_executable(test_sorter test_sorter.cpp)
target_link_libraries(test_sorter PRIVATE Qt6::Test StockWidgetCore)
add_test(NAME test_sorter COMMAND test_sorter)

qt_add_executable(test_namealias test_namealias.cpp)
target_link_libraries(test_namealias PRIVATE Qt6::Test StockWidgetCore)
add_test(NAME test_namealias COMMAND test_namealias)
```

`tests/test_namealias.cpp` 本步先建占位文件（Task 3 会写满），否则 CMake 配置失败：

```cpp
#include <QtTest>
class TestNameAliasPlaceholder : public QObject { Q_OBJECT };
QTEST_GUILESS_MAIN(TestNameAliasPlaceholder)
#include "test_namealias.moc"
```

Run: `scripts\build.cmd`
Expected: 编译失败，错误形如 `fatal error C1083: 无法打开包括文件: "data/QuoteSort.h"`

- [ ] **Step 3: 实现**

新建 `src/data/QuoteSort.h`：

```cpp
#pragma once
#include "data/Quote.h"
#include <QString>
#include <QStringList>
#include <QVector>

namespace QuoteSort {
// 可排序键白名单（必须与 QuoteColumns 的 sortKey 列一一对应）
QStringList sortableKeys();
bool isValidKey(const QString& key);
// key 非法或元素不足 2 个时保持原序；asc=false 为降序
void sortQuotes(QVector<Quote>& quotes, const QString& key, bool asc);
}
```

新建 `src/data/QuoteSort.cpp`：

```cpp
#include "data/QuoteSort.h"
#include <algorithm>

namespace {

const QStringList kKeys{QStringLiteral("code"),    QStringLiteral("name"),
                        QStringLiteral("price"),   QStringLiteral("change"),
                        QStringLiteral("change_pct"), QStringLiteral("buy1"),
                        QStringLiteral("sell1"),   QStringLiteral("commi"),
                        QStringLiteral("vol"),     QStringLiteral("amount"),
                        QStringLiteral("avg")};

bool isTextKey(const QString& key) {
    return key == QStringLiteral("code") || key == QStringLiteral("name");
}

double numericValue(const Quote& q, const QString& key) {
    if (key == QStringLiteral("price")) return q.price;
    if (key == QStringLiteral("change")) return q.prevClose ? q.price - q.prevClose : 0.0;
    if (key == QStringLiteral("change_pct"))
        return q.prevClose ? (q.price / q.prevClose - 1.0) * 100.0 : 0.0;
    if (key == QStringLiteral("buy1")) return q.buy1;
    if (key == QStringLiteral("sell1")) return q.sell1;
    if (key == QStringLiteral("commi")) return q.committee;
    if (key == QStringLiteral("vol")) return q.volume;
    if (key == QStringLiteral("amount")) return q.amount;
    if (key == QStringLiteral("avg")) return q.avg;
    return 0.0;
}

}  // namespace

QStringList QuoteSort::sortableKeys() { return kKeys; }

bool QuoteSort::isValidKey(const QString& key) { return kKeys.contains(key); }

void QuoteSort::sortQuotes(QVector<Quote>& quotes, const QString& key, bool asc) {
    if (!isValidKey(key) || quotes.size() < 2) return;
    if (isTextKey(key)) {
        const bool byCode = key == QStringLiteral("code");
        std::stable_sort(quotes.begin(), quotes.end(),
                         [byCode, asc](const Quote& a, const Quote& b) {
                             const int cmp = byCode ? QString::compare(a.code, b.code)
                                                    : QString::compare(a.name, b.name);
                             return asc ? cmp < 0 : cmp > 0;
                         });
        return;
    }
    std::stable_sort(quotes.begin(), quotes.end(), [&key, asc](const Quote& a, const Quote& b) {
        const double va = numericValue(a, key);
        const double vb = numericValue(b, key);
        return asc ? va < vb : va > vb;
    });
}
```

`CMakeLists.txt` 的 `StockWidgetCore` 源文件列表里，在 `src/data/QuoteParser.cpp` 之后插入：

```cmake
    src/data/QuoteSort.cpp
```

- [ ] **Step 4: 构建并运行测试**

Run: `scripts\build.cmd` 然后 `build\test_sorter.exe`
Expected: `Totals: 9 passed, 0 failed`

- [ ] **Step 5: 提交**

```
git add src/data/QuoteSort.h src/data/QuoteSort.cpp CMakeLists.txt tests/test_sorter.cpp tests/test_namealias.cpp tests/CMakeLists.txt
git commit -m "feat(data): 新增 QuoteSort 排序纯函数（stable、键白名单）"
```

---

### Task 3: NameAlias 别名映射

**Files:**
- Create: `src/data/NameAlias.h`
- Create: `src/data/NameAlias.cpp`
- Modify: `CMakeLists.txt`
- Test: `tests/test_namealias.cpp`

- [ ] **Step 1: 写失败测试**

把 `tests/test_namealias.cpp` 覆盖为：

```cpp
#include <QtTest>
#include <QJsonObject>
#include "data/NameAlias.h"
#include "data/QuoteParser.h"

namespace {
const char* kPufa =
    "var hq_str_sh600000=\"浦发银行,10.100,10.000,10.200,10.300,9.900,"
    "10.190,10.200,8000,81600.000,"
    "100,10.180,200,10.170,300,10.160,400,10.150,500,10.140,"
    "600,10.210,700,10.220,800,10.230,900,10.240,1000,10.250,"
    "2026-09-11,15:00:00,00\";\n";

QVector<Quote> pufa(const QuoteFormatOptions& opt = {}) {
    return QuoteParser::parseText(QString::fromUtf8(kPufa), opt);
}
}  // namespace

class TestNameAlias : public QObject {
    Q_OBJECT
private slots:
    void aliasOverridesQuoteName() {
        QVector<Quote> q = pufa();
        NameAlias::applyAliases(q, QJsonObject{{QStringLiteral("sh600000"),
                                                QStringLiteral("浦发(老仓)")}});
        QCOMPARE(q.at(0).name, QString("浦发(老仓)"));
    }
    void emptyAliasFallsBackToQuoteName() {
        QVector<Quote> q = pufa();
        NameAlias::applyAliases(q, QJsonObject{{QStringLiteral("sh600000"), QString()}});
        QCOMPARE(q.at(0).name, QString("浦发银行"));
    }
    void unknownCodeKeepsQuoteName() {
        QVector<Quote> q = pufa();
        NameAlias::applyAliases(q, QJsonObject{{QStringLiteral("sz000001"),
                                                QStringLiteral("平安银行(别名)")}});
        QCOMPARE(q.at(0).name, QString("浦发银行"));
    }
    void bareNumericKeyStillMatches() {
        QVector<Quote> q = pufa();
        NameAlias::applyAliases(q, QJsonObject{{QStringLiteral("600000"),
                                                QStringLiteral("老仓")}});
        QCOMPARE(q.at(0).name, QString("老仓"));
    }
    void aliasIsNotTruncatedByParser() {
        QuoteFormatOptions opt;
        opt.nameLength = 2;
        QVector<Quote> q = pufa(opt);
        QCOMPARE(q.at(0).name, QString("浦发"));  // 截断只作用于行情名称
        NameAlias::applyAliases(q, QJsonObject{{QStringLiteral("sh600000"),
                                                QStringLiteral("浦发(老仓)")}});
        QCOMPARE(q.at(0).name, QString("浦发(老仓)"));
    }
    void aliasForReturnsEmptyWhenMissing() {
        QVERIFY(NameAlias::aliasFor({}, QStringLiteral("sh600000")).isEmpty());
        QVERIFY(NameAlias::aliasFor(QJsonObject{{QStringLiteral("sh600000"), QStringLiteral("x")}},
                                    QStringLiteral("sz000001"))
                    .isEmpty());
    }
    void emptyMapIsNoOp() {
        QVector<Quote> q = pufa();
        NameAlias::applyAliases(q, {});
        QCOMPARE(q.at(0).name, QString("浦发银行"));
    }
};

QTEST_GUILESS_MAIN(TestNameAlias)
#include "test_namealias.moc"
```

- [ ] **Step 2: 构建并确认失败**

Run: `scripts\build.cmd`
Expected: 编译失败，错误形如 `无法打开包括文件: "data/NameAlias.h"`

- [ ] **Step 3: 实现**

新建 `src/data/NameAlias.h`：

```cpp
#pragma once
#include "data/Quote.h"
#include <QJsonObject>
#include <QString>
#include <QVector>

namespace NameAlias {
// 命中返回别名（已 trim），否则返回空串
QString aliasFor(const QJsonObject& nameMap, const QString& code);
// 别名优先于行情名称；别名不受 name_length 截断
void applyAliases(QVector<Quote>& quotes, const QJsonObject& nameMap);
}
```

新建 `src/data/NameAlias.cpp`：

```cpp
#include "data/NameAlias.h"
#include "data/StockCode.h"

namespace {

// 把映射表的键统一成归一化形式（容错：手改配置写入的裸代码 / 大小写混杂）
// 单次调用的开销 O(map)；批量场景请走 applyAliases，它只归一化一次。
QJsonObject normalizedMap(const QJsonObject& nameMap) {
    QJsonObject out;
    for (auto it = nameMap.begin(); it != nameMap.end(); ++it) {
        const QString value = it.value().toString().trimmed();
        if (value.isEmpty()) continue;
        const auto normalized = StockCode::normalize(it.key());
        const QString key = normalized ? *normalized : it.key();
        if (!out.contains(key)) out.insert(key, value);
    }
    return out;
}

}  // namespace

QString NameAlias::aliasFor(const QJsonObject& nameMap, const QString& code) {
    if (nameMap.isEmpty() || code.isEmpty()) return QString();
    const QJsonObject map = normalizedMap(nameMap);
    QString alias = map.value(code).toString();
    if (alias.isEmpty()) {
        const auto normalized = StockCode::normalize(code);
        if (normalized) alias = map.value(*normalized).toString();
    }
    return alias;
}

void NameAlias::applyAliases(QVector<Quote>& quotes, const QJsonObject& nameMap) {
    if (nameMap.isEmpty() || quotes.isEmpty()) return;
    const QJsonObject map = normalizedMap(nameMap);
    if (map.isEmpty()) return;
    for (Quote& q : quotes) {
        QString alias = map.value(q.code).toString();  // 常见路径：q.code 已是归一化形式
        if (alias.isEmpty()) {
            const auto normalized = StockCode::normalize(q.code);
            if (normalized) alias = map.value(*normalized).toString();
        }
        if (!alias.isEmpty()) q.name = alias;
    }
}
```

`CMakeLists.txt` 的 `StockWidgetCore` 源文件列表里，在 `src/data/StockCode.cpp` 之后插入：

```cmake
    src/data/NameAlias.cpp
```

- [ ] **Step 4: 构建并运行测试**

Run: `scripts\build.cmd` 然后 `build\test_namealias.exe`
Expected: `Totals: 7 passed, 0 failed`

- [ ] **Step 5: 提交**

```
git add src/data/NameAlias.h src/data/NameAlias.cpp CMakeLists.txt tests/test_namealias.cpp
git commit -m "feat(data): 新增 NameAlias 别名映射（别名优先、不受截断）"
```

---

### Task 4: ConfigStore::normalize 容错新增键

**Files:**
- Modify: `src/app/ConfigStore.cpp`
- Test: `tests/test_config.cpp`

- [ ] **Step 1: 写失败测试**

在 `tests/test_config.cpp` 的 `columnKeyMapping()` 之后追加：

```cpp
    void normalizesNameMapKeys() {
        QJsonObject raw;
        raw["name_map"] = QJsonObject{{"600000", "浦发(老仓)"},
                                      {"sz000001", "  "},
                                      {"bad", "x"}};
        const QJsonObject out = ConfigStore::normalize(raw);
        const QJsonObject map = out.value("name_map").toObject();
        QCOMPARE(map.size(), 1);
        QCOMPARE(map.value("sh600000").toString(), QString("浦发(老仓)"));
    }
    void dropsNonObjectNameMap() {
        QJsonObject raw;
        raw["name_map"] = QJsonArray{"sh600000"};
        QVERIFY(!ConfigStore::normalize(raw).contains("name_map"));
    }
    void resetsUnknownSortKeyButKeepsOrderFlag() {
        QJsonObject raw;
        raw["sort_key"] = "kline_visible";
        raw["sort_asc"] = true;
        QJsonObject out = ConfigStore::normalize(raw);
        QCOMPARE(out.value("sort_key").toString(), QString());
        QCOMPARE(out.value("sort_asc").toBool(), true);
        raw["sort_key"] = "change_pct";
        out = ConfigStore::normalize(raw);
        QCOMPARE(out.value("sort_key").toString(), QString("change_pct"));
    }
    void legacyConfigKeepsOldBehavior() {
        const QJsonObject out = ConfigStore::normalize(QJsonObject{{"refresh_seconds", 5}});
        QCOMPARE(out.value("sort_key").toString(), QString());
        QCOMPARE(out.value("sort_asc").toBool(), false);
        QCOMPARE(out.value("name_map").toObject().size(), 0);
    }
```

- [ ] **Step 2: 构建并确认失败**

Run: `scripts\build.cmd`
Expected: `build\test_config.exe` 运行后失败，形如
`FAIL!  : TestConfig::normalizesNameMapKeys() Compared values are not the same`（或 map.size() 为 3）

- [ ] **Step 3: 实现**

`src/app/ConfigStore.cpp` 顶部 include 增加两行（放在 `#include "data/QuoteColumns.h"` 之后）：

```cpp
#include "data/QuoteSort.h"
#include "data/StockCode.h"
```

在 `normalize()` 的 `return out;` 之前追加：

```cpp
    // name_map：非 object 丢弃；键归一化；空值剔除
    if (out.contains(QStringLiteral("name_map"))) {
        const QJsonValue value = out.value(QStringLiteral("name_map"));
        if (!value.isObject()) {
            out.remove(QStringLiteral("name_map"));
        } else {
            const QJsonObject src = value.toObject();
            QJsonObject cleaned;
            for (auto it = src.begin(); it != src.end(); ++it) {
                const auto code = StockCode::normalize(it.key());
                if (!code) continue;
                const QString alias = it.value().toString().trimmed();
                if (alias.isEmpty()) continue;
                cleaned.insert(*code, alias);
            }
            out.insert(QStringLiteral("name_map"), cleaned);
        }
    }

    // sort_key：白名单兜底；sort_asc 归一化为 bool
    const QString sortKey = out.value(QStringLiteral("sort_key")).toString();
    if (!sortKey.isEmpty() && !QuoteSort::isValidKey(sortKey))
        out.insert(QStringLiteral("sort_key"), QString());
    if (out.contains(QStringLiteral("sort_asc")))
        out.insert(QStringLiteral("sort_asc"), out.value(QStringLiteral("sort_asc")).toBool(false));
```

- [ ] **Step 4: 构建并运行测试**

Run: `scripts\build.cmd` 然后 `build\test_config.exe`
Expected: `Totals: 8 passed, 0 failed`

- [ ] **Step 5: 提交**

```
git add src/app/ConfigStore.cpp tests/test_config.cpp
git commit -m "feat(config): normalize 容错 name_map / sort_key / sort_asc"
```

---

### Task 5: FloatWindow 排序管道 + 表头三态 + 指示器

**Files:**
- Modify: `src/ui/FloatWindow.h`
- Modify: `src/ui/FloatWindow.cpp`
- Test: `tests/test_ui.cpp`

- [ ] **Step 1: 写失败测试**

`tests/test_ui.cpp` 头部 include 区（`#include "ui/SettingsDialog.h"` 之后）追加：

```cpp
#include <QComboBox>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QTimer>
#include "data/QuoteParser.h"
#include "data/QuoteSort.h"
#include "ui/QuoteModel.h"
```

在 `ProbeWindow` 类里追加两个成员函数：

```cpp
    QMenu* menuAt(const QPoint& globalPos) { return buildContextMenu(globalPos); }
    void pushQuotes(const QVector<Quote>& quotes) {
        QMetaObject::invokeMethod(this, "onQuotesReady", Qt::DirectConnection,
                                  Q_ARG(QVector<Quote>, quotes));
    }
```

在 `baseConfig()` 的 `cfg["checked_codes"]` 之后补两行（现有用例不受影响）：

```cpp
    cfg["codes"] = QJsonArray{"sh600000"};
    cfg["name_visible"] = true;
```

在 `baseConfig()` 之前插入共享数据与工具函数：

```cpp
// 两行真实新浪格式行情：sh600000 +2.00%，sz000001 -5.00%（行序 = 自选顺序）
static const char* kTwoLines =
    "var hq_str_sh600000=\"浦发银行,10.100,10.000,10.200,10.300,9.900,"
    "10.190,10.200,8000,81600.000,"
    "100,10.180,200,10.170,300,10.160,400,10.150,500,10.140,"
    "600,10.210,700,10.220,800,10.230,900,10.240,1000,10.250,"
    "2026-09-11,15:00:00,00\";\n"
    "var hq_str_sz000001=\"平安银行,10.000,10.000,9.500,10.100,9.400,"
    "9.490,9.500,5000,47500.000,"
    "100,9.480,200,9.470,300,9.460,400,9.450,500,9.440,"
    "600,9.510,700,9.520,800,9.530,900,9.540,1000,9.550,"
    "2026-09-11,15:00:00,00\";\n";

static QVector<Quote> twoQuotes() {
    return QuoteParser::parseText(QString::fromUtf8(kTwoLines), QuoteFormatOptions{});
}

static int columnOf(QAbstractItemModel* model, const QString& header) {
    for (int c = 0; c < model->columnCount(); ++c)
        if (model->headerData(c, Qt::Horizontal).toString() == header) return c;
    return -1;
}

static QString cellText(QAbstractItemModel* model, int row, int col) {
    return model->data(model->index(row, col), Qt::DisplayRole).toString();
}
```

在 `TestUi` 的 `private slots:` 之后（第一个用例之前）加入 `initTestCase`：

```cpp
    void initTestCase() { qRegisterMetaType<QVector<Quote>>("QVector<Quote>"); }
```

在 `settingsDialogBuildsAllTabs()` 之前追加三个用例：

```cpp
    void sortFromConfigAppliedOnQuotesReady() {
        QJsonObject cfg = baseConfig();
        cfg["sort_key"] = "change_pct";
        cfg["sort_asc"] = false;
        ProbeWindow w(cfg);
        w.show();
        QTest::qWait(200);

        auto* table = w.findChild<QTableView*>();
        QVERIFY(table);
        w.pushQuotes(twoQuotes());

        const int col = columnOf(table->model(), "涨跌幅");
        QVERIFY(col >= 0);
        QCOMPARE(cellText(table->model(), 0, col), QString("+2.00%"));
        QCOMPARE(cellText(table->model(), 1, col), QString("-5.00%"));

        auto* header = table->horizontalHeader();
        QVERIFY(header->isSortIndicatorShown());
        QCOMPARE(header->sortIndicatorSection(), col);
        QCOMPARE(header->sortIndicatorOrder(), Qt::DescendingOrder);
    }

    void headerClickCyclesSortDescAscOff() {
        QJsonObject cfg = baseConfig();
        cfg["header_visible"] = true;  // 表头不可点时点击会落到视图（老行为：隐藏窗口）
        ProbeWindow w(cfg);
        w.show();
        QTest::qWait(200);

        auto* table = w.findChild<QTableView*>();
        QVERIFY(table);
        w.pushQuotes(twoQuotes());

        auto* header = table->horizontalHeader();
        const int col = columnOf(table->model(), "涨跌幅");
        QVERIFY(col >= 0);
        // 每次都按当前列宽重算点击位置：排序指示器出现/数据变化都会让列宽微调
        auto clickHeader = [header, col] {
            const QPoint p(header->sectionViewportPosition(col) + header->sectionSize(col) / 2,
                           header->height() / 2);
            QTest::mouseClick(header, Qt::LeftButton, Qt::NoModifier, p);
            QTest::qWait(50);
        };

        clickHeader();  // 第一次 → 降序
        QCOMPARE(w.currentConfig().value("sort_key").toString(), QString("change_pct"));
        QCOMPARE(w.currentConfig().value("sort_asc").toBool(), false);
        QVERIFY(header->isSortIndicatorShown());
        QCOMPARE(header->sortIndicatorSection(), col);
        QCOMPARE(cellText(table->model(), 0, col), QString("+2.00%"));
        QVERIFY2(w.isVisible(), "clicking the header must not hide the window");

        clickHeader();  // 第二次 → 升序
        QCOMPARE(w.currentConfig().value("sort_key").toString(), QString("change_pct"));
        QCOMPARE(w.currentConfig().value("sort_asc").toBool(), true);
        QCOMPARE(header->sortIndicatorOrder(), Qt::AscendingOrder);
        QCOMPARE(cellText(table->model(), 0, col), QString("-5.00%"));
        QVERIFY(w.isVisible());

        clickHeader();  // 第三次 → 取消排序，回到自选顺序
        QCOMPARE(w.currentConfig().value("sort_key").toString(), QString());
        QVERIFY(!header->isSortIndicatorShown());
        QCOMPARE(cellText(table->model(), 0, col), QString("+2.00%"));
        QVERIFY(w.isVisible());
    }

    void headerClickOnKLineColumnIsIgnored() {
        QJsonObject cfg = baseConfig();
        cfg["kline_visible"] = true;
        cfg["header_visible"] = true;
        ProbeWindow w(cfg);
        w.show();
        QTest::qWait(200);

        auto* table = w.findChild<QTableView*>();
        QVERIFY(table);
        w.pushQuotes(twoQuotes());

        auto* header = table->horizontalHeader();
        const int col = columnOf(table->model(), "K线");
        QVERIFY(col >= 0);
        const QPoint pos(header->sectionViewportPosition(col) + header->sectionSize(col) / 2,
                         header->height() / 2);
        QTest::mouseClick(header, Qt::LeftButton, Qt::NoModifier, pos);
        QTest::qWait(50);

        QCOMPARE(w.currentConfig().value("sort_key").toString(), QString());
        QVERIFY(!header->isSortIndicatorShown());
        QVERIFY(w.isVisible());
    }
```

- [ ] **Step 2: 构建并确认失败**

Run: `scripts\build.cmd`
Expected: 编译失败，错误形如 `error C2039: "buildContextMenu": 不是 "FloatWindow" 的成员`

- [ ] **Step 3: 实现**

`src/ui/FloatWindow.h`：

1) 前置声明区加入 `class QMenu;`（放在 `class QVBoxLayout;` 之后）

2) public 区 `void stop();` 之后加入：

```cpp
    void setAlias(const QString& code, const QString& alias);  // Task 6 用到；本任务先占位实现
```

3) protected 区 `void contextMenuEvent(QContextMenuEvent* event) override;` 之后加入：

```cpp
    QMenu* buildContextMenu(const QPoint& globalPos);
```

4) 把 `void onQuotesReady(const QVector<Quote>& quotes);` 从 `private:` 区**移到**新的 `private slots:` 区（**并从 private 区原位置删除该行**，否则会重复声明）：

```cpp
private slots:
    void onQuotesReady(const QVector<Quote>& quotes);

private:
```

5) private 区 `void showContextMenu(const QPoint& globalPos);` 之后加入：

```cpp
    void updateSortIndicator();
    void cycleSort(int column);
    void redisplayLastQuotes();
```

6) 成员区 `int m_nameLength = 0;` 之后加入：

```cpp
    QJsonObject m_nameMap;
    QString m_sortKey;
    bool m_sortAsc = false;
    QVector<Quote> m_rawQuotes;
```

7) 成员区 `bool m_columnWidthsFrozen = false;` 之后加入：

```cpp
    bool m_pressOnHeader = false;
    int m_headerPressColumn = -1;
```

`src/ui/FloatWindow.cpp`：

1) include 区（`#include "data/StockCode.h"` 之后）加入：

```cpp
#include "data/NameAlias.h"
#include "data/QuoteSort.h"
```

2) 构造函数里，`m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);` 之后加入：

```cpp
    m_table->horizontalHeader()->setSectionsClickable(true);
    m_table->horizontalHeader()->setSortIndicatorShown(false);
```

3) 构造函数里 `m_table->setModel(m_model);` 之后**不需要**任何 connect（表头事件由 `eventFilter` 接管）。

4) `applyConfig()` 里，`m_nameLength = raw.value(QStringLiteral("name_length")).toInt(0);` 之后加入：

```cpp
    const QJsonObject oldNameMap = m_nameMap;
    const QString oldSortKey = m_sortKey;
    const bool oldSortAsc = m_sortAsc;
    m_nameMap = raw.value(QStringLiteral("name_map")).toObject();
    m_sortKey = raw.value(QStringLiteral("sort_key")).toString();
    m_sortAsc = raw.value(QStringLiteral("sort_asc")).toBool(false);
```

5) `applyConfig()` 末尾（`if (oldChecked != m_checkedCodes && isVisible()) refreshNow();` 之后）加入：

```cpp
    updateSortIndicator();
    if (m_nameMap != oldNameMap || m_sortKey != oldSortKey || m_sortAsc != oldSortAsc)
        redisplayLastQuotes();
```

6) `currentConfig()` 里，`cfg[QStringLiteral("name_length")] = m_nameLength;` 之后加入：

```cpp
    cfg[QStringLiteral("name_map")] = m_nameMap;
    cfg[QStringLiteral("sort_key")] = m_sortKey;
    cfg[QStringLiteral("sort_asc")] = m_sortAsc;
```

7) 用下面三个函数整体替换现有的 `onQuotesReady()`：

```cpp
void FloatWindow::onQuotesReady(const QVector<Quote>& quotes) {
    m_errorLabel->setVisible(false);
    m_rawQuotes = quotes;
    redisplayLastQuotes();
}

void FloatWindow::redisplayLastQuotes() {
    if (m_rawQuotes.isEmpty()) return;
    QVector<Quote> quotes = m_rawQuotes;
    NameAlias::applyAliases(quotes, m_nameMap);
    QuoteSort::sortQuotes(quotes, m_sortKey, m_sortAsc);
    const int before = m_model->rowCount();
    m_model->setQuotes(quotes);
    if (m_model->rowCount() != before) m_columnWidthsFrozen = false;
    refitSize();
}

void FloatWindow::updateSortIndicator() {
    QHeaderView* header = m_table->horizontalHeader();
    int column = -1;
    if (!m_sortKey.isEmpty()) {
        for (int c = 0; c < m_model->columnCount(); ++c) {
            if (QuoteColumns::sortKeyFor(
                    m_model->headerData(c, Qt::Horizontal).toString()) == m_sortKey) {
                column = c;
                break;
            }
        }
    }
    if (column < 0) {
        header->setSortIndicatorShown(false);
        return;
    }
    header->setSortIndicatorShown(true);
    header->setSortIndicator(column, m_sortAsc ? Qt::AscendingOrder : Qt::DescendingOrder);
}

void FloatWindow::cycleSort(int column) {
    const QString header = m_model->headerData(column, Qt::Horizontal).toString();
    const QString key = QuoteColumns::sortKeyFor(header);
    if (key.isEmpty()) return;  // K 线不可排序
    if (m_sortKey != key) {
        m_sortKey = key;
        m_sortAsc = false;  // 首次点击 → 降序
    } else if (!m_sortAsc) {
        m_sortAsc = true;  // 第二次 → 升序
    } else {
        m_sortKey.clear();  // 第三次 → 取消排序，回到自选顺序
        m_sortAsc = false;
    }
    updateSortIndicator();
    redisplayLastQuotes();
    notifyChanged();
}
```

8) `eventFilter()` 函数体最前面加入表头接管分支：

```cpp
bool FloatWindow::eventFilter(QObject* obj, QEvent* event) {
    // 表头左键由本窗口接管：QHeaderView 不接受这些事件，Qt 会把它继续冒泡给 QTableView，
    // 从而被当成拖动/单击隐藏（并吞掉 sectionClicked）。这里自己实现三态排序，语义同 sectionClicked。
    if (obj == m_table->horizontalHeader()) {
        switch (event->type()) {
            case QEvent::MouseButtonPress: {
                auto* me = static_cast<QMouseEvent*>(event);
                if (me->button() != Qt::LeftButton) break;
                m_dragging = false;
                m_pressOnHeader = true;
                m_headerPressColumn = m_table->horizontalHeader()->logicalIndexAt(
                    me->position().toPoint().x());
                return true;
            }
            case QEvent::MouseMove:
                if (m_pressOnHeader) return true;
                break;
            case QEvent::MouseButtonRelease: {
                auto* me = static_cast<QMouseEvent*>(event);
                if (me->button() != Qt::LeftButton || !m_pressOnHeader) break;
                m_pressOnHeader = false;
                const int column = m_table->horizontalHeader()->logicalIndexAt(
                    me->position().toPoint().x());
                if (column >= 0 && column == m_headerPressColumn) cycleSort(column);
                return true;
            }
            case QEvent::MouseButtonDblClick:
                return true;  // 表头上的双击不触发「单击隐藏」
            default:
                break;
        }
    }
    if (event->type() == QEvent::MouseButtonDblClick) {
```

> 注意：`eventFilter` 原有分支**一行都不要动**，只新增上面这段。
> **不要**连接 `QHeaderView::sectionClicked`（会在事件被接管后双重触发，且 Qt 实际上不会发出）。

9) 本任务先给出 `setAlias()` 的可用实现（Task 6 会接入 UI 入口）：

```cpp
void FloatWindow::setAlias(const QString& code, const QString& alias) {
    const auto normalized = StockCode::normalize(code);
    if (!normalized) return;
    QJsonObject map = m_nameMap;
    const QString value = alias.trimmed();
    if (value.isEmpty()) map.remove(*normalized);
    else map.insert(*normalized, value);
    if (map == m_nameMap) return;
    m_nameMap = map;
    redisplayLastQuotes();
    notifyChanged();
}
```

- [ ] **Step 4: 构建并运行测试**

Run: `scripts\build.cmd` 然后 `build\test_ui.exe`
Expected: 全部用例 PASS（`Totals: 14 passed, 0 failed`），其中 `headerClickCyclesSortDescAscOff` 验证三态与"表头点击不隐藏窗口"

- [ ] **Step 5: 提交**

```
git add src/ui/FloatWindow.h src/ui/FloatWindow.cpp tests/test_ui.cpp
git commit -m "feat(ui): 浮窗表头三态排序 + 指示器 + 别名/排序数据管道"
```

---

### Task 6: 浮窗行内右键「自定义名称…」

**Files:**
- Modify: `src/ui/QuoteModel.h`
- Modify: `src/ui/QuoteModel.cpp`
- Modify: `src/ui/FloatWindow.cpp`
- Test: `tests/test_ui.cpp`

- [ ] **Step 1: 写失败测试**

在 `tests/test_ui.cpp` 的 `headerClickOnKLineColumnIsIgnored()` 之后追加：

```cpp
    void setAliasUpdatesModelImmediately() {
        ProbeWindow w(baseConfig());
        w.show();
        QTest::qWait(200);

        auto* table = w.findChild<QTableView*>();
        QVERIFY(table);
        w.pushQuotes(twoQuotes());

        const int col = columnOf(table->model(), "名称");
        QVERIFY(col >= 0);
        QCOMPARE(cellText(table->model(), 0, col), QString("浦发银行"));

        w.setAlias("600000", "浦发(老仓)");
        QCOMPARE(w.currentConfig().value("name_map").toObject().value("sh600000").toString(),
                 QString("浦发(老仓)"));
        QCOMPARE(cellText(table->model(), 0, col), QString("浦发(老仓)"));

        w.setAlias("600000", "   ");  // 留空 = 恢复行情名称
        QCOMPARE(w.currentConfig().value("name_map").toObject().size(), 0);
        QCOMPARE(cellText(table->model(), 0, col), QString("浦发银行"));
    }

    void contextMenuOnRowOffersAliasEdit() {
        ProbeWindow w(baseConfig());
        w.show();
        QTest::qWait(200);

        auto* table = w.findChild<QTableView*>();
        QVERIFY(table);
        w.pushQuotes(twoQuotes());

        QMenu* menu = w.menuAt(table->viewport()->mapToGlobal(QPoint(5, 5)));
        QVERIFY(menu);
        QVERIFY(!menu->actions().isEmpty());
        QCOMPARE(menu->actions().first()->text(), QString("自定义名称…"));
        menu->deleteLater();

        QMenu* blank = w.menuAt(w.mapToGlobal(QPoint(2, w.height() - 2)));
        QVERIFY(blank);
        QVERIFY(!blank->actions().isEmpty());
        QVERIFY(blank->actions().first()->text() != QString("自定义名称…"));
        blank->deleteLater();
    }
```

- [ ] **Step 2: 构建并确认失败**

Run: `scripts\build.cmd`
Expected: 运行 `build\test_ui.exe` 时 `setAliasUpdatesModelImmediately` 与 `contextMenuOnRowOffersAliasEdit` 通过，但 `ProbeWindow::menuAt` 调用的 `buildContextMenu` 尚无"自定义名称…"项 → `FAIL!  : ... Compared values are not the same`（first action 是「显示指标」）。
（若此时仍编译失败，请先完成 Task 5 Step 3。）

- [ ] **Step 3: 实现**

`src/ui/QuoteModel.h`：public 区 `int klineColumn() const;` 之后加入：

```cpp
    QString quoteCodeAt(int row) const;
    QString quoteNameAt(int row) const;
```

`src/ui/QuoteModel.cpp`：`klineColumn()` 之后加入：

```cpp
QString QuoteModel::quoteCodeAt(int row) const {
    return (row >= 0 && row < m_quotes.size()) ? m_quotes.at(row).code : QString();
}

QString QuoteModel::quoteNameAt(int row) const {
    return (row >= 0 && row < m_quotes.size()) ? m_quotes.at(row).name : QString();
}
```

`src/ui/FloatWindow.cpp`：include 区加入：

```cpp
#include <QInputDialog>
#include <QLineEdit>
```

把现有的 `FloatWindow::showContextMenu(const QPoint& globalPos)` **整个函数**（函数体全部内容 + 末尾的 `menu.exec(globalPos);`）替换为下面两个函数：

```cpp
void FloatWindow::showContextMenu(const QPoint& globalPos) {
    QMenu* menu = buildContextMenu(globalPos);
    menu->exec(globalPos);
    menu->deleteLater();
}

QMenu* FloatWindow::buildContextMenu(const QPoint& globalPos) {
    auto* menu = new QMenu(this);
    const QModelIndex hit = m_table->indexAt(m_table->viewport()->mapFromGlobal(globalPos));
    if (hit.isValid()) {
        const QString code = m_model->quoteCodeAt(hit.row());
        const QString name = m_model->quoteNameAt(hit.row());
        menu->addAction(QStringLiteral("自定义名称…"), this, [this, code, name] {
            bool ok = false;
            const QString input = QInputDialog::getText(
                this, QStringLiteral("自定义名称"),
                QStringLiteral("%1（%2）\n留空 = 使用行情名称").arg(code, name), QLineEdit::Normal,
                NameAlias::aliasFor(m_nameMap, code), &ok);
            if (ok) setAlias(code, input);
        });
        menu->addSeparator();
    }
    QMenu* cols = menu->addMenu(QStringLiteral("显示指标"));
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
    auto* actHeader = menu->addAction(QStringLiteral("显示表头"));
    actHeader->setCheckable(true);
    actHeader->setChecked(m_headerVisible);
    connect(actHeader, &QAction::toggled, this, [this](bool on) {
        m_headerVisible = on;
        m_table->horizontalHeader()->setVisible(on);
        refitSize();
        notifyChanged();
    });
    auto* actGrid = menu->addAction(QStringLiteral("显示网格"));
    actGrid->setCheckable(true);
    actGrid->setChecked(m_gridVisible);
    connect(actGrid, &QAction::toggled, this, [this](bool on) {
        m_gridVisible = on;
        m_columnCfg[QStringLiteral("grid_visible")] = on;
        applyStyle();
        notifyChanged();
    });
    auto* actColor = menu->addAction(QStringLiteral("默认颜色"));
    actColor->setCheckable(true);
    actColor->setChecked(m_defaultColor);
    connect(actColor, &QAction::toggled, this, [this](bool on) {
        m_defaultColor = on;
        m_columnCfg[QStringLiteral("default_color")] = on;
        applyFontAndMetrics();
        applyStyle();
        notifyChanged();
    });
    menu->addSeparator();
    menu->addAction(QStringLiteral("设置…"), this, [this] {
        if (m_openSettings) m_openSettings();
    });
    menu->addSeparator();
    menu->addAction(QStringLiteral("隐藏浮窗"), this, &QWidget::hide);
    return menu;
}
```

即：整段替换后，菜单第一项在“右键落在某一行”时为「自定义名称…」，其余菜单项与行为保持原样。

若 `QAction` 未包含，include 区已有 `#include <QAction>`（现有代码已用到，无需新增）。

- [ ] **Step 4: 构建并运行测试**

Run: `scripts\build.cmd` 然后 `build\test_ui.exe`
Expected: `Totals: 16 passed, 0 failed`

- [ ] **Step 5: 提交**

```
git add src/ui/QuoteModel.h src/ui/QuoteModel.cpp src/ui/FloatWindow.cpp tests/test_ui.cpp
git commit -m "feat(ui): 浮窗行内右键自定义名称（QuoteModel 暴露 code/name）"
```

---

### Task 7: 设置面板名称显示 + 自选列表别名编辑

**Files:**
- Modify: `src/ui/SettingsDialog.h`
- Modify: `src/ui/SettingsDialog.cpp`
- Test: `tests/test_ui.cpp`

- [ ] **Step 1: 写失败测试**

在 `tests/test_ui.cpp` 的 `contextMenuOnRowOffersAliasEdit()` 之后追加：

```cpp
    void settingsNameLengthComboWritesConfig() {
        ProbeWindow w(baseConfig());
        w.show();
        QTest::qWait(150);

        SettingsDialog dlg(&w, &w);
        dlg.show();
        QTest::qWait(50);
        dlg.findChild<QTabWidget*>()->setCurrentIndex(1);  // 显示数据页
        QTest::qWait(50);

        auto* combo = dlg.findChild<QComboBox*>("nameLengthCombo");
        QVERIFY(combo);
        QCOMPARE(combo->count(), 5);  // 全称 / 1~4 字
        combo->setCurrentIndex(combo->findData(2));
        QTest::qWait(30);
        QCOMPARE(w.currentConfig().value("name_length").toInt(), 2);
        combo->setCurrentIndex(combo->findData(0));
        QTest::qWait(30);
        QCOMPARE(w.currentConfig().value("name_length").toInt(), 0);
        dlg.close();
    }

    void settingsListShowsAliasAndApplyCodeEditWritesMap() {
        ProbeWindow w(baseConfig());
        w.show();
        QTest::qWait(150);

        SettingsDialog dlg(&w, &w);
        dlg.show();
        QTest::qWait(50);

        auto* list = dlg.findChild<QListWidget*>("codeList");
        QVERIFY(list);
        QCOMPARE(list->count(), 1);
        QCOMPARE(list->item(0)->text(), QString("sh600000"));

        QVERIFY(dlg.applyCodeEdit("600000", "浦发(老仓)"));
        QTest::qWait(30);
        QCOMPARE(list->item(0)->text(), QString("sh600000  浦发(老仓)"));
        QCOMPARE(w.currentConfig().value("name_map").toObject().value("sh600000").toString(),
                 QString("浦发(老仓)"));

        QVERIFY(dlg.applyCodeEdit("600000", ""));
        QTest::qWait(30);
        QCOMPARE(list->item(0)->text(), QString("sh600000"));
        QCOMPARE(w.currentConfig().value("name_map").toObject().size(), 0);

        QVERIFY(!dlg.applyCodeEdit("not-a-code", "x"));  // 非法代码被拒
        dlg.close();
    }

    void doubleClickListItemEditsAlias() {
        ProbeWindow w(baseConfig());
        w.show();
        QTest::qWait(150);

        SettingsDialog dlg(&w, &w);
        dlg.show();
        QTest::qWait(50);

        auto* list = dlg.findChild<QListWidget*>("codeList");
        QVERIFY(list);
        QVERIFY(list->count() == 1);

        bool sawDialog = false;
        QTimer::singleShot(200, [&sawDialog] {
            QWidget* modal = QApplication::activeModalWidget();
            if (!modal) return;
            sawDialog = modal->objectName() == QString("codeEditDialog");
            if (auto* alias = modal->findChild<QLineEdit*>("aliasEdit"))
                alias->setText(QStringLiteral("浦发(老仓)"));
            if (auto* box = modal->findChild<QDialogButtonBox*>())
                if (auto* ok = box->button(QDialogButtonBox::Ok)) ok->click();
        });
        const QRect rect = list->visualItemRect(list->item(0));
        QTest::mouseDClick(list->viewport(), Qt::LeftButton, Qt::NoModifier, rect.center());
        QTest::qWait(50);

        QVERIFY(sawDialog);
        QCOMPARE(list->item(0)->text(), QString("sh600000  浦发(老仓)"));
        QCOMPARE(w.currentConfig().value("name_map").toObject().value("sh600000").toString(),
                 QString("浦发(老仓)"));
        dlg.close();
    }
```

- [ ] **Step 2: 构建并确认失败**

Run: `scripts\build.cmd`
Expected: 编译失败，错误形如 `error C2039: "applyCodeEdit": 不是 "SettingsDialog" 的成员`

- [ ] **Step 3: 实现**

`src/ui/SettingsDialog.h`：

1) public 区 `static SettingsDialog* showFor(...)` 之后加入：

```cpp
    // 校验并落库一条「代码 + 别名」；item 为空则按代码查找/新建。非法代码返回 false
    bool applyCodeEdit(const QString& code, const QString& alias, QListWidgetItem* item = nullptr);
```

2) private 区 `void ensureTab(int index);` 之前加入：

```cpp
    void commitCodes();
    void editCodeItem(QListWidgetItem* item);
```

3) 成员区 `QComboBox* m_interval = nullptr;` 之后加入：

```cpp
    QComboBox* m_nameLength = nullptr;
```

`src/ui/SettingsDialog.cpp`：

1) include 区加入：

```cpp
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QMessageBox>
#include <QSignalBlocker>
```

2) `buildDataTab()` 里，`lay->addWidget(flags);` 之后、`connect(m_interval, ...)` 之前插入：

```cpp
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
```

3) `buildCodesTab()` 里，列表填充段整体替换为（注意 `Qt::UserRole` 存代码、`Qt::UserRole + 1` 存别名）：

```cpp
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
```

4) `buildCodesTab()` 的 `addCode` lambda 整体替换为：

```cpp
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
```

5) `buildCodesTab()` 里 `m_searchEdit = new QLineEdit(group);` 之后加入一行（便于测试定位）：

```cpp
    m_searchEdit->setObjectName(QStringLiteral("searchEdit"));
```

6) `buildCodesTab()` 里 `connect(m_codeList, &QListWidget::itemChanged, ...)` 之前加入：

```cpp
    connect(m_codeList, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem* it) { editCodeItem(it); });
```

7) 在 `buildCodesTab()` 函数之后新增三个成员函数：

```cpp
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
```

- [ ] **Step 4: 构建并运行测试**

Run: `scripts\build.cmd` 然后 `build\test_ui.exe`
Expected: `Totals: 19 passed, 0 failed`

- [ ] **Step 5: 提交**

```
git add src/ui/SettingsDialog.h src/ui/SettingsDialog.cpp tests/test_ui.cpp
git commit -m "feat(settings): 名称显示下拉 + 自选列表别名显示与双击编辑"
```

---

### Task 8: 文档与全量验证

**Files:**
- Modify: `README.md`

- [ ] **Step 1: 更新 README**

1) 「✨ 功能」列表里「股票代码管理」这一条的末尾追加一行：

```markdown
* **名称显示与自定义映射**：名称列可设为「全称」或截断 1~4 字；可为任一代码指定自定义显示名（设置面板双击自选项，或在浮窗某一行上右键「自定义名称…」，留空即恢复行情名称）。
* **点击表头排序**：点任意指标表头 → 降序，再点 → 升序，第三次 → 取消排序回到自选顺序；排序状态自动保存，每次刷新实时重排（同值行保持自选顺序）。
```

2) 「⚙️ 设置面板」一节里，「**自选列表**」那条改为：

```markdown
* **自选列表**：增/删/改/上移/下移 + 勾选要在浮窗中显示的股票；顶部搜索框支持「不带前缀的代码」直接添加与「名称/代码片段」模糊搜索点选；**双击某项**可同时改代码与该代码的自定义名称（留空 = 使用行情名称）。
```

3) 「**显示数据**」那条改为：

```markdown
* **显示数据**：刷新间隔；12 列独立开关；名称显示长度（全称 / 1~4 字）。
```

- [ ] **Step 2: 全量构建 + 测试**

Run:

```
scripts\build.cmd
scripts\test.cmd
```

Expected: 构建成功；ctest 全部通过，形如 `100% tests passed, 0 tests failed out of 9`
（9 个测试目标：codes / sorter / namealias / parser / config / model / schedule / suggest / ui）

- [ ] **Step 3: 手工冒烟（可选但推荐）**

Run: `build\StockWidget.exe`
检查：
1. 设置 → 显示数据 → 名称显示选「全称 / 2 字」，浮窗「名称」列即时变化
2. 设置 → 自选列表 → 双击一项 → 填自定义名称 → 浮窗该行名称变为别名
3. 浮窗某行右键 → 「自定义名称…」→ 留空确定 → 恢复行情名称
4. 点「涨跌幅」表头三次：降 → 升 → 取消；重启程序后排序状态保留

- [ ] **Step 4: 提交**

```
git add README.md
git commit -m "docs: README 补充名称显示/自定义映射/表头排序说明"
```

> 发布（`v1.6.0`）由用户决定后再打标签：`git tag v1.6.0 && git push origin v1.6.0`，由 `.github/workflows/release.yml` 自动出包。本计划不含打标签步骤。

---

## 完成判据

- [ ] `scripts\build.cmd` 无警告级错误，`scripts\test.cmd` 全绿（9 个测试目标）
- [ ] `name_map` / `name_length` / `sort_key` / `sort_asc` 四个键读写闭环，老配置行为不变
- [ ] 别名优先级、别名不被 `name_length` 截断、排序用最终显示值 —— 均有测试固化
- [ ] 表头点击不隐藏窗口、不触发拖动 —— `test_ui` 有断言
- [ ] `QuoteModel` 的 diff 缓存结构与 `KLineDelegate` 未被修改（`git diff` 校对）
