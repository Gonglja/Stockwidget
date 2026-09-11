# StockWidget Qt/C++ 重构 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 用 Qt 6 C++ 从零重写 StockWidget 透明盯盘浮窗，功能与现有 PySide6 版本完整对等，并消除拖动/刷新/设置面板的卡顿。

**Architecture:** 单 GUI 线程 + `QNetworkAccessManager` 异步拉取（GUI 永不阻塞）；`QuoteModel` 逐单元格 diff，仅 `dataChanged` 变化格；列宽/行高缓存，刷新路径零 `resizeColumnsToContents`、零 `setStyleSheet`；拖动期间只 `move()`；设置对话框单例常驻、标签页懒构造、字体列表缓存。纯逻辑（代码规格化 / 解析 / 配置迁移 / 模型）抽入 `StockWidgetCore` 静态库以便单测。

**Tech Stack:** Qt 6.11.1 (msvc2022_64，本机 `C:/1/Qt/6.11.1`)、CMake + Ninja、Qt 模块 `Core Gui Widgets Network Core5Compat Test`、Win32 `RegisterHotKey`、Windows 注册表。无第三方库。

**设计文档:** `docs/superpowers/specs/2026-09-11-stockwidget-qt-refactor-design.md`

---

## 文件结构（决策锁定）

| 文件 | 职责 |
|---|---|
| `CMakeLists.txt` | 顶层工程：`StockWidget` 可执行 + `StockWidgetCore` 静态库 |
| `scripts/build.cmd` / `scripts/test.cmd` | 封装 vcvars + cmake 构建 / ctest |
| `src/main.cpp` | 入口：AppUserModelID + `Application` |
| `src/app/Application.{h,cpp}` | 托盘、图标、设置对话框编排、退出、退出前保存 |
| `src/app/ConfigStore.{h,cpp}` | `SW_config.json` 读取/原子写入 + 旧键迁移 |
| `src/ui/FloatWindow.{h,cpp}` | 无框透明浮窗：布局/绘制/拖动/右键/刷新 |
| `src/ui/QuoteModel.{h,cpp}` | `QAbstractTableModel`，增量 diff |
| `src/ui/KLineDelegate.{h,cpp}` | 当日 K 线绘制 |
| `src/ui/SettingsDialog.{h,cpp}` | 4 页设置，单例、懒构造 |
| `src/data/Quote.h` | 单只股票解析结果结构体 + 格式选项 |
| `src/data/StockCode.{h,cpp}` | 代码规格化/去重（纯函数） |
| `src/data/QuoteColumns.{h,cpp}` | 列定义表：标题、配置键、对齐、取文本/取符号 |
| `src/data/QuoteParser.{h,cpp}` | 纯解析：文本 → `QVector<Quote>` |
| `src/data/SinaQuoteSource.{h,cpp}` | QNAM 异步请求 + GBK 解码 + 超时 |
| `src/platform/GlobalHotkey.{h,cpp}` | Win32 `RegisterHotKey` + 原生事件过滤 |
| `src/platform/AutoStart.{h,cpp}` | HKCU Run 键读写 |
| `tests/CMakeLists.txt` | 4 个 QTest 目标 |
| `tests/test_codes.cpp` / `test_parser.cpp` / `test_config.cpp` / `test_model.cpp` | 单元测试 |
| `resources/StockWidget.ico` | 图标（已存在，从仓库根复制） |

---

## Task 1: Qt 6 安装 + CMake 骨架 + 空窗跑通

**Files:**
- Create: `CMakeLists.txt`
- Create: `scripts/build.cmd`
- Create: `scripts/test.cmd`
- Create: `src/main.cpp`

- [ ] **Step 1: 校验本机 Qt 6.11.1**

本机已安装 Qt，无需下载：

```bash
ls C:/1/Qt/6.11.1/msvc2022_64/bin/qmake.exe
ls C:/1/Qt/6.11.1/msvc2022_64/lib/cmake | grep -E 'Qt6(Core5Compat|Network|Widgets|Test)$'
```

预期：qmake.exe 存在，且 Core5Compat / Network / Widgets / Test 四个模块的 cmake 目录都在。
（套件：`msvc2022_64`；`mingw_64` 也存在但不用。）

- [ ] **Step 2: 写构建脚本 `scripts/build.cmd`**

```bat
@echo off
setlocal
set "ROOT=%~dp0.."
set "QT=C:/1/Qt/6.11.1/msvc2022_64"
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
set "PATH=%QT%\bin;%PATH%"
cmake -S "%ROOT%" -B "%ROOT%\build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=%QT% || exit /b 1
cmake --build "%ROOT%\build" || exit /b 1
```

- [ ] **Step 3: 写测试脚本 `scripts/test.cmd`**

```bat
@echo off
setlocal
set "QT=C:/1/Qt/6.11.1/msvc2022_64"
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
set "PATH=%QT%\bin;%PATH%"
ctest --test-dir "%~dp0..\build" --output-on-failure || exit /b 1
```

- [ ] **Step 4: 写顶层 `CMakeLists.txt`**

```cmake
cmake_minimum_required(VERSION 3.21)
project(StockWidget VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets Network Core5Compat Test)
qt_standard_project_setup()

qt_add_executable(StockWidget WIN32
    src/main.cpp
)
target_include_directories(StockWidget PRIVATE src)
target_link_libraries(StockWidget PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::Core5Compat
)

qt_add_resources(StockWidget "app_resources"
    PREFIX "/"
    BASE resources
    FILES resources/StockWidget.ico
)

enable_testing()
```

- [ ] **Step 5: 写占位 `src/main.cpp`**

```cpp
#include <QApplication>
#include <QLabel>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QLabel w(QStringLiteral("StockWidget Qt skeleton"));
    w.show();
    return app.exec();
}
```

- [ ] **Step 6: 复制图标资源**

```bash
mkdir -p resources && cp StockWidget.ico resources/StockWidget.ico
```

- [ ] **Step 7: 构建**

Run: `cmd /c scripts\build.cmd`
Expected: `[build]` 无错误，生成 `build/StockWidget.exe`。

- [ ] **Step 8: 冒烟运行**

Run: `build\StockWidget.exe`
Expected: 弹出一个标题为 `StockWidget Qt skeleton` 的窗口；关闭窗口后进程退出。

- [ ] **Step 9: 提交**

```bash
git add CMakeLists.txt scripts/build.cmd scripts/test.cmd src/main.cpp resources/StockWidget.ico
git commit -m "build: Qt6+CMake 骨架与构建脚本"
```

---

## Task 2: StockCode 代码规格化（TDD）

**Files:**
- Create: `src/data/StockCode.h`
- Create: `src/data/StockCode.cpp`
- Create: `tests/CMakeLists.txt`
- Create: `tests/test_codes.cpp`
- Modify: `CMakeLists.txt`（新增 `StockWidgetCore` 库、`add_subdirectory(tests)`）

- [ ] **Step 1: 写失败测试 `tests/test_codes.cpp`**

```cpp
#include <QtTest>
#include "data/StockCode.h"

class TestCodes : public QObject {
    Q_OBJECT
private slots:
    void normalizesShanghai() {
        QCOMPARE(StockCode::normalize("600000").value(), QString("sh600000"));
        QCOMPARE(StockCode::normalize("900901").value(), QString("sh900901"));
        QCOMPARE(StockCode::normalize("510300").value(), QString("sh510300"));
        QCOMPARE(StockCode::normalize("  SH600000 ").value(), QString("sh600000"));
    }
    void normalizesShenzhen() {
        QCOMPARE(StockCode::normalize("000001").value(), QString("sz000001"));
        QCOMPARE(StockCode::normalize("300136").value(), QString("sz300136"));
        QCOMPARE(StockCode::normalize("159915").value(), QString("sz159915"));
    }
    void normalizesBeijing() {
        QCOMPARE(StockCode::normalize("830799").value(), QString("bj830799"));
        QCOMPARE(StockCode::normalize("430047").value(), QString("bj430047"));
        QCOMPARE(StockCode::normalize("920001").value(), QString("bj920001"));
    }
    void rejectsInvalid() {
        QVERIFY(!StockCode::normalize("").has_value());
        QVERIFY(!StockCode::normalize("abc").has_value());
        QVERIFY(!StockCode::normalize("12345").has_value());
        QVERIFY(!StockCode::normalize("700000").has_value());
    }
    void dedupesPreservingOrder() {
        const QStringList in{"600000", "sh600000", "000001", "sz000001", "bad"};
        QCOMPARE(StockCode::normalizeList(in), QStringList({"sh600000", "sz000001"}));
    }
};

QTEST_GUILESS_MAIN(TestCodes)
#include "test_codes.moc"
```

- [ ] **Step 2: 建 `tests/CMakeLists.txt`**

```cmake
qt_add_executable(test_codes test_codes.cpp)
target_link_libraries(test_codes PRIVATE Qt6::Test StockWidgetCore)
add_test(NAME test_codes COMMAND test_codes)
```

- [ ] **Step 3: 运行测试确认失败（编译报错：StockCode.h 不存在）**

Run: `cmd /c scripts\build.cmd`
Expected: FAIL，`fatal error: 'data/StockCode.h' file not found`。

- [ ] **Step 4: 写 `src/data/StockCode.h`**

```cpp
#pragma once
#include <QString>
#include <QStringList>
#include <optional>

namespace StockCode {
std::optional<QString> normalize(const QString& input);
QStringList normalizeList(const QStringList& inputs);
}
```

- [ ] **Step 5: 写 `src/data/StockCode.cpp`**

```cpp
#include "data/StockCode.h"
#include <QRegularExpression>
#include <QSet>

namespace {
const QRegularExpression kPrefixed(QStringLiteral("^(sh|sz|bj)\\d+$"));
const QRegularExpression kNumeric(QStringLiteral("^\\d{6}$"));
}

std::optional<QString> StockCode::normalize(const QString& input) {
    QString s = input.trimmed().toLower();
    s.remove(QRegularExpression(QStringLiteral("[^a-z0-9]")));
    if (s.isEmpty()) return std::nullopt;
    if (kPrefixed.match(s).hasMatch()) return s;
    if (kNumeric.match(s).hasMatch()) {
        if (s.startsWith(QStringLiteral("6")) || s.startsWith(QStringLiteral("90")) ||
            s.startsWith(QStringLiteral("5")))
            return QStringLiteral("sh") + s;
        if (s.startsWith(QStringLiteral("0")) || s.startsWith(QStringLiteral("3")) ||
            s.startsWith(QStringLiteral("2")) || s.startsWith(QStringLiteral("1")))
            return QStringLiteral("sz") + s;
        if (s.startsWith(QStringLiteral("8")) || s.startsWith(QStringLiteral("4")) ||
            s.startsWith(QStringLiteral("92")))
            return QStringLiteral("bj") + s;
    }
    return std::nullopt;
}

QStringList StockCode::normalizeList(const QStringList& inputs) {
    QStringList out;
    QSet<QString> seen;
    for (const QString& in : inputs) {
        const auto n = normalize(in);
        if (!n || seen.contains(*n)) continue;
        seen.insert(*n);
        out.append(*n);
    }
    return out;
}
```

- [ ] **Step 6: 修改 `CMakeLists.txt` 注册核心库与测试**

```cmake
# 在 find_package 之后、qt_add_executable(StockWidget ...) 之前插入：
add_library(StockWidgetCore STATIC
    src/data/StockCode.cpp
)
target_include_directories(StockWidgetCore PUBLIC src)
target_link_libraries(StockWidgetCore PUBLIC Qt6::Core Qt6::Gui)

# 在 enable_testing() 之后插入：
add_subdirectory(tests)
```

- [ ] **Step 7: 构建并运行测试确认通过**

Run: `cmd /c scripts\build.cmd && cmd /c scripts\test.cmd`
Expected: `100% tests passed, 1 tests passed`。

- [ ] **Step 8: 提交**

```bash
git add src/data/StockCode.h src/data/StockCode.cpp tests/CMakeLists.txt tests/test_codes.cpp CMakeLists.txt
git commit -m "feat: StockCode 代码规格化与去重"
```

---

## Task 3: Quote 结构体 + QuoteParser（TDD）

**Files:**
- Create: `src/data/Quote.h`
- Create: `src/data/QuoteParser.h`
- Create: `src/data/QuoteParser.cpp`
- Create: `tests/test_parser.cpp`
- Modify: `tests/CMakeLists.txt`
- Modify: `CMakeLists.txt`（向 `StockWidgetCore` 加 `src/data/QuoteParser.cpp`）

