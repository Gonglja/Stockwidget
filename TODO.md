# 名称显示：全称 + 自定义映射 — 头脑风暴

- [x] **1. 探索项目上下文** — 已读 QuoteParser/QuoteColumns/ConfigStore/SettingsDialog（名称截断 name_length 已有解析但无 UI；自选列表只存代码字符串）
- [x] **2. 澄清问题** — 已确认：方案 C（名称不截断 + 补 `name_length` UI + 新增 `name_map`）；入口 D（设置双击对话框 + 浮窗行内右键）；排序 A（表头三态、持久化、实时重排）
- [x] **3. 提出 2-3 种方案** — 排序落点三选一，用户选 ① 上游纯函数排序
- [x] **4. 呈现设计** — 三节（数据与配置层 / UI 交互 / 排序键与测试）逐节确认通过
- [x] **5. 写设计文档** — docs/superpowers/specs/2026-09-17-name-map-and-sort-design.md（已提交 130373f）
- [x] **6. 设计自审** — 已内联修正：指示器清除方式、别名与排序交互、查表键规则（4a389df）
- [x] **7. 用户审阅设计文档** — 用户确认「没问题」
- [x] **8. 转入 writing-plans** — docs/superpowers/plans/2026-09-17-name-map-and-sort.md（已提交 4fe9593，8 个 TDD 任务）
- [x] **9. 实现执行** — executing-plans 完成 8/8 任务，全部 TDD；合并入 main（246d4e9）；`scripts\test.cmd` 9/9 测试目标全绿

- [x] **10. 追加需求（用户反馈「排序没做」）** — 右下角 ⚙ 配置按钮 + 「自定义配置」对话框（排序 + 自定义名称）；`gear_visible` 开关；提交 d67c385 / a0ec5a7；规格文档 §10
- [x] **11. 视觉验证** — 截图逐像素核对：齿轮 alpha 211（背景 172）、悬停 255 + 中心点；位置在右下内边距内不遮单元格（截图 2x DPI，逻辑坐标需 ÷DPR）

## 追加需求记录

用户反馈「排序没做」的原因：排序已实现但**唯一入口是点表头**，而默认 `header_visible=false` 时没有任何可见入口 → 追加右下角配置入口（见规格 §10）。

## 执行记录（与计划的偏差，已同步回计划/规格文档）

| 处 | 偏差 | 原因 |
|---|---|---|
| QuoteSort 测试 | 修正 2 处期望笔误 | `amount` 降序 `at(2)` 索引写错；`QString::compare` 是 UTF-16 码位序（乙 U+4E59 < 甲 U+7532），非拼音 |
| NameAlias 实现 | 改为每次调用只归一化一遍映射表 | 原计划逐行 `normalize()` 是 O(rows×map) 正则开销，1s 刷新间隔下会拖慢 |
| 表头点击 | 不用 `QHeaderView::sectionClicked`，改由 `FloatWindow::eventFilter` 接管表头左键 | 实测 Qt 会把未被接受的表头事件冒泡给 `QTableView`（`press:QHeaderView` → `press:QTableView`），点击被当成拖动并隐藏窗口 |
| 表头点击测试 | 需 `header_visible=true`，且每次点击前按当前列宽重算位置 | 表头不可点时点击落到视图；排序指示器/数据变化会让列宽微调 |
| 设置列表双击测试 | 需 `QTest::mouseClick` 后再 `mouseDClick`，定时器绑 context object | `QAbstractItemView` 仅在 `pressedIndex` 匹配时发 `doubleClicked`；否则定时器会在对象销毁后触发（实测崩溃） |

---

# StockWidget Qt/C++ 重构 — 头脑风暴（已完成）

目标：用 Qt C++ 重写当前 PySide6 版本，解决卡顿问题。

- [x] **1. 探索项目上下文** — 已读完全部源文件 + 环境探测
- [x] **2. 提出澄清问题** — 范围=全功能对等；发行=便携文件夹；卡顿点=拖动/刷新/设置面板
- [ ] **3. 提出 2-3 种方案** — 分析架构取舍
- [ ] **4. 呈现设计** — 给出推荐方案和迁移路径
- [x] **5. 编写设计文档** — docs/superpowers/specs/2026-09-11-stockwidget-qt-refactor-design.md（已提交 21e9f78）
- [x] **6. 设计自审** — 已内联修正：补充 StockCode 独立模块
- [x] **7. 用户审阅设计文档** — 用户确认通过
- [x] **8. 转入写实现计划（writing-plans）** — docs/superpowers/plans/2026-09-11-stockwidget-qt-refactor.md（已提交 83c8a91）

