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

## 执行记录（与计划的偏差，已同步回计划文档）

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