字段顺序依据新浪 `hq.sinajs.cn`：`0名称,1今开,2昨收,3现价,4最高,5最低,6买一价,7卖一价,8成交量,9成交额,10..18买量,11..19买价,20..28卖量,21..29卖价,30日期,31时间`。

- [ ] **Step 1: 写失败测试 `tests/test_parser.cpp`**

```cpp
#include <QtTest>
#include "data/QuoteParser.h"

static const char* kPufa =
    "var hq_str_sh600000=\"浦发银行,10.100,10.000,10.200,10.300,9.900,"
    "10.190,10.200,8000,81600.000,"
    "100,10.180,200,10.170,300,10.160,400,10.150,500,10.140,"
    "600,10.210,700,10.220,800,10.230,900,10.240,1000,10.250,"
    "2026-09-11,15:00:00,00\";\n";

class TestParser : public QObject {
    Q_OBJECT
private slots:
    void parsesContinuousTrading() {
        QuoteFormatOptions opt; opt.shortCode = false; opt.nameLength = 2;
        const auto q = QuoteParser::parseText(QString::fromUtf8(kPufa), opt);
        QCOMPARE(q.size(), 1);
        const Quote& s = q.first();
        QCOMPARE(s.code, QString("sh600000"));
        QCOMPARE(s.name, QString("浦发"));
        QCOMPARE(s.priceText, QString("10.20 "));
        QCOMPARE(s.changeText, QString("+0.20"));
        QCOMPARE(s.changePctText, QString("+2.00%"));
        QCOMPARE(s.deltaSign, 1);
        QCOMPARE(s.b1Text, QString("1 "));
        QCOMPARE(s.s1Text, QString("6 "));
        QCOMPARE(s.b1Sign, 1);
        QCOMPARE(s.s1Sign, -1);
        QCOMPARE(s.committeeText, QString("-45.45%"));
        QCOMPARE(s.volumeText, QString("8000"));
        QCOMPARE(s.amountText, QString("8.16万"));
        QCOMPARE(s.avgText, QString("10.20"));
    }
    void shortCodeAndFullName() {
        QuoteFormatOptions opt; opt.shortCode = true; opt.nameLength = 0;
        const auto q = QuoteParser::parseText(QString::fromUtf8(kPufa), opt);
        QCOMPARE(q.first().code, QString("600000"));
        QCOMPARE(q.first().name, QString("浦发银行"));
    }
    void etfUsesThreeDecimals() {
        const QString line = QStringLiteral(
            "var hq_str_sh510300=\"沪深300,1.200,1.100,1.234,1.300,1.000,"
            "1.230,1.240,5000,6170.000,"
            "100,1.220,200,1.210,300,1.200,400,1.190,500,1.180,"
            "600,1.250,700,1.260,800,1.270,900,1.280,1000,1.290,"
            "2026-09-11,15:00:00,00\";\n");
        const auto q = QuoteParser::parseText(line, QuoteFormatOptions{});
        QCOMPARE(q.first().priceText, QString("1.234 "));
        QCOMPARE(q.first().changeText, QString("+0.134"));
    }
    void callAuctionShowsPairedAndUnpaired() {
        const QString line = QStringLiteral(
            "var hq_str_sh600000=\"浦发银行,10.000,10.000,10.000,10.000,10.000,"
            "10.000,10.000,1000,10000.000,"
            "100,10.000,200,9.990,300,9.980,400,9.970,500,9.960,"
            "600,10.000,700,10.010,800,10.020,900,10.030,1000,10.040,"
            "2026-09-11,09:20:00,00\";\n");
        const auto q = QuoteParser::parseText(line, QuoteFormatOptions{});
        QCOMPARE(q.first().b1Text, QString("6"));      // paired 600/100 = 6
        QCOMPARE(q.first().s1Text, QString("+7"));     // unpaired 700/100 = 7
        QCOMPARE(q.first().b1Sign, 1);                 // 买方优势
    }
    void skipsDirtyAndEmptyLines() {
        const QString text = QStringLiteral(
            "garbage line\n"
            "var hq_str_sh600000=\"\";\n"
            "var hq_str_sh600001=\"only,three,fields\";\n");
        QVERIFY(QuoteParser::parseText(text, QuoteFormatOptions{}).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestParser)
#include "test_parser.moc"
```

- [ ] **Step 2: 追加到 `tests/CMakeLists.txt`**

```cmake
qt_add_executable(test_parser test_parser.cpp)
target_link_libraries(test_parser PRIVATE Qt6::Test StockWidgetCore)
add_test(NAME test_parser COMMAND test_parser)
```

- [ ] **Step 3: 运行测试确认失败**

Run: `cmd /c scripts\build.cmd`
Expected: FAIL，`'data/QuoteParser.h' file not found`。

- [ ] **Step 4: 写 `src/data/Quote.h`**

```cpp
#pragma once
#include <QMetaType>
#include <QString>
#include <QVector>

struct Quote {
    QString code;
    QString name;
    double open = 0, prevClose = 0, price = 0, high = 0, low = 0;
    double buy1 = 0, sell1 = 0, volume = 0, amount = 0;
    double avg = 0, committee = 0;
    bool etf = false;

    QString priceText, changeText, changePctText;
    QString b1Text, s1Text, committeeText, volumeText, amountText, avgText;
    int deltaSign = 0, committeeSign = 0, avgSign = 0, b1Sign = 0, s1Sign = 0;

    double kOpen = 0, kClose = 0, kHigh = 0, kLow = 0, kPrev = 0;
};

struct QuoteFormatOptions {
    enum class B1S1Display { Qty, Price, Both };
    bool shortCode = false;
    int nameLength = 0;
    B1S1Display b1s1 = B1S1Display::Qty;
};

Q_DECLARE_METATYPE(Quote)
```

- [ ] **Step 5: 写 `src/data/QuoteParser.h`**

```cpp
#pragma once
#include "data/Quote.h"

namespace QuoteParser {
QVector<Quote> parseText(const QString& text, const QuoteFormatOptions& opt);
}
```

- [ ] **Step 6: 写 `src/data/QuoteParser.cpp`**

```cpp
#include "data/QuoteParser.h"
#include <QtMath>

namespace {

QString formatPrice(double v, bool etf) {
    return QString::number(v, 'f', etf ? 3 : 2);
}

QString signedNumber(qint64 v) {
    return (v > 0 ? QStringLiteral("+") : QString()) + QString::number(v);
}

QString formatVolume(double v) {
    if (v < 1e4) return QString::number(qint64(v));
    if (v < 1e8) return QString::number(v / 1e4, 'f', 2) + QStringLiteral("万");
    return QString::number(v / 1e8, 'f', 2) + QStringLiteral("亿");
}

QString formatAmount(double v) {
    if (v < 1e8) return QString::number(v / 1e4, 'f', 2) + QStringLiteral("万");
    if (v < 1e12) return QString::number(v / 1e8, 'f', 2) + QStringLiteral("亿");
    return QString::number(v / 1e12, 'f', 2) + QStringLiteral("万亿");
}

double toNum(const QString& s) { return s.isEmpty() ? 0.0 : s.toDouble(); }
qint64 toInt(const QString& s) { return s.isEmpty() ? 0LL : s.toLongLong(); }

}  // namespace

QVector<Quote> QuoteParser::parseText(const QString& text, const QuoteFormatOptions& opt) {
    QVector<Quote> out;
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (const QString& rawLine : lines) {
        const QString line = rawLine.trimmed();
        const int eq = line.indexOf(QStringLiteral("=\""));
        if (eq < 0) continue;

        const QString code = line.left(eq).split(QLatin1Char('_')).last().trimmed();
        QString payload = line.mid(eq + 2);
        const QStringList parts = payload.split(QLatin1Char(','));
        if (parts.size() < 32) continue;

        Quote q;
        q.code = code;
        q.name = parts.at(0);
        if (q.name.isEmpty()) continue;

        q.open = toNum(parts.at(1));
        q.prevClose = toNum(parts.at(2));
        q.price = toNum(parts.at(3));
        q.high = toNum(parts.at(4));
        q.low = toNum(parts.at(5));
        q.buy1 = toNum(parts.at(6));
        q.sell1 = toNum(parts.at(7));
        q.volume = toNum(parts.at(8));
        q.amount = toNum(parts.at(9));

        QVector<qint64> buyVols, sellVols;
        for (int i : {10, 12, 14, 16, 18}) buyVols.append(toInt(parts.at(i)));
        for (int i : {20, 22, 24, 26, 28}) sellVols.append(toInt(parts.at(i)));

        q.etf = code.size() >= 3 &&
                (code.at(2) == QLatin1Char('1') || code.at(2) == QLatin1Char('5'));
        const int dec = q.etf ? 3 : 2;
        const double scale = std::pow(10.0, dec);
        auto almostEq = [scale](double a, double b) {
            return qRound64(a * scale) == qRound64(b * scale);
        };

        QString buyMarker = QStringLiteral(" ");
        QString sellMarker = QStringLiteral(" ");
        if (q.buy1 > 0 && almostEq(q.price, q.buy1)) buyMarker = QStringLiteral("<");
        if (q.sell1 > 0 && almostEq(q.price, q.sell1)) sellMarker = QStringLiteral(">");

        const auto mode = opt.b1s1;
        if (q.buy1 == q.sell1 && q.buy1 > 0) {
            q.price = q.sell1;  // 集合竞价 9:15~9:25 / 14:57~15:00
            const qint64 paired = sellVols.at(0);
            const qint64 unpaired = (sellVols.at(1) > 0) ? -sellVols.at(1) : buyVols.at(1);
            const qint64 pairedCnt = paired / 100;
            const qint64 unpairedCnt = unpaired / 100;
            const QString bp = formatPrice(q.buy1, q.etf);
            const QString sp = formatPrice(q.sell1, q.etf);
            if (mode == QuoteFormatOptions::B1S1Display::Price) {
                q.b1Text = bp;
                q.s1Text = sp;
            } else if (mode == QuoteFormatOptions::B1S1Display::Both) {
                q.b1Text = QString::number(pairedCnt) + "(" + bp + ")";
                q.s1Text = signedNumber(unpairedCnt) + "(" + sp + ")";
            } else {
                q.b1Text = QString::number(pairedCnt);
                q.s1Text = signedNumber(unpairedCnt);
            }
            if (unpaired > 0) q.b1Sign = q.s1Sign = 1;
            else if (unpaired < 0) q.b1Sign = q.s1Sign = -1;
        } else {
            if (q.buy1 > 0) {
                const QString cnt = QString::number(buyVols.at(0) / 100);
                const QString bp = formatPrice(q.buy1, q.etf);
                if (mode == QuoteFormatOptions::B1S1Display::Price) q.b1Text = bp + buyMarker;
                else if (mode == QuoteFormatOptions::B1S1Display::Both)
                    q.b1Text = cnt + "(" + bp + ")" + buyMarker;
                else q.b1Text = cnt + buyMarker;
            } else {
                q.b1Text = QStringLiteral("-") + buyMarker;
            }
            if (q.sell1 > 0) {
                const QString cnt = QString::number(sellVols.at(0) / 100);
                const QString sp = formatPrice(q.sell1, q.etf);
                if (mode == QuoteFormatOptions::B1S1Display::Price) q.s1Text = sellMarker + sp;
                else if (mode == QuoteFormatOptions::B1S1Display::Both)
                    q.s1Text = sellMarker + cnt + "(" + sp + ")";
                else q.s1Text = sellMarker + cnt;
            } else {
                q.s1Text = sellMarker + QStringLiteral("-");
            }
            q.b1Sign = 1;
            q.s1Sign = -1;
        }

        if (q.price == 0) q.price = q.prevClose;  // 9:00~9:15 无数据
        if (q.open == 0) {
            q.open = q.price;
            q.high = q.price;
            q.low = q.price;
        }

        const double change = q.prevClose ? q.price - q.prevClose : 0.0;
        const double changePct = q.prevClose ? (q.price / q.prevClose - 1) * 100.0 : 0.0;
        q.avg = q.volume > 0 ? q.amount / q.volume : q.prevClose;

        qint64 pSum = 0, sSum = 0;
        for (qint64 v : buyVols) pSum += v;
        for (qint64 v : sellVols) sSum += v;
        q.committee = (pSum + sSum) > 0 ? 100.0 * (pSum - sSum) / (pSum + sSum) : 0.0;

        QString arrow = QStringLiteral(" ");
        if (q.high > q.low) {
            if (q.price == q.high) arrow = QStringLiteral("↑");
            else if (q.price == q.low) arrow = QStringLiteral("↓");
        }

        q.priceText = formatPrice(q.price, q.etf) + arrow;
        q.changeText = (change >= 0 ? QStringLiteral("+") : QString()) +
                       QString::number(change, 'f', dec);
        q.changePctText = (changePct >= 0 ? QStringLiteral("+") : QString()) +
                          QString::number(changePct, 'f', 2) + QStringLiteral("%");
        q.committeeText = (q.committee >= 0 ? QStringLiteral("+") : QString()) +
                          QString::number(q.committee, 'f', 2) + QStringLiteral("%");
        q.avgText = QString::number(q.avg, 'f', dec);
        q.volumeText = formatVolume(q.volume);
        q.amountText = formatAmount(q.amount);

        q.deltaSign = (change > 0) - (change < 0);
        q.committeeSign = (q.committee > 0) - (q.committee < 0);
        q.avgSign = (q.avg > q.prevClose) - (q.avg < q.prevClose);

        q.kOpen = q.open;
        q.kClose = q.price;
        q.kHigh = q.high;
        q.kLow = q.low;
        q.kPrev = q.prevClose;

        if (opt.nameLength > 0) q.name = q.name.left(opt.nameLength);
        if (opt.shortCode && q.code.size() > 2) q.code = q.code.mid(2);

        out.append(q);
    }
    return out;
}
```