## 项目上下文快照

**现有代码结构（PySide6，1860 行）**
- `StockWidget.py` — 入口
- `App.py` (206) — QApplication + 系统托盘 + 配置读写 + 开机启动 + 图标
- `WidgetPanel.py` (870) — 浮窗本体：无框透明置顶、网络抓取、列投影、拖拽/双击/右键
- `Display.py` (180) — SimpleTableModel + KLineDelegate（当日K线）
- `SettingPanel.py` (591) — 4 页设置对话框

**卡顿根因（初步判断）**
1. `_refresh_from_function` 在 GUI 线程同步调用 `requests.get`（timeout=3）→ 每次刷新界面冻结
2. 每次刷新 `_project_columns` 走 `beginResetModel/endResetModel` 全量重置
3. 每次刷新 `_fit_to_contents` → `resizeColumnsToContents()` 全表重算
4. `apply_style` 反复 setStyleSheet 解析
5. `_ensure_on_top` 每秒 raise_()

**环境探测结果**
- VS2022 Community，MSVC 14.44 + Windows SDK 10.0.26100 ✅
- CMake ✅ / Ninja ✅ / Python 3.12 + pip ✅
- ❌ Qt 未安装（C:/Qt 不存在）
- ❌ vcpkg 未安装

---

## 2026-09-20 追加需求：自选列表两列 + 非交易时段提示 + 托盘图标即时生效

- [x] **1. 探索项目上下文** — 读 FloatWindow / SettingsDialog / Application / ConfigStore + 用户实际配置（`fetch_mode=market`、`code_visible=false`、`name_length=4`）
- [x] **2. 澄清问题** — ①改的是**设置面板**的自选列表并固定两列；名称列 = 别名优先、回落浮窗当前行情名；②提示文案「非交易时段」；③「卡顿」= 托盘图标延迟生效；**浮窗继续受「显示指标」开关控制、不改默认值**
- [x] **3-4. 方案与设计** — 三处改动（QTreeWidget 两列 / infoLabel 提示 / applyConfig 补 notifyChanged），逐节确认通过
- [x] **5-7. 设计文档** — `docs/superpowers/specs/2026-09-20-watchlist-columns-info-label-tray-icon-design.md`；用户确认「可以，实现它」
- [x] **8. 实现计划** — `docs/superpowers/plans/2026-09-20-watchlist-columns-info-label-tray-icon.md`（6 个 TDD 任务）
- [x] **9. 实现执行** — executing-plans 内联完成 6/6 任务（本环境 `dispatch_agent` 不可用）；全量 `ctest` **9/9 通过**（`test_ui` 30 用例）；视觉验证 4 张截图（ASCII 判读）
- [x] **10. 收尾** — README 更新；合并回 main

**提交序列（分支 `feat/watchlist-cols-info-tray`）**

| 提交 | 内容 |
|---|---|
| `47a48b4` | FloatWindow 暴露 `quoteNameFor()` 与 `quotesUpdated()` 信号 |
| `a74f25b` | 非请求时段且无数据时显示「非交易时段」提示 |
| `44b415f` | 图标/快捷键/开机启动改动立即落地（补 `notifyChanged`） |
| `b4bda51` | 设置面板自选列表改为「代码 / 名称」两列 |
| `f3c7eda` | 设置面板名称列随行情到达自动回填 |

**执行记录（与计划的偏差）**

| 处 | 偏差 | 原因 |
|---|---|---|
| 所有任务的「运行单个用例」验证步骤 | `build\test_ui.exe -v1 <case>` 无任何输出 → 改用 `build\test_ui.exe -o report.txt,txt` 与 `scripts\test.cmd -R test_ui` | Qt 测试 exe 的 stdout 不被 cmd/bash 管道捕获（本机 GUI 子系统行为），退出码仍可信 |
| Task 4 双击坐标 | `tree->visualItemRect(item, 0)` → `tree->visualItemRect(item)` | `QTreeWidget` 只有单参重载（两参的是 `QTreeView::visualRect(QModelIndex)`） |
| 执行方式 | 计划推荐 subagent-driven-development，实际用 executing-plans 内联 | 本环境 `dispatch_agent` 连续两次立即失败（exit 1），子代理不可用 |

