# StockWidget Qt/C++ 重构设计

- 日期：2026-09-11
- 状态：待用户审阅
- 目标：用 Qt 6 C++ 重写现有 PySide6 版本，解决卡顿，功能完整对等

---

## 1. 背景与问题

现有项目为 PySide6 实现（约 1860 行）：`App.py`(206) / `WidgetPanel.py`(870) / `Display.py`(180) / `SettingPanel.py`(591) / `StockWidget.py`(13)。用户反馈三类卡顿：

- **A 拖动浮窗卡顿**
- **B 每隔几秒刷新瞬间无响应**
- **C 打开设置面板 / 切换标签页卡**

### 1.1 根因分析

1. `WidgetPanel._refresh_from_function` 在 GUI 线程同步调用 `requests.get(timeout=3)`，每次刷新阻塞事件循环。
2. 每次刷新 `_project_columns` 走 `beginResetModel/endResetModel` 全量重置。
3. 每次刷新 `_fit_to_contents` 调用 `resizeColumnsToContents()` 全表重算。
4. `apply_style()` 反复 `setStyleSheet` 解析样式字符串。
5. `mouseMoveEvent` 每次移动都 `raise_()`；另有每秒 `_keep_top_timer` 触发 `_ensure_on_top()`。
6. `SettingsDialog` 每次打开都 `new`，构造时枚举全系统字体（`QFontDatabase.families()`），切标签 `setFixedSize` 反复重排。

> 结论：卡顿本质是**架构问题**（同步 IO + 全量刷新 + 重排），换语言只是次要因素；重构必须同时改掉这些。

---

## 2. 范围与约束

- **功能范围**：完整功能对等重写（用户选定），不做裁剪。
- **平台**：仅 Windows 10/11 x64。
- **发行形态**：便携文件夹（`exe + Qt DLL`），本机预装 Qt 6.11.1 + `windeployqt`（用户选定）。
- **技术栈**：Qt 6.11.1（msvc2022_64，`C:/1/Qt/6.11.1/msvc2022_64`），MSVC 2022 x64，CMake + Ninja。
- **依赖**：仅 Qt 模块 `Core Gui Widgets Network Core5Compat`（测试加 `Test`）。热键用 Win32 低层键盘钩子 `WH_KEYBOARD_LL`，开机启动用注册表，**不引入第三方库**。
  - 为什么不用 `RegisterHotKey`：它受「快捷键已被占用」限制（实测 `Ctrl+Alt+F` 报 `ERROR_HOTKEY_ALREADY_REGISTERED`），而旧 Python 版的 `keyboard` 库用的是全局钩子、无此冲突。
- **配置兼容**：沿用 `%APPDATA%\StockWidget\SW_config.json`，兼容旧键。
- **不做（YAGNI）**：多屏 DPI 特化、跨平台、QThread 并行。

---

## 3. 架构设计

### 3.1 目录结构

```
StockWidget/
├─ CMakeLists.txt
├─ resources/            StockWidget.ico + app.qrc
├─ src/
│  ├─ main.cpp
│  ├─ app/
│  │   ├─ Application.{h,cpp}     QApplication 子类：托盘 / 编排 / 退出 / 图标
│  │   └─ ConfigStore.{h,cpp}     SW_config.json 读写（原子写入）
│  ├─ ui/
│  │   ├─ FloatWindow.{h,cpp}     无框透明置顶浮窗：布局/拖动/右键菜单
│  │   ├─ QuoteModel.{h,cpp}      QAbstractTableModel，增量 diff
│  │   ├─ KLineDelegate.{h,cpp}   QStyledItemDelegate 画当日K线
│  │   └─ SettingsDialog.{h,cpp}  4 页设置，单例常驻
│  ├─ data/
│  │   ├─ Quote.h                 单只股票解析后的结构体
│  │   ├─ QuoteColumns.h          列定义表（标题/键/对齐/格式化）
│  │   ├─ StockCode.{h,cpp}       代码规格化与去重（纯函数）
│  │   ├─ QuoteParser.{h,cpp}     纯函数：原始文本 → QVector<Quote>
│  │   └─ SinaQuoteSource.{h,cpp} QNAM 异步拉取 + GBK 解码 + 超时
│  └─ platform/
│      ├─ GlobalHotkey.{h,cpp}    Win32 RegisterHotKey + 原生事件过滤
│      └─ AutoStart.{h,cpp}       HKCU Run 键读写
└─ tests/
   ├─ test_parser.cpp             QTest：解析
   ├─ test_codes.cpp              QTest：代码规格化
   └─ test_config.cpp             QTest：配置往返/迁移
```