- [ ] **Step 7: 向 `CMakeLists.txt` 的 `StockWidgetCore` 添加源文件**

```cmake
add_library(StockWidgetCore STATIC
    src/data/StockCode.cpp
    src/data/QuoteParser.cpp
)
```

- [ ] **Step 8: 构建并运行测试确认全部通过**

Run: `cmd /c scripts\build.cmd && cmd /c scripts\test.cmd`
Expected: `100% tests passed, 2 tests passed`。

- [ ] **Step 9: 提交**

```bash
git add src/data/Quote.h src/data/QuoteParser.h src/data/QuoteParser.cpp tests/test_parser.cpp tests/CMakeLists.txt CMakeLists.txt
git commit -m "feat: Quote 结构与新浪行情解析器"
```

---

## Task 4: ConfigStore 配置读写 + 旧键迁移（TDD）

**Files:**
- Create: `src/data/QuoteColumns.h`
- Create: `src/data/QuoteColumns.cpp`
- Create: `src/app/ConfigStore.h`
- Create: `src/app/ConfigStore.cpp`
- Create: `tests/test_config.cpp`
- Modify: `tests/CMakeLists.txt`
- Modify: `CMakeLists.txt`

`QuoteColumns` 先建立「列定义表」，`ConfigStore` 迁移复用它（DRY）。

- [ ] **Step 1: 写失败测试 `tests/test_config.cpp`**

```cpp
#include <QtTest>
#include <QDir>
#include <QTemporaryDir>
#include "app/ConfigStore.h"
#include "data/QuoteColumns.h"

class TestConfig : public QObject {
    Q_OBJECT
private slots:
    void init() {
        m_tmp = new QTemporaryDir();
        QVERIFY(m_tmp->isValid());
        qputenv("SW_CONFIG_DIR", m_tmp->path().toUtf8());
    }
    void cleanup() { delete m_tmp; qunsetenv("SW_CONFIG_DIR"); }

    void migratesLegacyFlags() {
        QJsonObject raw;
        raw["flags"] = QJsonArray{false, false, true, false, true, false,
                                  false, false, false, false, false, false};
        raw["b1s1_price"] = true;
        raw["visible_codes"] = QJsonArray{"sh600000"};
        const QJsonObject out = ConfigStore::normalize(raw);
        QCOMPARE(out.value("price_visible").toBool(), true);
        QCOMPARE(out.value("change_pct_visible").toBool(), true);
        QCOMPARE(out.value("b1s1_display").toString(), QString("price"));
        QCOMPARE(out.value("checked_codes").toArray().first().toString(), QString("sh600000"));
        QVERIFY(!out.contains("flags"));
    }
    void roundTripsToDisk() {
        QJsonObject cfg;
        cfg["refresh_seconds"] = 5;
        cfg["codes"] = QJsonArray{"sh600000"};
        QVERIFY(ConfigStore::save(cfg));
        const QJsonObject back = ConfigStore::load();
        QCOMPARE(back.value("refresh_seconds").toInt(), 5);
        QCOMPARE(back.value("codes").toArray().first().toString(), QString("sh600000"));
    }
    void corruptFileFallsBackToEmpty() {
        const QString path = ConfigStore::configFilePath();
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path); QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("{ not json"); f.close();
        QVERIFY(ConfigStore::load().isEmpty());
    }
    void columnKeyMapping() {
        QCOMPARE(QuoteColumns::configKeyFor("现价"), QString("price_visible"));
        QCOMPARE(QuoteColumns::configKeyFor("卖一"), QString("b1s1_visible"));
        QCOMPARE(QuoteColumns::allHeaders().size(), 12);
    }
};

QTEST_GUILESS_MAIN(TestConfig)
#include "test_config.moc"
```

- [ ] **Step 2: 追加到 `tests/CMakeLists.txt`**

```cmake
qt_add_executable(test_config test_config.cpp)
target_link_libraries(test_config PRIVATE Qt6::Test StockWidgetCore)
add_test(NAME test_config COMMAND test_config)
```

- [ ] **Step 3: 运行确认失败**

Run: `cmd /c scripts\build.cmd`
Expected: FAIL，`'app/ConfigStore.h' file not found`。

- [ ] **Step 4: 写 `src/data/QuoteColumns.h`**

```cpp
#pragma once
#include "data/Quote.h"
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <functional>

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
bool isVisible(const QJsonObject& cfg, const QString& header);
QVector<ColumnSpec> activeColumns(const QJsonObject& cfg);
}
```

- [ ] **Step 5: 写 `src/data/QuoteColumns.cpp`**

```cpp
#include "data/QuoteColumns.h"

namespace {
struct Entry {
    const char* header;
    const char* key;
    bool right;
    bool colored;
};
const Entry kEntries[] = {
    {"代码",   "code_visible",       true,  false},
    {"名称",   "name_visible",       false, false},
    {"现价",   "price_visible",      true,  true},
    {"涨跌值", "change_visible",     true,  true},
    {"涨跌幅", "change_pct_visible", true,  true},
    {"买一",   "b1s1_visible",       true,  true},
    {"卖一",   "b1s1_visible",       false, true},
    {"委比",   "commi_visible",      true,  true},
    {"成交量", "vol_visible",        true,  false},
    {"成交额", "amount_visible",     true,  false},
    {"均价",   "avg_visible",        true,  true},
    {"K线",    "kline_visible",      false, false},
};
}  // namespace

QStringList QuoteColumns::allHeaders() {
    QStringList out;
    for (const Entry& e : kEntries) out.append(QString::fromUtf8(e.header));
    return out;
}

QString QuoteColumns::configKeyFor(const QString& header) {
    for (const Entry& e : kEntries)
        if (QString::fromUtf8(e.header) == header) return QString::fromUtf8(e.key);
    return QString();
}

bool QuoteColumns::isVisible(const QJsonObject& cfg, const QString& header) {
    const QString key = configKeyFor(header);
    return !key.isEmpty() && cfg.value(key).toBool(false);
}

QVector<ColumnSpec> QuoteColumns::activeColumns(const QJsonObject& cfg) {
    using namespace std::placeholders;
    QVector<ColumnSpec> cols;
    for (const Entry& e : kEntries) {
        const QString header = QString::fromUtf8(e.header);
        if (!isVisible(cfg, header)) continue;

        ColumnSpec spec;
        spec.header = header;
        spec.rightAlign = e.right;
        spec.colored = e.colored;
        spec.isKLine = header == QStringLiteral("K线");
        spec.text = [header](const Quote& q) -> QString {
            if (header == QStringLiteral("代码")) return q.code;
            if (header == QStringLiteral("名称")) return q.name;
            if (header == QStringLiteral("现价")) return q.priceText;
            if (header == QStringLiteral("涨跌值")) return q.changeText;
            if (header == QStringLiteral("涨跌幅")) return q.changePctText;
            if (header == QStringLiteral("买一")) return q.b1Text;
            if (header == QStringLiteral("卖一")) return q.s1Text;
            if (header == QStringLiteral("委比")) return q.committeeText;
            if (header == QStringLiteral("成交量")) return q.volumeText;
            if (header == QStringLiteral("成交额")) return q.amountText;
            if (header == QStringLiteral("均价")) return q.avgText;
            return QString();
        };
        spec.sign = [header](const Quote& q) -> int {
            if (header == QStringLiteral("现价") || header == QStringLiteral("涨跌值") ||
                header == QStringLiteral("涨跌幅")) return q.deltaSign;
            if (header == QStringLiteral("买一")) return q.b1Sign;
            if (header == QStringLiteral("卖一")) return q.s1Sign;
            if (header == QStringLiteral("委比")) return q.committeeSign;
            if (header == QStringLiteral("均价")) return q.avgSign;
            return 0;
        };
        cols.append(spec);
    }
    return cols;
}
```

- [ ] **Step 6: 写 `src/app/ConfigStore.h`**

```cpp
#pragma once
#include <QJsonObject>
#include <QString>

namespace ConfigStore {
QString configFilePath();
QJsonObject load();
bool save(const QJsonObject& cfg);
QJsonObject normalize(const QJsonObject& raw);
}
```

- [ ] **Step 7: 写 `src/app/ConfigStore.cpp`**

```cpp
#include "app/ConfigStore.h"
#include "data/QuoteColumns.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>

namespace {

QString configDir() {
    const QString override = qEnvironmentVariable("SW_CONFIG_DIR");
    if (!override.isEmpty()) return override;
    QString base = qEnvironmentVariable("APPDATA");
    if (base.isEmpty()) base = QDir::homePath();
    return base + QStringLiteral("/StockWidget");
}

}  // namespace

QString ConfigStore::configFilePath() {
    return configDir() + QStringLiteral("/SW_config.json");
}

QJsonObject ConfigStore::load() {
    QFile f(configFilePath());
    if (!f.open(QIODevice::ReadOnly)) return {};
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) return {};
    return doc.object();
}

bool ConfigStore::save(const QJsonObject& cfg) {
    QDir().mkpath(configDir());
    QSaveFile f(configFilePath());
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(QJsonDocument(cfg).toJson(QJsonDocument::Indented));
    return f.commit();
}

QJsonObject ConfigStore::normalize(const QJsonObject& raw) {
    QJsonObject out = raw;

    if (!out.contains(QStringLiteral("checked_codes")) &&
        out.contains(QStringLiteral("visible_codes"))) {
        out.insert(QStringLiteral("checked_codes"), out.value(QStringLiteral("visible_codes")));
    }

    if (out.contains(QStringLiteral("flags"))) {
        QJsonObject oldFlags;
        const QJsonValue flags = out.value(QStringLiteral("flags"));
        const QStringList headers = QuoteColumns::allHeaders();
        if (flags.isArray()) {
            const QJsonArray arr = flags.toArray();
            for (int i = 0; i < headers.size() && i < arr.size(); ++i)
                oldFlags.insert(headers.at(i), arr.at(i).toBool());
        } else if (flags.isObject()) {
            const QJsonObject obj = flags.toObject();
            for (const QString& h : headers) oldFlags.insert(h, obj.value(h).toBool());
        }
        for (const QString& h : headers) {
            const QString key = QuoteColumns::configKeyFor(h);
            if (key.isEmpty() || key == QStringLiteral("b1s1_visible")) continue;
            if (!out.contains(key)) out.insert(key, oldFlags.value(h).toBool(false));
        }
        if (!out.contains(QStringLiteral("b1s1_visible"))) {
            out.insert(QStringLiteral("b1s1_visible"),
                       oldFlags.value(QStringLiteral("买一")).toBool(false) ||
                           oldFlags.value(QStringLiteral("卖一")).toBool(false));
        }
        out.remove(QStringLiteral("flags"));
    }

    if (!out.contains(QStringLiteral("b1s1_display"))) {
        out.insert(QStringLiteral("b1s1_display"),
                   out.value(QStringLiteral("b1s1_price")).toBool(false)
                       ? QStringLiteral("price")
                       : QStringLiteral("qty"));
    }
    return out;
}
```

- [ ] **Step 8: 向 `CMakeLists.txt` 的 `StockWidgetCore` 添加源文件**

```cmake
add_library(StockWidgetCore STATIC
    src/data/StockCode.cpp
    src/data/QuoteColumns.cpp
    src/data/QuoteParser.cpp
    src/app/ConfigStore.cpp
)
```

