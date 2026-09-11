# StockWidget Qt/C++ 重构 — 头脑风暴

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