### 3.2 模块职责与接口

| 模块 | 职责 | 接口 |
|---|---|---|
| `QuoteParser` | 纯解析，无 IO，可单测 | `static QVector<Quote> parseText(const QString&)`（GBK 解码在 SinaQuoteSource） |
| `StockCode` | 代码规格化/去重，纯函数，可单测 | `static std::optional<QString> normalize(QString)` |
| `SinaQuoteSource` | 异步请求 + 超时 + GBK 解码 | `void fetch(QStringList codes)`；信号 `quotesReady(QVector<Quote>)` / `error(QString)` |
| `QuoteModel` | 持有数据与列配置，增量 diff | `setQuotes()` / `setColumns()` → `dataChanged` / `layoutChanged` |
| `KLineDelegate` | 绘制当日 K 线 | `paint()` |
| `FloatWindow` | 浮窗交互与渲染，组合以上组件 | 设置 setter + `currentConfig()` |
| `SettingsDialog` | 4 页设置，单例 | 信号回写 `FloatWindow` |
| `ConfigStore` | 配置加载/保存 | `load()` / `save(cfg)`，`QSaveFile` 原子写 |
| `GlobalHotkey` | 全局快捷键（WH_KEYBOARD_LL 低层键盘钩子） | 注册/注销，触发信号 |
| `AutoStart` | 开机启动 | 读写 HKCU Run 键 |
| `Application` | 托盘、编排、退出、图标管理 | — |

### 3.3 数据流（无阻塞）

```
QTimer(刷新间隔) ──► FloatWindow
                       └─► SinaQuoteSource.fetch(已勾选代码)      ← 立即返回
                               │ QNetworkAccessManager（异步）
                               ▼
                         quotesReady(QVector<Quote>)
                               └─► QuoteModel.setQuotes(...)         ← 逐格 diff
                                       └─► dataChanged(仅变化格) ──► 视图局部重绘
```

- **并发策略**：全程单 GUI 线程 + QNAM 异步，**不引入 QThread**（股票数少，解析开销可忽略）。
- 网络超时用 `QNetworkRequest::setTransferTimeout(3000)`；GBK 解码用 Core5Compat 的 `QTextCodec("GB18030")`；请求头带 `Referer: https://finance.sina.com.cn`。

---

## 4. 性能设计（A/B/C 逐条对策）

| 卡顿点 | 旧做法 | 新设计 |
|---|---|---|
| **B 刷新卡** | GUI 线程同步 IO；全量 `beginResetModel`；每 tick `resizeColumnsToContents()` | ① QNAM 异步 + 超时，回调不阻塞；② `setQuotes()` 逐格 diff，只 `emit dataChanged(变化格)`，仅列集合变化才 `layoutChanged`；③ 列宽缓存，仅字体/列集合变化或文本超宽时才 `resizeColumnToContents`；④ 行高按字体算一次 `setDefaultSectionSize`；⑤ 刷新路径零 `setStyleSheet` |
| **A 拖动卡** | 移动中 `raise_()` + 每秒置顶定时器 + 多层事件过滤器 | ① 拖动期 `m_dragging`，不 `raise_()`、不重排，只 `move()`；② 删除每秒置顶定时器，靠 `WindowStaysOnTopHint\|Tool`，仅 show 与拖动结束 raise 一次；③ 拖动中不 `adjustSize/resize`；④ 圆角背景改 `paintEvent` 绘制（`WA_TranslucentBackground`），不靠样式表逐帧解析 |
| **C 设置卡** | 每次 `new`；构造枚举全系统字体；切标签 `setFixedSize` | ① 对话框单例常驻，show/hide 复用；② 字体列表函数内 `static` 缓存；③ 标签页首次访问懒构造；④ 取消 per-tab `setFixedSize`（避免重排抖动）；⑤ 同步控件状态 `blockSignals` 防回环 |
| **E 长跑变卡（预防）** | reply/临时对象泄漏风险 | 每个 `QNetworkReply` `deleteLater()`；paint 路径零临时分配；模型复用行缓冲 |