- [ ] **Step 9: 构建并运行测试确认通过**

Run: `cmd /c scripts\build.cmd && cmd /c scripts\test.cmd`
Expected: `100% tests passed, 3 tests passed`。

- [ ] **Step 10: 提交**

```bash
git add src/data/QuoteColumns.* src/app/ConfigStore.* tests/test_config.cpp tests/CMakeLists.txt CMakeLists.txt
git commit -m "feat: 列定义表与配置存储（含旧键迁移）"
```

---

## Task 5: QuoteModel 增量 diff 模型（TDD）

**Files:**
- Create: `src/ui/QuoteModel.h`
- Create: `src/ui/QuoteModel.cpp`
- Create: `tests/test_model.cpp`
- Modify: `tests/CMakeLists.txt`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: 写失败测试 `tests/test_model.cpp`**

```cpp
#include <QtTest>
#include <QAbstractItemModelTester>
#include <QSignalSpy>
#include "ui/QuoteModel.h"

static Quote makeQuote(double price, double prevClose) {
    Quote q;
    q.code = "sh600000"; q.name = "浦发";
    q.price = price; q.prevClose = prevClose;
    q.priceText = QString::number(price, 'f', 2);
    q.changeText = QString::number(price - prevClose, 'f', 2);
    q.deltaSign = (price > prevClose) - (price < prevClose);
    q.kOpen = 10; q.kClose = price; q.kHigh = 11; q.kLow = 9; q.kPrev = prevClose;
    return q;
}

class TestModel : public QObject {
    Q_OBJECT
private slots:
    void respectsColumnConfig() {
        QuoteModel m;
        QJsonObject cfg;
        cfg["price_visible"] = true;
        cfg["name_visible"] = true;
        m.setColumns(QuoteColumns::activeColumns(cfg));
        QAbstractItemModelTester tester(&m);
        m.setQuotes({makeQuote(10.5, 10.0)});
        QCOMPARE(m.rowCount(), 1);
        QCOMPARE(m.columnCount(), 2);
        QCOMPARE(m.headerData(0, Qt::Horizontal).toString(), QString("名称"));
        QCOMPARE(m.headerData(1, Qt::Horizontal).toString(), QString("现价"));
    }
    void emitsDataChangedOnlyForChangedCells() {
        QuoteModel m;
        QJsonObject cfg; cfg["price_visible"] = true;
        m.setColumns(QuoteColumns::activeColumns(cfg));
        m.setQuotes({makeQuote(10.5, 10.0)});
        QSignalSpy spy(&m, &QAbstractItemModel::dataChanged);
        m.setQuotes({makeQuote(10.5, 10.0)});
        QCOMPARE(spy.count(), 0);
        m.setQuotes({makeQuote(10.6, 10.0)});
        QCOMPARE(spy.count(), 1);
        const auto args = spy.takeFirst();
        QCOMPARE(args.at(0).toModelIndex().column(), 0);
    }
    void exposesKLinePayload() {
        QuoteModel m;
        QJsonObject cfg; cfg["kline_visible"] = true;
        m.setColumns(QuoteColumns::activeColumns(cfg));
        m.setQuotes({makeQuote(10.5, 10.0)});
        const QVariant v = m.data(m.index(0, 0), QuoteModel::KLineRole);
        QCOMPARE(v.toList().size(), 5);
    }
    void colorsBySign() {
        QuoteModel m;
        QJsonObject cfg; cfg["price_visible"] = true;
        m.setColumns(QuoteColumns::activeColumns(cfg));
        m.setColorScheme(true, Qt::white);
        m.setQuotes({makeQuote(10.6, 10.0)});
        QCOMPARE(m.data(m.index(0, 0), Qt::ForegroundRole).value<QColor>(),
                 QColor(0xdd, 0x21, 0x00));
    }
};

QTEST_GUILESS_MAIN(TestModel)
#include "test_model.moc"
```

- [ ] **Step 2: 追加到 `tests/CMakeLists.txt`**

```cmake
qt_add_executable(test_model test_model.cpp)
target_link_libraries(test_model PRIVATE Qt6::Test StockWidgetCore)
add_test(NAME test_model COMMAND test_model)
```

- [ ] **Step 3: 运行确认失败**

Run: `cmd /c scripts\build.cmd`
Expected: FAIL，`'ui/QuoteModel.h' file not found`。

- [ ] **Step 4: 写 `src/ui/QuoteModel.h`**

```cpp
#pragma once
#include "data/Quote.h"
#include "data/QuoteColumns.h"
#include <QAbstractTableModel>
#include <QColor>

class QuoteModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Roles { KLineRole = Qt::UserRole + 1 };
    explicit QuoteModel(QObject* parent = nullptr);

    void setColumns(const QVector<ColumnSpec>& cols);
    void setQuotes(const QVector<Quote>& quotes);
    void setColorScheme(bool defaultColor, const QColor& fg);
    int klineColumn() const;

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

private:
    void rebuildCache();
    QVector<ColumnSpec> m_cols;
    QVector<Quote> m_quotes;
    QVector<QVector<QString>> m_cellCache;
    QVector<QVector<int>> m_signCache;
    bool m_defaultColor = false;
    QColor m_fg = QColor(QStringLiteral("#FFFFFF"));
};
```

- [ ] **Step 5: 写 `src/ui/QuoteModel.cpp`**

```cpp
#include "ui/QuoteModel.h"

QuoteModel::QuoteModel(QObject* parent) : QAbstractTableModel(parent) {}

void QuoteModel::setColumns(const QVector<ColumnSpec>& cols) {
    beginResetModel();
    m_cols = cols;
    rebuildCache();
    endResetModel();
}

void QuoteModel::setQuotes(const QVector<Quote>& quotes) {
    if (quotes.size() != m_quotes.size()) {
        beginResetModel();
        m_quotes = quotes;
        rebuildCache();
        endResetModel();
        return;
    }
    QVector<QVector<QString>> newText(quotes.size());
    QVector<QVector<int>> newSign(quotes.size());
    for (int r = 0; r < quotes.size(); ++r) {
        newText[r].resize(m_cols.size());
        newSign[r].resize(m_cols.size());
        for (int c = 0; c < m_cols.size(); ++c) {
            newText[r][c] = m_cols[c].text ? m_cols[c].text(quotes[r]) : QString();
            newSign[r][c] = m_cols[c].sign ? m_cols[c].sign(quotes[r]) : 0;
        }
    }
    m_quotes = quotes;
    for (int r = 0; r < quotes.size(); ++r) {
        int first = -1, last = -1;
        for (int c = 0; c < m_cols.size(); ++c) {
            const bool changed = newText[r][c] != m_cellCache[r][c] ||
                                 newSign[r][c] != m_signCache[r][c];
            if (changed && first < 0) first = c;
            if (changed) last = c;
        }
        if (first >= 0) emit dataChanged(index(r, first), index(r, last));
    }
    m_cellCache = newText;
    m_signCache = newSign;
}

void QuoteModel::rebuildCache() {
    m_cellCache.clear();
    m_signCache.clear();
    m_cellCache.resize(m_quotes.size());
    m_signCache.resize(m_quotes.size());
    for (int r = 0; r < m_quotes.size(); ++r) {
        m_cellCache[r].resize(m_cols.size());
        m_signCache[r].resize(m_cols.size());
        for (int c = 0; c < m_cols.size(); ++c) {
            m_cellCache[r][c] = m_cols[c].text ? m_cols[c].text(m_quotes[r]) : QString();
            m_signCache[r][c] = m_cols[c].sign ? m_cols[c].sign(m_quotes[r]) : 0;
        }
    }
}

void QuoteModel::setColorScheme(bool defaultColor, const QColor& fg) {
    m_defaultColor = defaultColor;
    m_fg = fg;
    if (rowCount() > 0 && columnCount() > 0)
        emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1),
                         {Qt::ForegroundRole});
}

int QuoteModel::klineColumn() const {
    for (int c = 0; c < m_cols.size(); ++c)
        if (m_cols[c].isKLine) return c;
    return -1;
}

int QuoteModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : m_quotes.size();
}

int QuoteModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : m_cols.size();
}

QVariant QuoteModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= m_quotes.size() || index.column() >= m_cols.size())
        return {};
    const ColumnSpec& col = m_cols.at(index.column());
    const int r = index.row();
    switch (role) {
        case Qt::DisplayRole:
            return col.isKLine ? QString() : m_cellCache[r][index.column()];
        case Qt::TextAlignmentRole:
            return int((col.rightAlign ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter);
        case Qt::ForegroundRole: {
            if (!m_defaultColor || !col.colored) return m_fg;
            const int sign = m_signCache[r][index.column()];
            if (sign > 0) return QColor(0xdd, 0x21, 0x00);
            if (sign < 0) return QColor(0x01, 0x99, 0x33);
            return QColor(0x49, 0x49, 0x49);
        }
        case KLineRole: {
            if (!col.isKLine) return {};
            const Quote& q = m_quotes.at(r);
            return QVariantList{q.kOpen, q.kClose, q.kHigh, q.kLow, q.kPrev};
        }
        default:
            return {};
    }
}

QVariant QuoteModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal && section < m_cols.size())
        return m_cols.at(section).header;
    return {};
}
```

- [ ] **Step 6: 向 `CMakeLists.txt` 的 `StockWidgetCore` 添加源文件**

```cmake
add_library(StockWidgetCore STATIC
    src/data/StockCode.cpp
    src/data/QuoteColumns.cpp
    src/data/QuoteParser.cpp
    src/ui/QuoteModel.cpp
    src/app/ConfigStore.cpp
)
```

- [ ] **Step 7: 构建并运行测试确认通过**

Run: `cmd /c scripts\build.cmd && cmd /c scripts\test.cmd`
Expected: `100% tests passed, 4 tests passed`。

- [ ] **Step 8: 提交**

```bash
git add src/ui/QuoteModel.* tests/test_model.cpp tests/CMakeLists.txt CMakeLists.txt
git commit -m "feat: QuoteModel 增量 diff 模型"
```

---

## Task 6: SinaQuoteSource 异步拉取

**Files:**
- Create: `src/data/SinaQuoteSource.h`
- Create: `src/data/SinaQuoteSource.cpp`
- Modify: `CMakeLists.txt`

无单元测试（依赖真实网络）；用 Step 4 的临时探针程序人工验证。

- [ ] **Step 1: 写 `src/data/SinaQuoteSource.h`**

```cpp
#pragma once
#include "data/Quote.h"
#include <QNetworkAccessManager>
#include <QObject>

class QNetworkReply;

class SinaQuoteSource : public QObject {
    Q_OBJECT
public:
    explicit SinaQuoteSource(QObject* parent = nullptr);
    void setFormatOptions(const QuoteFormatOptions& opt);
    void fetch(const QStringList& codes);

signals:
    void quotesReady(const QVector<Quote>& quotes);
    void error(const QString& message);

private:
    QNetworkAccessManager m_nam;
    QuoteFormatOptions m_opt;
    QNetworkReply* m_reply = nullptr;
};
```

- [ ] **Step 2: 写 `src/data/SinaQuoteSource.cpp`**

```cpp
#include "data/SinaQuoteSource.h"
#include "data/QuoteParser.h"
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTextCodec>

SinaQuoteSource::SinaQuoteSource(QObject* parent) : QObject(parent) {}

void SinaQuoteSource::setFormatOptions(const QuoteFormatOptions& opt) { m_opt = opt; }

void SinaQuoteSource::fetch(const QStringList& codes) {
    if (codes.isEmpty()) {
        emit error(QStringLiteral("暂无数据，请添加自选"));
        return;
    }
    if (m_reply) {
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }

    QNetworkRequest req(QUrl(QStringLiteral("https://hq.sinajs.cn/list=") +
                             codes.join(QLatin1Char(','))));
    req.setRawHeader("Referer", "https://finance.sina.com.cn");
    req.setRawHeader("User-Agent", "Mozilla/5.0");
    req.setTransferTimeout(3000);

    m_reply = m_nam.get(req);
    connect(m_reply, &QNetworkReply::finished, this, [this] {
        QNetworkReply* reply = m_reply;
        m_reply = nullptr;
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit error(QStringLiteral("无网络连接"));
            return;
        }
        const QByteArray bytes = reply->readAll();
        QTextCodec* codec = QTextCodec::codecForName("GB18030");
        const QString text = codec ? codec->toUnicode(bytes) : QString::fromUtf8(bytes);
        emit quotesReady(QuoteParser::parseText(text, m_opt));
    });
}
```