---

## 2026-09-20 追加需求 2：设置面板名称自主取数

- [x] **1. 探索/澄清** — 根因：名称列只读浮窗（受请求时段限制）；联想接口返回的名称被丢弃；手输代码没有任何取名路径
- [x] **2-4. 设计与确认** — 设置面板自带行情源（不看请求时段）+ 三触发点 + 别名>本页>浮窗优先级 + 请求队列；用户确认「可以的」
- [x] **5-7. spec/计划** — `docs/superpowers/specs/2026-09-20-settings-name-fetch-design.md`、`docs/superpowers/plans/2026-09-20-settings-name-fetch.md`
- [x] **8-9. 实现（分支 `feat/settings-name-fetch`）** — 3 个提交；`test_ui` 35 用例；全量 `ctest` 9/9
- [x] **10. 真网端到端验证** — 盘外探针（浮窗不请求）：设置面板拉到 `中信证券/中国平安/平安银行`，浮窗名称为空 ✅

**提交序列**

| 提交 | 内容 |
|---|---|
| `4b4e28e` | SinaQuoteSource 支持注入 NAM（测试用） |
| `d9e2f87` | 设置面板自主拉取名称（缓存 + 请求队列 + 三触发点） |
| `52ae7d0` | 搜索联想自带的名称直接复用（零请求） |

**执行记录（与计划的偏差）**

| 处 | 偏差 | 原因 |
|---|---|---|
| `ensureNamesFor` 过滤条件 | 计划只写了「非法/已有名」，实现补了 `aliasForCode()` 别名检查 | 测试 `settingsNamesSkipAliasedRows` 抓出：有自定义名称的代码仍会被请求 |
| 测试替身 | 计划里 `StubNam` 返回固定 body → 改为**按 URL 的 `list=` 参数**生成响应 | 固定 body 会把没请求的代码也塞进缓存，导致 `settingsNameRequestedOnManualAdd` 假失败（真实新浪只返回请求的代码） |
| 测试辅助函数 | 用脚本批量替换时误删了 `columnOf`/`cellText`，已恢复 | 替换区间从注释行切到 `baseConfig()`，跨过了这两个函数 |

---

## 2026-09-20 追加需求 3：版本号跟随 tag

- [x] **背景** — `project(StockWidget VERSION 1.0.0)` 从没跟 tag 涨过；且 exe 没有 VERSIONINFO，文件属性里看不到任何版本
- [x] **方案** — 环境变量 `SW_VERSION_OVERRIDE`（CI 传 tag）> git tag > `0.0.0`；注入 `SW_VERSION` 宏；生成 `StockWidget.rc` 写 VERSIONINFO；托盘提示与设置窗口标题显示 `vX.Y.Z`
- [x] **验证** — ①无注入 → `1.6.1`（取 git tag）；②`SW_VERSION_OVERRIDE=v9.9.9` → `9.9.9`（去掉 `v`）；③再去掉注入 → 回到 `1.6.1`（**无 CMake 缓存污染**，故用环境变量而非 `-D`）；④exe `VersionInfo.FileVersion=1.6.1`，中文描述以 UTF-16 正确写入（码位校验通过）；⑤全量 `ctest` 9/9（含新增 `versionMacroFollowsTag`）

**提交**：（见 git log）

**执行记录（偏差）**

| 处 | 偏差 | 原因 |
|---|---|---|
| 版本注入方式 | 计划用 `-DSW_VERSION_OVERRIDE`，实现改为**环境变量** | `-D` 会写进 `CMakeCache.txt`，后续不传也继续生效（本地验证时踩到） |
| `StockWidget.rc` | 需要 `#include <winres.h>` + `#pragma code_page(65001)` | 否则 `VS_FFI_FILEFLAGSMASK` 未定义 / 中文值报 RC2133 |
| CMake | 额外 `string(STRIP)` | cmd 的 `set VAR=x && ...` 会把尾随空格带进值，导致正则校验失败回落 0.0.0 |