---

## 5. 功能对等清单

- **浮窗**：无框/透明/置顶/`Qt::Tool`；拖拽任意区域移动；**单击（非拖动）隐藏，双击亦可**；右键菜单（显示指标子菜单、显示表头、显示网格、默认颜色、设置…、隐藏浮窗）。
- **托盘**：左键切换显隐；右键 显隐/设置/退出。
- **表格 12 列**：代码·名称·现价·涨跌值·涨跌幅·买一·卖一·委比·成交量·成交额·均价·K线；除 名称/K线/卖一 外右对齐；现价触当日高/低加 `↑/↓`。
- **盘口**：`qty / price / both` 三种模式；集合竞价「配对量 / 未配对量」；连续竞价 `<>` 位置箭头（买一箭头 `<` 在右、卖一箭头 `>` 在左）；连续竞价买一红、卖一绿。
- **格式化**：成交量/成交额 万 / 亿 / 万亿；ETF（`code[2] in {1,5}`）三位小数，其余两位；委比 `±x.xx%`。
- **K线**：昨收虚线、涨红跌绿/单色、实体+上下影线、一字线、跌时空心实体填充、随字号缩放（scale 0.5–1.5）。
- **显示设置**：12 列独立开关、表头开关、网格开关、默认颜色/单色、文字色、背景色 + 背景不透明度 0–100、整体不透明度 20–100。
  - 注：整体不透明度**烘入颜色 alpha**（不用 `setWindowOpacity`）；绘制背景时 alpha 钳制到 **≥1**，否则 Windows 分层窗口会该区域鼠标穿透，导致文字以外的区域点不到。
- **字体行距**：字体、字号 8–15pt、行距 0–20px（行高 = 字高 + 行距）。
- **显示/请求时段（两套独立开关）**：
  - 显示时段：`always`（一直显示）/ `market`（周一~五 9:15–15:00）/ `custom`（自定义起止 HH:MM）。
  - 请求时段：`always` / `market` / `custom`。
  - 显示：20s 调度定时器，仅在**跨越时段边界**时切换（进入→`show()`，离开→`hide()`），不打断手动隐藏。
  - 请求：非请求时段 `refreshNow()` 直接跳过，保留上次数据。
- **贴边隐藏**（`edge_hide`，默认关）：
  - **方向** `edge_side`：`auto`（默认，就近）/ `left` / `right` / `top` / `bottom`；非 auto 时强制收到指定边。
  - 150ms 轮询鼠标位置；离开 400ms 后就吸到最近屏幕边（左/右/上/下）并滑出，仅留 4px 可见细条。
  - 鼠标回到细条上（光标重新处于窗口矩形内）即恢复至贴边位置；菜单弹出时不收起。
  - 开启时禁用单击隐藏，避免冲突；从托盘/快捷键重新显示时不会停在收起位置。
  - 持久化的是**贴边位置**而非收起位置。
  - **宽限期**：每次 `show()` 后 3s 内不自动收起，避免“刚显示就消失”。
- **定位浮窗**（`FloatWindow::locate()`，托盘右键菜单）：移到屏幕居中 + `restoreFromEdge()` + 强制显示 10s（绕过显示时段），用于找回丢失/被收起的窗口。
- **自选管理**：增/删/改/上移/下移 + 勾选显示（`checked_codes`）；代码规格化去重规则：
  - `sh|sz|bj` + 数字 → 直接接受
  - `6`/`90`/`5` 开头 → `sh`；`0`/`1`/`2`/`3` 开头 → `sz`；`4`/`8`/`92` 开头 → `bj`
  - 非法输入回退到上次有效值