- [ ] **Step 3: 向 `CMakeLists.txt` 的 `StockWidgetCore` 添加源文件**

```cmake
add_library(StockWidgetCore STATIC
    src/data/StockCode.cpp
    src/data/QuoteColumns.cpp
    src/data/QuoteParser.cpp
    src/data/SinaQuoteSource.cpp
    src/ui/QuoteModel.cpp
    src/app/ConfigStore.cpp
)
```

- [ ] **Step 4: 临时探针验证（改 `src/main.cpp`，验证后还原）**

```cpp
#include <QApplication>
#include <QDebug>
#include <QTimer>
#include "data/SinaQuoteSource.h"

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    SinaQuoteSource src;
    QObject::connect(&src, &SinaQuoteSource::quotesReady, [](const QVector<Quote>& q) {
        for (const Quote& s : q) qDebug() << s.code << s.name << s.priceText << s.changePctText;
        qApp->quit();
    });
    QObject::connect(&src, &SinaQuoteSource::error, [](const QString& e) {
        qDebug() << "ERROR:" << e; qApp->quit();
    });
    src.fetch({"sh600000", "sz000001"});
    QTimer::singleShot(5000, &app, &QCoreApplication::quit);
    return app.exec();
}
```

Run: `cmd /c scripts\build.cmd && build\StockWidget.exe`
Expected: 控制台打印两行行情；无卡顿。

- [ ] **Step 5: 还原 `src/main.cpp` 为 Task 1 的占位版本**

```cpp
#include <QApplication>
#include <QLabel>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QLabel w(QStringLiteral("StockWidget Qt skeleton"));
    w.show();
    return app.exec();
}
```

- [ ] **Step 6: 构建确认通过**

Run: `cmd /c scripts\build.cmd`
Expected: 无错误。

- [ ] **Step 7: 提交**

```bash
git add src/data/SinaQuoteSource.* CMakeLists.txt
git commit -m "feat: 新浪行情异步数据源"
```

---

## Task 7: KLineDelegate 当日 K 线绘制

**Files:**
- Create: `src/ui/KLineDelegate.h`
- Create: `src/ui/KLineDelegate.cpp`
- Modify: `CMakeLists.txt`（加入 `StockWidget` 目标，不是 Core）

- [ ] **Step 1: 写 `src/ui/KLineDelegate.h`**

```cpp
#pragma once
#include <QColor>
#include <QStyledItemDelegate>

class KLineDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit KLineDelegate(QObject* parent = nullptr);
    void setColorScheme(bool defaultColor, const QColor& fg);
    void setPointSize(int pt);
    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;

private:
    bool m_defaultColor = false;
    QColor m_fg = QColor(QStringLiteral("#FFFFFF"));
    double m_scale = 1.0;
};
```

- [ ] **Step 2: 写 `src/ui/KLineDelegate.cpp`**

```cpp
#include "ui/KLineDelegate.h"
#include "ui/QuoteModel.h"
#include <QPainter>

namespace {
const QColor kUp(0xdd, 0x21, 0x00);
const QColor kDown(0x01, 0x99, 0x33);
const QColor kNeutral(0x49, 0x49, 0x49);
}

KLineDelegate::KLineDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

void KLineDelegate::setColorScheme(bool defaultColor, const QColor& fg) {
    m_defaultColor = defaultColor;
    m_fg = fg;
}

void KLineDelegate::setPointSize(int pt) {
    m_scale = qBound(0.5, static_cast<double>(pt) / 12.0, 1.5);
}

void KLineDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                          const QModelIndex& index) const {
    const QVariantList k = index.data(QuoteModel::KLineRole).toList();
    if (k.size() != 5) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }
    double o = k[0].toDouble(), c = k[1].toDouble(), h = k[2].toDouble();
    double l = k[3].toDouble(), p = k[4].toDouble();
    if (h < l) std::swap(h, l);

    const QRect cell = option.rect;
    const QRect rect = cell.adjusted(2, 2, -2, -2);
    const int vpad = qMax(2, int(rect.height() * (0.12 + 0.06 * (m_scale - 1))));
    const QRect krect(rect.left(), rect.top() + vpad, rect.width(),
                      qMax(2, rect.height() - 2 * vpad));

    const double lo = qMin(l, p), hi = qMax(h, p);
    auto yFor = [&](double v) {
        const double y = (hi == lo) ? 0.5 : (v - lo) / (hi - lo);
        return krect.top() + (1.0 - y) * krect.height();
    };
    const double yO = yFor(o), yC = yFor(c), yH = yFor(h), yL = yFor(l), yP = yFor(p);

    painter->save();
    painter->setClipRect(cell);
    painter->setRenderHint(QPainter::Antialiasing, true);

    const int bodyW = qMax(5, qMin(int(krect.width() * 0.4 * m_scale), 10));
    const double x = krect.center().x();

    QColor dash(m_defaultColor ? kNeutral : m_fg);
    dash.setAlpha(180);
    painter->setPen(QPen(dash, 1, Qt::DashLine));
    painter->drawLine(QPointF(x - bodyW, yP), QPointF(x + bodyW, yP));

    QColor kcolor = m_fg;
    if (m_defaultColor) {
        if (c > o) kcolor = kUp;
        else if (c < o) kcolor = kDown;
        else kcolor = kNeutral;
    }

    const double top = qMin(yO, yC), bot = qMax(yO, yC);
    const double bodyH = qMax(2.0, bot - top);
    const double bodyX = x - bodyW / 2.0;

    painter->setPen(QPen(kcolor, 1));
    if (c != o) painter->drawRect(QRectF(bodyX, top, bodyW, bodyH));
    else painter->drawLine(QPointF(bodyX, yC), QPointF(bodyX + bodyW, yC));
    if (yH < top) painter->drawLine(QPointF(x, yH), QPointF(x, top));
    if (yL > bot) painter->drawLine(QPointF(x, bot), QPointF(x, yL));
    if (c < o) painter->fillRect(QRectF(bodyX, top, bodyW, bodyH), kcolor);

    painter->restore();
}
```

- [ ] **Step 3: 向 `CMakeLists.txt` 的 `StockWidget` 目标添加源文件**

```cmake
qt_add_executable(StockWidget WIN32
    src/main.cpp
    src/ui/KLineDelegate.cpp
)
```

- [ ] **Step 4: 构建确认通过**

Run: `cmd /c scripts\build.cmd`
Expected: 无错误。

- [ ] **Step 5: 提交**

```bash
git add src/ui/KLineDelegate.* CMakeLists.txt
git commit -m "feat: K 线绘制委托"
```

---

## Task 8: FloatWindow 浮窗（布局 / 绘制 / 拖动 / 右键 / 刷新）

**Files:**
- Create: `src/ui/FloatWindow.h`
- Create: `src/ui/FloatWindow.cpp`
- Modify: `CMakeLists.txt`

这是核心 UI。刷新路径**不得**出现 `setStyleSheet` / `resizeColumnsToContents`。

- [ ] **Step 1: 写 `src/ui/FloatWindow.h`**

```cpp
#pragma once
#include <QColor>
#include <QFont>
#include <QJsonObject>
#include <QWidget>
#include <functional>

class QLabel;
class QTableView;
class QTimer;
class QuoteModel;
class KLineDelegate;
class SinaQuoteSource;

class FloatWindow : public QWidget {
    Q_OBJECT
public:
    explicit FloatWindow(const QJsonObject& cfg, QWidget* parent = nullptr);

    QJsonObject currentConfig() const;
    void setOpenSettingsCallback(std::function<void()> cb) { m_openSettings = std::move(cb); }
    void stop();

signals:
    void configChanged();

protected:
    void paintEvent(QPaintEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    void applyConfig(const QJsonObject& cfg);
    void persistGeometry();
    void rebuildColumns();
    void applyFontAndMetrics();
    void refreshNow();
    void refitSize();
    void notifyChanged();
    void setHeaderFlag(const QString& header, bool on);
    void onQuotesReady(const QVector<Quote>& quotes);

    // 配置状态
    QStringList m_codes;
    QStringList m_checkedCodes;
    int m_refreshSeconds = 2;
    bool m_shortCode = false;
    int m_nameLength = 0;
    QuoteFormatOptions::B1S1Display m_b1s1 = QuoteFormatOptions::B1S1Display::Qty;
    bool m_headerVisible = false;
    bool m_gridVisible = false;
    bool m_defaultColor = false;
    int m_lineExtraPx = 1;
    int m_opacityPct = 90;
    QString m_fontFamily = QStringLiteral("Microsoft YaHei");
    int m_fontSize = 10;
    QString m_hotkey = QStringLiteral("Ctrl+Alt+F");
    bool m_startOnBoot = false;
    QString m_appIcon;
    QJsonObject m_columnCfg;

    QColor m_fg = QColor(QStringLiteral("#FFFFFF"));
    QColor m_bg = QColor(0, 0, 0, 191);
    QFont m_font;

    // UI
    QWidget* m_panel = nullptr;
    QTableView* m_table = nullptr;
    QLabel* m_errorLabel = nullptr;
    QuoteModel* m_model = nullptr;
    KLineDelegate* m_klineDelegate = nullptr;
    SinaQuoteSource* m_source = nullptr;
    QTimer* m_timer = nullptr;

    bool m_dragging = false;
    QPoint m_dragOffset;
    std::function<void()> m_openSettings;
    bool m_columnWidthsFrozen = false;
};
```

- [ ] **Step 2: 写 `src/ui/FloatWindow.cpp`**

```cpp
#include "ui/FloatWindow.h"
#include "data/SinaQuoteSource.h"
#include "ui/KLineDelegate.h"
#include "ui/QuoteModel.h"

#include <QAction>
#include <QApplication>
#include <QContextMenuEvent>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>

namespace {
const QJsonObject kEmptyCfg;

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
    setMouseTracking(false);

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

    QWidget* filtered[] = {m_panel, m_table, m_table->viewport(),
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

void FloatWindow::applyConfig(const QJsonObject& raw) {
    m_columnCfg = raw;
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
    m_b1s1 = b1s1FromString(raw.value(QStringLiteral("b1s1_display")).toString(
        raw.value(QStringLiteral("b1s1_price")).toBool(false) ? QStringLiteral("price")
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
    m_table->setShowGrid(m_gridVisible);
    setWindowOpacity(qBound(20, m_opacityPct, 100) / 100.0);

    QuoteFormatOptions opt;
    opt.shortCode = m_shortCode;
    opt.nameLength = m_nameLength;
    opt.b1s1 = m_b1s1;
    m_source->setFormatOptions(opt);

    applyFontAndMetrics();
    rebuildColumns();
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
    if (m_model->rowCount() != before) m_columnWidthsFrozen = false;  // 行数变化时重算列宽
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
    const int hh = m_table->horizontalHeader()->isVisible() ? m_table->horizontalHeader()->height() : 0;
    const int totalH = hh + 2 * m_table->frameWidth() + rows * m_table->verticalHeader()->defaultSectionSize();
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
    bg[QStringLiteral("r")] = m_bg.red(); bg[QStringLiteral("g")] = m_bg.green();
    bg[QStringLiteral("b")] = m_bg.blue(); bg[QStringLiteral("a")] = m_bg.alpha();
    cfg[QStringLiteral("bg")] = bg;
    QJsonObject pos;
    pos[QStringLiteral("x")] = x(); pos[QStringLiteral("y")] = y();
    cfg[QStringLiteral("pos")] = pos;
    return cfg;
}

void FloatWindow::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragOffset = e->globalPosition().toPoint() - frameGeometry().topLeft();
        setFocus(Qt::MouseFocusReason);
        e->accept();
    }
}

void FloatWindow::mouseMoveEvent(QMouseEvent* e) {
    if (m_dragging && (e->buttons() & Qt::LeftButton)) {
        move(e->globalPosition().toPoint() - m_dragOffset);
        e->accept();
    }
}

void FloatWindow::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton && m_dragging) {
        m_dragging = false;
        persistGeometry();
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
        if (me->button() == Qt::LeftButton) { m_dragging = false; hide(); return true; }
    } else if (event->type() == QEvent::MouseButtonPress) {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            m_dragging = true;
            m_dragOffset = me->globalPosition().toPoint() - frameGeometry().topLeft();
            return true;
        }
    } else if (event->type() == QEvent::MouseMove) {
        auto* me = static_cast<QMouseEvent*>(event);
        if (m_dragging && (me->buttons() & Qt::LeftButton)) {
            move(me->globalPosition().toPoint() - m_dragOffset);
            return true;
        }
    } else if (event->type() == QEvent::MouseButtonRelease) {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton && m_dragging) { m_dragging = false; persistGeometry(); return true; }
    }
    return QWidget::eventFilter(obj, event);
}

void FloatWindow::persistGeometry() { notifyChanged(); }

void FloatWindow::contextMenuEvent(QContextMenuEvent* event) {
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
        connect(act, &QAction::toggled, this, [this, header](bool on) { setHeaderFlag(header, on); });
    }
    auto* actHeader = menu.addAction(QStringLiteral("显示表头"));
    actHeader->setCheckable(true); actHeader->setChecked(m_headerVisible);
    connect(actHeader, &QAction::toggled, this, [this](bool on) {
        m_headerVisible = on; m_table->horizontalHeader()->setVisible(on); refitSize(); notifyChanged();
    });
    auto* actGrid = menu.addAction(QStringLiteral("显示网格"));
    actGrid->setCheckable(true); actGrid->setChecked(m_gridVisible);
    connect(actGrid, &QAction::toggled, this, [this](bool on) {
        m_gridVisible = on; m_table->setShowGrid(on); notifyChanged();
    });
    auto* actColor = menu.addAction(QStringLiteral("默认颜色"));
    actColor->setCheckable(true); actColor->setChecked(m_defaultColor);
    connect(actColor, &QAction::toggled, this, [this](bool on) {
        m_defaultColor = on; applyFontAndMetrics(); notifyChanged();
    });
    menu.addSeparator();
    menu.addAction(QStringLiteral("设置…"), this, [this] { if (m_openSettings) m_openSettings(); });
    menu.addSeparator();
    menu.addAction(QStringLiteral("隐藏浮窗"), this, &QWidget::hide);
    menu.exec(event->globalPos());
}

void FloatWindow::setHeaderFlag(const QString& header, bool on) {
    const QString key = QuoteColumns::configKeyFor(header);
    if (key.isEmpty()) return;
    m_columnCfg[key] = on;
    rebuildColumns();
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
```

- [ ] **Step 3: 添加缺失 include**

在 `src/ui/FloatWindow.cpp` 顶部补上：

```cpp
#include "data/StockCode.h"
#include "data/QuoteColumns.h"
#include <QJsonArray>
```

- [ ] **Step 4: 向 `CMakeLists.txt` 的 `StockWidget` 目标添加源文件**

```cmake
qt_add_executable(StockWidget WIN32
    src/main.cpp
    src/ui/FloatWindow.cpp
    src/ui/KLineDelegate.cpp
)
```

- [ ] **Step 5: 修改 `src/main.cpp` 为最小可见浮窗**

```cpp
#include <QApplication>
#include "ui/FloatWindow.h"

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QJsonObject cfg;
    cfg["checked_codes"] = QJsonArray{"sh600000"};
    cfg["price_visible"] = true;
    cfg["change_pct_visible"] = true;
    FloatWindow w(cfg);
    w.show();
    return app.exec();
}
```

- [ ] **Step 6: 构建并人工冒烟**

Run: `cmd /c scripts\build.cmd && build\StockWidget.exe`
Expected：右侧出现透明浮窗，1–3 秒内显示 `浦发银行` 现价与涨跌幅；**拖动时窗口流畅跟随**；右键菜单可开列、表头、网格、默认颜色；双击隐藏。

- [ ] **Step 7: 提交**

```bash
git add src/ui/FloatWindow.* src/main.cpp CMakeLists.txt
git commit -m "feat: 浮窗（透明绘制/异步刷新/拖动/右键菜单）"
```

---

## Task 9: SettingsDialog 设置面板（4 页，懒构造）

**Files:**
- Create: `src/ui/SettingsDialog.h`
- Create: `src/ui/SettingsDialog.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: 写 `src/ui/SettingsDialog.h`**

```cpp
#pragma once
#include <QDialog>
#include <functional>

class FloatWindow;
class QCheckBox;
class QComboBox;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QSlider;
class QTabWidget;

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    SettingsDialog(FloatWindow* win, QWidget* parent = nullptr);
    static SettingsDialog* showFor(FloatWindow* win, QWidget* parent = nullptr);

private:
    QWidget* buildCodesTab();
    QWidget* buildDataTab();
    QWidget* buildAppearanceTab();
    QWidget* buildGeneralTab();
    void ensureTab(int index);
    void pickForeground();
    void pickBackground();
    void pickIcon();

    FloatWindow* m_win = nullptr;
    QTabWidget* m_tabs = nullptr;
    QListWidget* m_codeList = nullptr;
    QCheckBox* m_shortCode = nullptr;
    QComboBox* m_nameLength = nullptr;
    QComboBox* m_interval = nullptr;
    QCheckBox* m_tableHeader = nullptr;
    QCheckBox* m_tableGrid = nullptr;
    QCheckBox* m_defaultColor = nullptr;
    QPushButton* m_fgButton = nullptr;
    QPushButton* m_bgButton = nullptr;
    QSlider* m_bgAlpha = nullptr;
    QSlider* m_winOpacity = nullptr;
    QSlider* m_fontSize = nullptr;
    QSlider* m_lineSpacing = nullptr;
    QComboBox* m_fontFamily = nullptr;
    QCheckBox* m_startOnBoot = nullptr;
    QComboBox* m_icon = nullptr;
    int m_builtTabs = 0;
};
```

- [ ] **Step 2: 写 `src/ui/SettingsDialog.cpp`**

```cpp
#include "ui/SettingsDialog.h"
#include "data/QuoteColumns.h"
#include "data/StockCode.h"
#include "ui/FloatWindow.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QFileDialog>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSlider>
#include <QTabWidget>
#include <QVBoxLayout>
#include <functional>

namespace {
SettingsDialog* g_instance = nullptr;
const QStringList kIntervals{"1", "2", "3", "5", "10", "15", "30", "60"};
}  // namespace

SettingsDialog::SettingsDialog(FloatWindow* win, QWidget* parent) : QDialog(parent), m_win(win) {
    setWindowTitle(QStringLiteral("设置"));
    setModal(false);
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    m_tabs = new QTabWidget(this);
    root->addWidget(m_tabs);
    m_tabs->addTab(new QWidget(), QStringLiteral("自选列表"));
    m_tabs->addTab(new QWidget(), QStringLiteral("显示数据"));
    m_tabs->addTab(new QWidget(), QStringLiteral("外观"));
    m_tabs->addTab(new QWidget(), QStringLiteral("常规"));
    connect(m_tabs, &QTabWidget::currentChanged, this, &SettingsDialog::ensureTab);
    ensureTab(0);
    resize(440, 420);
}

SettingsDialog* SettingsDialog::showFor(FloatWindow* win, QWidget* parent) {
    if (!g_instance) g_instance = new SettingsDialog(win, parent);
    g_instance->show();
    g_instance->raise();
    g_instance->activateWindow();
    return g_instance;
}

void SettingsDialog::ensureTab(int index) {
    if (m_builtTabs & (1 << index)) return;
    QWidget* page = nullptr;
    switch (index) {
        case 0: page = buildCodesTab(); break;
        case 1: page = buildDataTab(); break;
        case 2: page = buildAppearanceTab(); break;
        default: page = buildGeneralTab(); break;
    }
    QWidget* old = m_tabs->widget(index);
    m_tabs->removeTab(index);
    m_tabs->insertTab(index, page, QStringList{"自选列表", "显示数据", "外观", "常规"}.at(index));
    m_tabs->setCurrentIndex(index);
    delete old;
    m_builtTabs |= (1 << index);
}