- **其他**：仅显示数字（`short_code`）、名称截断 0–4 字（`name_length`）；刷新间隔 1/2/3/5/10/15/30/60s；全局快捷键（默认 `Ctrl+Alt+F`，**设置面板内可视化录制，改动即时重新注册**）；开机启动（改动即时写入注册表）；程序图标（默认/系统 6 种/自定义文件，改动即时应用）；位置记忆；隐藏暂停刷新、显示恢复；错误提示（网络异常显示「无网络连接」，保留上次有效数据）。

### 5.1 已知文档-代码不符（决策）

- README 称「涨跌停板时均价自动切换为封单数量」，但现有代码**无封单逻辑**，均价恒为 `成交额/成交量`。
- **本次决策：按代码现状移植，不实现封单切换**（保持行为对等）。如需补上，另行提出。

---

## 6. 配置迁移

- **同路径**：`%APPDATA%\StockWidget\SW_config.json`，老用户无缝沿用。
- **读取兼容旧键**：
  - `flags`（list 或 dict）→ 各列独立 bool 属性
  - `b1s1_price`（bool）→ `b1s1_display`（`price` / `qty`）
  - `visible_codes` → `checked_codes`
- **写回新规范 schema**，原子写（`QSaveFile`），一次性完成迁移。

规范 schema 键（与现有 `current_config()` 对齐）：`codes, checked_codes, code_visible, name_visible, price_visible, change_visible, change_pct_visible, b1s1_visible, commi_visible, vol_visible, amount_visible, avg_visible, kline_visible, short_code, name_length, b1s1_price, b1s1_display, header_visible, grid_visible, refresh_seconds, fg, bg{r,g,b,a}, opacity_pct, font_family, font_size, line_extra_px, default_color, pos{x,y}, hotkey, start_on_boot, app_icon, show_mode, show_start, show_end, fetch_mode, fetch_start, fetch_end, edge_hide, edge_side`。

---

## 7. 错误处理

- **网络异常** → 映射「无网络连接」，顶部错误标签；**保留上一次有效行情**，不闪空表。
- **解析异常** → 跳过该行，不影响其他股票。
- **配置损坏** → 回退默认值；`QSaveFile` 原子写防半写。
- **图标缺失** → 回退系统标准图标。

---

## 8. 测试策略

- `test_parser`（QTest）：普通股 / ETF 三位小数 / 集合竞价（买卖一相等）/ 一字板 / 空行脏数据。
- `test_codes`：`600000→sh600000`、`000001→sz000001`、`8xxxxx→bjxxxxxx`、去重、非法回退。
- `test_config`：往返一致 + 旧键迁移（`flags`、`b1s1_price`、`visible_codes`）。
- **人工冒烟**：拖动、刷新、切标签、右键菜单、托盘、双击隐藏、K线显示。

---

## 9. 构建与打包

- **Qt 6.11.1 (msvc2022_64)**，本机路径 `C:/1/Qt/6.11.1/msvc2022_64`（无需下载）。
- **CMake + Ninja**：`qt_standard_project_setup()` + `qt_add_executable(WIN32 ...)`。
- **链接模块**：`Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::Core5Compat`（测试目标加 `Qt6::Test`）。
- **资源**：`qt_add_resources` 内嵌 `app.qrc`（图标），`WIN32` 子系统无控制台窗口。
- **发布**：Release（`/O2`）+ `windeployqt --release` 产出便携文件夹。
- 仅 Windows；高 DPI 交给 Qt 6 自动处理。

---

## 10. 迁移路径

1. 搭建 CMake 工程骨架 + 安装 Qt 6，跑通空白透明浮窗。
2. 移植 `QuoteParser` + 测试（纯逻辑先行）。
3. 移植 `SinaQuoteSource` 异步拉取。
4. 移植 `QuoteModel` + `KLineDelegate` + `FloatWindow`（含拖动/右键）。
5. 移植 `ConfigStore` + 迁移逻辑 + 测试。
6. 移植 `SettingsDialog`（4 页，懒加载）。
7. 移植托盘、全局热键、开机启动、图标选择。
8. 打包 `windeployqt`，实测性能达标。