QWidget* SettingsDialog::buildCodesTab() {
    auto* page = new QWidget();
    auto* lay = new QVBoxLayout(page);
    auto* group = new QGroupBox(QStringLiteral("自选列表"), page);
    auto* h = new QHBoxLayout(group);
    m_codeList = new QListWidget(group);
    m_codeList->setFixedWidth(150);
    const QJsonArray codes = m_win->currentConfig().value(QStringLiteral("codes")).toArray();
    const QJsonArray checked = m_win->currentConfig().value(QStringLiteral("checked_codes")).toArray();
    for (const QJsonValue& v : codes) {
        auto* item = new QListWidgetItem(v.toString(), m_codeList);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEditable);
        item->setCheckState(checked.contains(v) ? Qt::Checked : Qt::Unchecked);
    }
    auto* btnCol = new QVBoxLayout();
    auto addBtn = [&](const QString& text, auto fn) {
        auto* b = new QPushButton(text, group); b->setFixedWidth(60);
        connect(b, &QPushButton::clicked, this, fn); btnCol->addWidget(b);
    };
    auto commit = [this] {
        QStringList list, checkedList;
        for (int i = 0; i < m_codeList->count(); ++i) {
            const auto* it = m_codeList->item(i);
            const auto n = StockCode::normalize(it->text());
            if (n) { list << *n; if (it->checkState() == Qt::Checked) checkedList << *n; }
        }
        QJsonObject cfg = m_win->currentConfig();
        cfg["codes"] = QJsonArray::fromStringList(list);
        cfg["checked_codes"] = QJsonArray::fromStringList(checkedList);
        m_win->applyConfig(cfg);
    };
    addBtn(QStringLiteral("添加"), [this, commit] {
        auto* it = new QListWidgetItem(QStringLiteral("sh000001"), m_codeList);
        it->setFlags(it->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEditable);
        m_codeList->setCurrentItem(it); m_codeList->editItem(it); commit();
    });
    addBtn(QStringLiteral("删除"), [this, commit] {
        delete m_codeList->takeItem(m_codeList->currentRow()); commit();
    });
    addBtn(QStringLiteral("上移"), [this, commit] {
        const int r = m_codeList->currentRow();
        if (r > 0) { auto* it = m_codeList->takeItem(r); m_codeList->insertItem(r - 1, it); m_codeList->setCurrentRow(r - 1); commit(); }
    });
    addBtn(QStringLiteral("下移"), [this, commit] {
        const int r = m_codeList->currentRow();
        if (r >= 0 && r < m_codeList->count() - 1) { auto* it = m_codeList->takeItem(r); m_codeList->insertItem(r + 1, it); m_codeList->setCurrentRow(r + 1); commit(); }
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
    for (const QString& s : kIntervals) m_interval->addItem(s + QStringLiteral(" 秒"), s.toInt());
    m_interval->setCurrentIndex(m_interval->findData(cfg.value(QStringLiteral("refresh_seconds")).toInt(2)));
    auto* intervalLay = new QVBoxLayout(intervalGroup);
    intervalLay->addWidget(m_interval);
    lay->addWidget(intervalGroup);

    auto* flags = new QGroupBox(QStringLiteral("显示指标"), page);
    auto* grid = new QGridLayout(flags);
    int col = 0, row = 0;
    for (const QString& header : QuoteColumns::allHeaders()) {
        if (header == QStringLiteral("卖一")) continue;
        const QString label = header == QStringLiteral("买一") ? QStringLiteral("买一/卖一") : header;
        auto* cb = new QCheckBox(label, flags);
        cb->setChecked(QuoteColumns::isVisible(cfg, header));
        connect(cb, &QCheckBox::toggled, this, [this, header](bool on) {
            QJsonObject c = m_win->currentConfig();
            c[QuoteColumns::configKeyFor(header)] = on;
            m_win->applyConfig(c);
        });
        grid->addWidget(cb, row, col);
        if (++col == 3) { col = 0; ++row; }
    }
    lay->addWidget(flags);

    connect(m_interval, &QComboBox::currentIndexChanged, this, [this](int) {
        QJsonObject c = m_win->currentConfig();
        c["refresh_seconds"] = m_interval->currentData().toInt();
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
    m_bgAlpha = new QSlider(Qt::Horizontal, color); m_bgAlpha->setRange(0, 100);
    m_bgAlpha->setValue(int(qRound(bg.value(QStringLiteral("a")).toInt(191) / 2.55)));
    m_winOpacity = new QSlider(Qt::Horizontal, color); m_winOpacity->setRange(20, 100);
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
    static const QStringList families = QFontDatabase::families();
    m_fontFamily->addItems(families);
    m_fontFamily->setCurrentText(cfg.value(QStringLiteral("font_family")).toString());
    m_fontSize = new QSlider(Qt::Horizontal, font); m_fontSize->setRange(8, 15);
    m_fontSize->setValue(cfg.value(QStringLiteral("font_size")).toInt(10));
    m_lineSpacing = new QSlider(Qt::Horizontal, font); m_lineSpacing->setRange(0, 20);
    m_lineSpacing->setValue(cfg.value(QStringLiteral("line_extra_px")).toInt(1));
    fg->addWidget(new QLabel(QStringLiteral("字体"), font), 0, 0);
    fg->addWidget(m_fontFamily, 0, 1);
    fg->addWidget(new QLabel(QStringLiteral("字号"), font), 1, 0);
    fg->addWidget(m_fontSize, 1, 1);
    fg->addWidget(new QLabel(QStringLiteral("行距"), font), 2, 0);
    fg->addWidget(m_lineSpacing, 2, 1);
    lay->addWidget(font);

    auto update = [this](const char* key, const QJsonValue& value) {
        QJsonObject c = m_win->currentConfig(); c[QString::fromLatin1(key)] = value; m_win->applyConfig(c);
    };
    connect(m_tableHeader, &QCheckBox::toggled, this, [update](bool on) { update("header_visible", on); });
    connect(m_tableGrid, &QCheckBox::toggled, this, [update](bool on) { update("grid_visible", on); });
    connect(m_defaultColor, &QCheckBox::toggled, this, [this, update](bool on) {
        m_fgButton->setEnabled(!on); update("default_color", on);
    });
    connect(m_bgAlpha, &QSlider::valueChanged, this, [update](int v) {
        QJsonObject c = m_win->currentConfig();
        QJsonObject bgc = c.value("bg").toObject();
        bgc["a"] = int(qRound(v * 2.55)); c["bg"] = bgc; m_win->applyConfig(c);
    });
    connect(m_winOpacity, &QSlider::valueChanged, this, [update](int v) { update("opacity_pct", v); });
    connect(m_fontFamily, &QComboBox::currentTextChanged, this, [update](const QString& f) { update("font_family", f); });
    connect(m_fontSize, &QSlider::valueChanged, this, [update](int v) { update("font_size", v); });
    connect(m_lineSpacing, &QSlider::valueChanged, this, [update](int v) { update("line_extra_px", v); });
    connect(m_fgButton, &QPushButton::clicked, this, &SettingsDialog::pickForeground);
    connect(m_bgButton, &QPushButton::clicked, this, &SettingsDialog::pickBackground);
    return page;
}

QWidget* SettingsDialog::buildGeneralTab() {
    const QJsonObject cfg = m_win->currentConfig();
    auto* page = new QWidget();
    auto* lay = new QVBoxLayout(page);
    m_startOnBoot = new QCheckBox(QStringLiteral("开机启动"), page);
    m_startOnBoot->setChecked(cfg.value(QStringLiteral("start_on_boot")).toBool(false));
    lay->addWidget(m_startOnBoot);

    auto* iconGroup = new QGroupBox(QStringLiteral("程序图标"), page);
    auto* ih = new QHBoxLayout(iconGroup);
    m_icon = new QComboBox(iconGroup);
    m_icon->addItem(QStringLiteral("默认"), QStringLiteral("default"));
    m_icon->addItem(QStringLiteral("系统：计算机"), QStringLiteral("std:computer"));
    m_icon->addItem(QStringLiteral("系统：网络"), QStringLiteral("std:network"));
    m_icon->addItem(QStringLiteral("系统：文件夹"), QStringLiteral("std:folder"));
    m_icon->addItem(QStringLiteral("系统：文件"), QStringLiteral("std:file"));
    m_icon->addItem(QStringLiteral("系统：回收站"), QStringLiteral("std:trash"));
    auto* pick = new QPushButton(QStringLiteral("自定义图标…"), iconGroup);
    ih->addWidget(m_icon);
    ih->addWidget(pick);
    lay->addWidget(iconGroup);

    connect(m_startOnBoot, &QCheckBox::toggled, this, [this](bool on) {
        QJsonObject c = m_win->currentConfig(); c["start_on_boot"] = on; m_win->applyConfig(c);
    });
    connect(m_icon, &QComboBox::currentIndexChanged, this, [this](int) {
        QJsonObject c = m_win->currentConfig(); c["app_icon"] = m_icon->currentData().toString(); m_win->applyConfig(c);
    });
    connect(pick, &QPushButton::clicked, this, &SettingsDialog::pickIcon);
    return page;
}

void SettingsDialog::pickForeground() {
    const QColor c = QColorDialog::getColor(Qt::white, this, QStringLiteral("选择文字颜色"));
    if (!c.isValid()) return;
    QJsonObject cfg = m_win->currentConfig(); cfg["fg"] = c.name(QColor::HexRgb); m_win->applyConfig(cfg);
}

void SettingsDialog::pickBackground() {
    const QColor c = QColorDialog::getColor(Qt::black, this, QStringLiteral("选择背景颜色"));
    if (!c.isValid()) return;
    QJsonObject cfg = m_win->currentConfig();
    QJsonObject bg = cfg.value("bg").toObject();
    bg["r"] = c.red(); bg["g"] = c.green(); bg["b"] = c.blue();
    cfg["bg"] = bg; m_win->applyConfig(cfg);
}

void SettingsDialog::pickIcon() {
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("选择图标文件"),
                                                      QString(), QStringLiteral("图标文件 (*.ico)"));
    if (path.isEmpty()) return;
    m_icon->addItem(QStringLiteral("自定义"), path);
    m_icon->setCurrentIndex(m_icon->count() - 1);
}
```

- [ ] **Step 3: 向 `CMakeLists.txt` 的 `StockWidget` 目标添加源文件**

```cmake
qt_add_executable(StockWidget WIN32
    src/main.cpp
    src/ui/FloatWindow.cpp
    src/ui/KLineDelegate.cpp
    src/ui/SettingsDialog.cpp
)
```

- [ ] **Step 4: 构建**

Run: `cmd /c scripts\build.cmd`
Expected: 无错误。

- [ ] **Step 5: 提交**

```bash
git add src/ui/SettingsDialog.* CMakeLists.txt
git commit -m "feat: 4 页设置面板（单例、懒构造、字体缓存）"
```

---

## Task 10: GlobalHotkey 全局快捷键 + AutoStart 开机启动

**Files:**
- Create: `src/platform/GlobalHotkey.h`
- Create: `src/platform/GlobalHotkey.cpp`
- Create: `src/platform/AutoStart.h`
- Create: `src/platform/AutoStart.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: 写 `src/platform/AutoStart.h`**

```cpp
#pragma once
#include <QString>

namespace AutoStart {
bool isEnabled();
void setEnabled(bool enabled);
}
```

- [ ] **Step 2: 写 `src/platform/AutoStart.cpp`**

```cpp
#include "platform/AutoStart.h"
#include <QCoreApplication>
#include <QDir>
#include <windows.h>

namespace {
const wchar_t* kRunKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
const wchar_t* kValueName = L"StockWidget";
}

bool AutoStart::isEnabled() {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_READ, &key) != ERROR_SUCCESS) return false;
    const bool exists = RegQueryValueExW(key, kValueName, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS;
    RegCloseKey(key);
    return exists;
}

void AutoStart::setEnabled(bool enabled) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_SET_VALUE, &key) != ERROR_SUCCESS) return;
    if (enabled) {
        const QString exe = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
        const std::wstring cmd = QStringLiteral("\"%1\"").arg(exe).toStdWString();
        RegSetValueExW(key, kValueName, 0, REG_SZ,
                       reinterpret_cast<const BYTE*>(cmd.c_str()),
                       static_cast<DWORD>((cmd.size() + 1) * sizeof(wchar_t)));
    } else {
        RegDeleteValueW(key, kValueName);
    }
    RegCloseKey(key);
}
```

- [ ] **Step 3: 写 `src/platform/GlobalHotkey.h`**

```cpp
#pragma once
#include <QAbstractNativeEventFilter>
#include <QObject>
#include <QString>

class GlobalHotkey : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT
public:
    explicit GlobalHotkey(QObject* parent = nullptr);
    ~GlobalHotkey() override;
    bool registerShortcut(const QString& sequence);
    void unregisterShortcut();
    static QString normalize(const QString& sequence);

signals:
    void activated();

protected:
    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;

private:
    int m_id = 1;
    bool m_registered = false;
    void* m_window = nullptr;
};
```

- [ ] **Step 4: 写 `src/platform/GlobalHotkey.cpp`**

```cpp
#include "platform/GlobalHotkey.h"
#include <QCoreApplication>
#include <QKeySequence>
#include <QWidget>
#include <windows.h>

GlobalHotkey::GlobalHotkey(QObject* parent) : QObject(parent) {
    auto* host = new QWidget();
    host->setWindowFlag(Qt::Tool);
    host->setAttribute(Qt::WA_DontShowOnScreen);
    host->setFixedSize(1, 1);
    host->winId();  // 强制创建原生窗口句柄
    m_window = reinterpret_cast<void*>(host->winId());
    qApp->installNativeEventFilter(this);
}

GlobalHotkey::~GlobalHotkey() {
    unregisterShortcut();
    qApp->removeNativeEventFilter(this);
}

QString GlobalHotkey::normalize(const QString& sequence) {
    return QKeySequence(sequence, QKeySequence::PortableText).toString(QKeySequence::PortableText);
}

bool GlobalHotkey::registerShortcut(const QString& sequence) {
    unregisterShortcut();
    QKeySequence seq(sequence, QKeySequence::PortableText);
    if (seq.isEmpty()) return false;
    const int key = seq[0].toCombined();
    const int vk = key & 0x01FFFFFF;
    UINT mods = 0;
    if (key & Qt::CTRL) mods |= MOD_CONTROL;
    if (key & Qt::ALT) mods |= MOD_ALT;
    if (key & Qt::SHIFT) mods |= MOD_SHIFT;
    if (key & Qt::META) mods |= MOD_WIN;
    m_registered = RegisterHotKey(static_cast<HWND>(m_window), m_id, mods, static_cast<UINT>(vk));
    return m_registered;
}

void GlobalHotkey::unregisterShortcut() {
    if (m_registered) {
        UnregisterHotKey(static_cast<HWND>(m_window), m_id);
        m_registered = false;
    }
}

bool GlobalHotkey::nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) {
    Q_UNUSED(eventType);
    Q_UNUSED(result);
    auto* msg = static_cast<MSG*>(message);
    if (msg->message == WM_HOTKEY && msg->wParam == static_cast<WPARAM>(m_id)) {
        emit activated();
        return true;
    }
    return false;
}
```

- [ ] **Step 5: 向 `CMakeLists.txt` 的 `StockWidget` 目标添加源文件**

```cmake
qt_add_executable(StockWidget WIN32
    src/main.cpp
    src/ui/FloatWindow.cpp
    src/ui/KLineDelegate.cpp
    src/ui/SettingsDialog.cpp
    src/platform/GlobalHotkey.cpp
    src/platform/AutoStart.cpp
)
```

- [ ] **Step 6: 构建**

Run: `cmd /c scripts\build.cmd`
Expected: 无错误。

- [ ] **Step 7: 提交**

```bash
git add src/platform/GlobalHotkey.* src/platform/AutoStart.* CMakeLists.txt
git commit -m "feat: 全局快捷键与开机启动"
```

---

## Task 11: Application 托盘与编排（接线 + 退出保存）

**Files:**
- Create: `src/app/Application.h`
- Create: `src/app/Application.cpp`
- Modify: `src/main.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: 写 `src/app/Application.h`**

```cpp
#pragma once
#include <QApplication>
#include <QIcon>
#include <QJsonObject>

class FloatWindow;
class GlobalHotkey;
class QSystemTrayIcon;

class Application : public QApplication {
    Q_OBJECT
public:
    Application(int& argc, char** argv);
    void applyHotkey(const QString& sequence);
    void applyIcon(const QString& choice);
    void quitApp();

private:
    void saveConfig();
    QIcon resolveIcon(const QString& choice) const;
    void openSettings();
    void toggleWindow();

    QJsonObject m_config;
    FloatWindow* m_window = nullptr;
    QSystemTrayIcon* m_tray = nullptr;
    GlobalHotkey* m_hotkey = nullptr;
    QString m_iconChoice;
};
```

- [ ] **Step 2: 写 `src/app/Application.cpp`**

```cpp
#include "app/Application.h"
#include "app/ConfigStore.h"
#include "platform/AutoStart.h"
#include "platform/GlobalHotkey.h"
#include "ui/FloatWindow.h"
#include "ui/SettingsDialog.h"

#include <QAction>
#include <QMenu>
#include <QStyle>
#include <QSystemTrayIcon>

Application::Application(int& argc, char** argv) : QApplication(argc, argv) {
    setQuitOnLastWindowClosed(false);
    m_config = ConfigStore::normalize(ConfigStore::load());
    m_iconChoice = m_config.value(QStringLiteral("app_icon")).toString(QStringLiteral("default"));

    m_window = new FloatWindow(m_config);
    m_window->setOpenSettingsCallback([this] { openSettings(); });
    connect(m_window, &FloatWindow::configChanged, this, &Application::saveConfig);

    m_hotkey = new GlobalHotkey(this);
    connect(m_hotkey, &GlobalHotkey::activated, this, &Application::toggleWindow);
    applyHotkey(m_config.value(QStringLiteral("hotkey")).toString(QStringLiteral("Ctrl+Alt+F")));

    AutoStart::setEnabled(m_config.value(QStringLiteral("start_on_boot")).toBool(false));
    applyIcon(m_iconChoice);

    m_tray = new QSystemTrayIcon(windowIcon(), this);
    m_tray->setToolTip(QStringLiteral("StockWidget"));
    auto* menu = new QMenu();
    menu->addAction(QStringLiteral("显示/隐藏 浮窗"), this, &Application::toggleWindow);
    menu->addAction(QStringLiteral("设置…"), this, &Application::openSettings);
    menu->addSeparator();
    menu->addAction(QStringLiteral("退出"), this, &Application::quitApp);
    m_tray->setContextMenu(menu);
    connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason r) {
        if (r == QSystemTrayIcon::Trigger || r == QSystemTrayIcon::DoubleClick) toggleWindow();
    });
    m_tray->show();

    m_window->show();
    m_window->raise();
    m_window->activateWindow();
    saveConfig();
}

QIcon Application::resolveIcon(const QString& choice) const {
    if (choice.isEmpty() || choice == QStringLiteral("default"))
        return QIcon(QStringLiteral(":/StockWidget.ico"));
    if (choice.startsWith(QStringLiteral("std:"))) {
        const QString key = choice.mid(4);
        static const QHash<QString, QStyle::StandardPixmap> map{
            {"computer", QStyle::SP_ComputerIcon}, {"network", QStyle::SP_DriveNetIcon},
            {"folder", QStyle::SP_DirIcon},        {"file", QStyle::SP_FileIcon},
            {"trash", QStyle::SP_TrashIcon},       {"desktop", QStyle::SP_DesktopIcon}};
        return style()->standardIcon(map.value(key, QStyle::SP_ComputerIcon));
    }
    return QIcon(choice);
}

void Application::applyIcon(const QString& choice) {
    m_iconChoice = choice;
    const QIcon icon = resolveIcon(choice);
    setWindowIcon(icon);
    if (m_tray) m_tray->setIcon(icon);
}

void Application::applyHotkey(const QString& sequence) {
    m_hotkey->registerShortcut(GlobalHotkey::normalize(sequence));
}

void Application::toggleWindow() {
    if (m_window->isVisible()) {
        m_window->hide();
    } else {
        m_window->show();
        m_window->raise();
        m_window->activateWindow();
    }
    saveConfig();
}

void Application::openSettings() { SettingsDialog::showFor(m_window, m_window); }

void Application::saveConfig() {
    if (!m_window) return;
    QJsonObject cfg = ConfigStore::normalize(m_window->currentConfig());
    cfg[QStringLiteral("app_icon")] = m_iconChoice;
    m_config = cfg;
    ConfigStore::save(cfg);
}

void Application::quitApp() {
    saveConfig();
    if (m_window) m_window->stop();
    if (m_tray) m_tray->hide();
    quit();
}
```

- [ ] **Step 3: 改写 `src/main.cpp`**

```cpp
#include "app/Application.h"
#include <windows.h>

// shellapi.h only declares this when NTDDI_VERSION >= NTDDI_WIN7; declare it
// explicitly so the app builds regardless of SDK macro defaults.
extern "C" HRESULT WINAPI SetCurrentProcessExplicitAppUserModelID(PCWSTR);

int main(int argc, char** argv) {
    SetCurrentProcessExplicitAppUserModelID(L"StockWidget.1");
    Application app(argc, argv);
    return app.exec();
}
```

- [ ] **Step 4: 向 `CMakeLists.txt` 的 `StockWidget` 目标添加源文件**

```cmake
qt_add_executable(StockWidget WIN32
    src/main.cpp
    src/app/Application.cpp
    src/ui/FloatWindow.cpp
    src/ui/KLineDelegate.cpp
    src/ui/SettingsDialog.cpp
    src/platform/GlobalHotkey.cpp
    src/platform/AutoStart.cpp
)
```

- [ ] **Step 5: 构建并人工冒烟**

Run: `cmd /c scripts\build.cmd && build\StockWidget.exe`
Expected：
- 浮窗显示真实行情；托盘图标出现，左键切换显隐，右键有「设置…/退出」。
- `Ctrl+Alt+F` 全局切换显隐。
- 关闭再启动，位置与设置被记住（`%APPDATA%\StockWidget\SW_config.json`）。

- [ ] **Step 6: 提交**

```bash
git add src/app/Application.* src/main.cpp CMakeLists.txt
git commit -m "feat: 托盘、快捷键与配置编排"
```

---

## Task 12: 打包（windeployqt）+ 性能验收

**Files:**
- Create: `scripts/package.cmd`

- [ ] **Step 1: 写 `scripts/package.cmd`**

```bat
@echo off
setlocal
set "ROOT=%~dp0.."
set "QT=C:/1/Qt/6.11.1/msvc2022_64"
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cmake --build "%ROOT%\build" --config Release || exit /b 1
if exist "%ROOT%\dist" rmdir /s /q "%ROOT%\dist"
mkdir "%ROOT%\dist"
copy "%ROOT%\build\StockWidget.exe" "%ROOT%\dist\" >nul
"%QT%\bin\windeployqt.exe" --release --no-translations "%ROOT%\dist\StockWidget.exe" || exit /b 1
echo Packaged to %ROOT%\dist
```

- [ ] **Step 2: 打包**

Run: `cmd /c scripts\package.cmd`
Expected: 生成 `dist\StockWidget.exe` 与所需 Qt DLL（`Qt6Core.dll`、`Qt6Gui.dll`、`Qt6Widgets.dll`、`Qt6Network.dll`、`Qt6Core5Compat.dll`、`platforms\qwindows.dll` 等）。

- [ ] **Step 3: 独立运行验证**

把 `dist` 目录复制到一台无 Qt 环境的路径（或临时改名 `C:\1\Qt`）后运行 `dist\StockWidget.exe`。
Expected: 正常启动、显示行情。

- [ ] **Step 4: 性能验收（对照卡顿点）**

| 检查项 | 方法 | 通过标准 |
|---|---|---|
| B 刷新不卡 | 刷新间隔设 1s，持续 1 分钟，期间点击拖动 | 界面无冻结、无鼠标转圈 |
| A 拖动流畅 | 按住浮窗在屏幕上快速拖动 | 窗口跟手、无迟滞 |
| C 设置不卡 | 反复打开设置、快速切换 4 个标签 | 首次打开字体枚举后，之后切换无卡顿 |
| E 无泄漏 | 运行 30 分钟后任务管理器看内存/句柄 | 数值稳定，不明显增长 |

- [ ] **Step 5: 提交**

```bash
git add scripts/package.cmd
git commit -m "build: windeployqt 打包脚本"
```

---

## Self-Review 结果

**Spec 覆盖检查**

| 设计章节 | 覆盖任务 |
|---|---|
| 3.1 目录结构 | Task 1–11 全部文件 |
| 3.2 模块职责/接口 | Task 2（StockCode）3（Parser）4（ConfigStore/Columns）5（Model）6（Source）7（Delegate）8（FloatWindow）9（Settings）10（Hotkey/AutoStart）11（Application） |
| 3.3 数据流（异步） | Task 6 + Task 8 |
| 4 A/B/C 性能对策 | Task 5（diff/缓存）、Task 8（拖动/无 styleSheet/列宽缓存）、Task 9（单例/懒构造/字体缓存） |
| 5 功能对等清单 | Task 3（解析/格式化/盘口/K线数据）、Task 5（12 列/颜色）、Task 7（K线绘制）、Task 8（右键菜单/表头/网格/默认颜色/拖动/双击）、Task 9（自选/显示/外观/常规 4 页）、Task 10（热键/开机启动）、Task 11（托盘/图标/退出） |
| 5.1 不实现封单 | 全计划未引入封单逻辑（均价恒 `成交额/成交量`）✅ |
| 6 配置迁移 | Task 4（`normalize`）+ Task 11（启动时 normalize 后写回） |
| 7 错误处理 | Task 6（网络→「无网络连接」）、Task 4（损坏回退/原子写）、Task 8（错误标签保留旧数据）、Task 11（图标回退） |
| 8 测试策略 | Task 2/3/4/5 单测 + Task 12 人工冒烟 |
| 9 构建打包 | Task 1（CMake）+ Task 12（windeployqt） |

**占位符扫描**：无 TBD / TODO / “类似 Task N”；所有代码步骤均含完整代码。

**类型一致性**：`QuoteFormatOptions::B1S1Display`（Task 3）→ `QuoteColumns`/`QuoteModel`/`FloatWindow`/`SettingsDialog` 全链路一致；`QuoteModel::KLineRole`（Task 5）与 `KLineDelegate`（Task 7）一致；`ConfigStore::normalize`（Task 4）在 Task 11 复用时签名一致；`QuoteColumns::configKeyFor/allHeaders/isVisible/activeColumns` 在 Task 4/8/9 调用一致。

**已知偏差（有意，已在计划内注明）**
- `formatVolume` 对 `<1e4` 输出整数而非 Python 的 `8000.0`（属改进）。
- `QuoteParser::parseText` 入参为已解码 `QString`（GBK 解码在 `SinaQuoteSource`），与设计文档 Section 3.2 一致。

---

## 实现变更记录（首次交付后按实测修正）

1. **全局快捷键改为 `WH_KEYBOARD_LL` 低层键盘钩子**（原计划用 `RegisterHotKey`）。
   - 实测 `RegisterHotKey(hwnd, .., MOD_CONTROL|MOD_ALT, 'F')` 返回 0，`GetLastError()=1409`（`ERROR_HOTKEY_ALREADY_REGISTERED`）——`Ctrl+Alt+F` 已被其他程序占用，这是旧 Python 版（用全局钩子）能用、Qt 版不能用的根因。
   - 现实现：`SetWindowsHookExW(WH_KEYBOARD_LL, ...)`，在钩子里比对 vk + 修饰键，命中后用 `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` 发信号。已用 `SendInput` 端到端验证：`registered=1 fired=1`。
   - Task 10 中的 `RegisterHotkey`/`nativeEventFilter` 代码已被 `GlobalHotkey.cpp` 的钩子实现取代。
2. **右键菜单改为在 `eventFilter` 中处理 `QEvent::ContextMenu`**。
   - 原实现依赖事件冒泡，但 `QTableView`/viewport 会消费右键事件，导致浮窗收不到 `contextMenuEvent`。
   - 现实现：在已安装过滤器的子控件上拦截 `ContextMenu`，构造并转发 `QContextMenuEvent` 给窗口自身（保留虚分派）。
3. **单击（非拖动）隐藏**（原计划为双击隐藏）。
   - `eventFilter`/`mouseReleaseEvent` 记录按下点，位移小于 `QApplication::startDragDistance()` 视为单击 → `hide()`；超过阈值才移动窗口，拖动行为不变。双击仍然隐藏。
4. **新增「点击区域」设置**（`padding_px`，默认 12，0–40）。
   - 原实现面板内边距固定为 `10,6,10,6`，窗口较小时难以点到；现改为可调内边距，默认加大。
5. **补齐快捷键可视化设置**。
   - 首次交付的设置面板「常规」页漏了快捷键编辑器（原计划 Task 9 未包含），只能改 JSON。
   - 现新增 `QKeySequenceEdit`（限制单个组合键），写入 `hotkey` 后由 `Application::saveConfig` 检测变化并**即时重新注册**钩子。
   - 同时修正：程序图标/开机启动的修改也由 `saveConfig` 即时应用（原来仅在启动时应用一次）。
   - 已用 `SendInput` 验证自定义组合键：`Ctrl+Shift+F9` → `registered=1 fired=1`。

### 诊断方法（可复现）

- UI 级合成事件测试：`tests/test_ui.cpp`（单击/双击隐藏、右键到达窗口）。
- OS 级诊断：用 `SendInput` + `SetCursorPos`（注意 **物理坐标 = 逻辑坐标 × devicePixelRatio**，本机 dpr=2）发送真实鼠标/键盘输入，并用 `WindowFromPoint` 确认命中窗口。

