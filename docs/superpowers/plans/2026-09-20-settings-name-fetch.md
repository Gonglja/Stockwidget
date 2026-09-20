# 设置面板名称自主取数 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 设置 →「自选列表」的名称列不再受浮窗「请求时段」限制：打开设置时批量拉一次、添加代码时拉一次、切回该页补一次；联想结果自带的名称直接复用（零请求）。

**Architecture:** `SettingsDialog` 自持一个 `SinaQuoteSource`（不拥有 NAM，可注入替身），用一个 `QHash<code,name>` 会话缓存 + `QStringList` 待请求队列驱动「别名 > 本页行情名 > 浮窗行情名」的显示优先级；首次请求放在 `showEvent()`（而不是构造函数），保证测试能在发请求前替换 NAM。`FloatWindow`、请求时段、`name_map` 写回全部不动。

**Tech Stack:** Qt 6.8+（Core/Gui/Widgets/Network/Core5Compat/Test），QTest（`tests/test_ui.cpp`），测试注入 `QNetworkAccessManager` 替身（不联网）。

**规格来源：** `docs/superpowers/specs/2026-09-20-settings-name-fetch-design.md`

**统一命令**

```
scripts\build.cmd                         # 配置 + 编译（含全部测试目标）
scripts\test.cmd                          # 全部 ctest
scripts\test.cmd -R test_ui               # 只跑 test_ui
```

单跑 `test_ui` 并看明细（**本机 exe 的 stdout 不被 shell 捕获，必须用 `-o` 落盘**）：

```
scripts\build.cmd
set "PATH=C:/1/Qt/6.11.1/msvc2022_64/bin;%PATH%"
build\test_ui.exe -o report.txt,txt
grep -E "^FAIL|^Totals" report.txt
```

---

### Task 1: SinaQuoteSource 支持注入 QNetworkAccessManager

**Files:**
- Modify: `src/data/SinaQuoteSource.h`
- Modify: `src/data/SinaQuoteSource.cpp`

- [ ] **Step 1: 改头文件**

`src/data/SinaQuoteSource.h` 里 `public:` 区加一行方法，并把 `m_nam` 值成员换成指针 + 内置实例：

```cpp
    explicit SinaQuoteSource(QObject* parent = nullptr);
    // 注入外部 NAM（不拥有，nullptr = 恢复内置）。测试用来避免真实联网。
    void setNetworkAccessManager(QNetworkAccessManager* nam);
    void setFormatOptions(const QuoteFormatOptions& opt);
    void fetch(const QStringList& codes);
```

```cpp
private:
    QNetworkAccessManager* m_nam = nullptr;        // 外部注入或内置
    QNetworkAccessManager* m_ownedNam = nullptr;   // 内置实例（父对象 = this）
    QuoteFormatOptions m_opt;
    QNetworkReply* m_reply = nullptr;
```

- [ ] **Step 2: 改实现**

`src/data/SinaQuoteSource.cpp`：

```cpp
SinaQuoteSource::SinaQuoteSource(QObject* parent) : QObject(parent) {
    m_ownedNam = new QNetworkAccessManager(this);
    m_nam = m_ownedNam;
}

void SinaQuoteSource::setNetworkAccessManager(QNetworkAccessManager* nam) {
    m_nam = nam ? nam : m_ownedNam;
}
```

并把 `fetch()` 里的 `m_nam.get(req)` 改为 `m_nam->get(req)`（并在函数首行加 `if (!m_nam) return;`）。

- [ ] **Step 3: 构建 + 全量测试（重构的安全网）**

Run: `scripts\build.cmd` 然后 `scripts\test.cmd`
Expected: `100% tests passed, 0 tests failed out of 9`（行为零变化）

- [ ] **Step 4: 提交**

```
git add src/data/SinaQuoteSource.h src/data/SinaQuoteSource.cpp
git commit -m "refactor(data): SinaQuoteSource 支持注入 QNetworkAccessManager（便于测试）"
```

---

### Task 2: 测试替身 + 设置面板自主拉名称

**Files:**
- Modify: `tests/test_ui.cpp`（替身 + 新用例 + 既有用例注入）
- Modify: `src/ui/SettingsDialog.h`（缓存/队列/方法/成员）
- Modify: `src/ui/SettingsDialog.cpp`（`nameFor` / `refreshItemName` / `ensureNamesFor` / `pumpNameQueue` / `applyNames` / `showEvent` / 触发点）

- [ ] **Step 1: 写测试替身（`tests/test_ui.cpp`）**

include 区追加：

```cpp
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTextCodec>
#include "data/SinaQuoteSource.h"
```

在 `static QVector<Quote> twoQuotes()` 之后追加：

```cpp
// ---- 测试替身：不联网的行情源（体内容用 GBK 编码，模拟新浪）----
class StubReply : public QNetworkReply {
public:
    StubReply(const QNetworkRequest& req, const QByteArray& body, bool autoFinish,
              QObject* parent)
        : QNetworkReply(parent), m_body(body) {
        setRequest(req);
        setUrl(req.url());
        open(QIODevice::ReadOnly);
        setFinished(true);
        if (autoFinish)
            QMetaObject::invokeMethod(this, [this] { emit readyRead(); emit finished(); },
                                      Qt::QueuedConnection);
    }
    void abort() override {}
    qint64 bytesAvailable() const override {
        return (m_body.size() - m_off) + QNetworkReply::bytesAvailable();
    }
    void finishNow() { emit readyRead(); emit finished(); }

protected:
    qint64 readData(char* data, qint64 maxSize) override {
        const qint64 n = qMin(maxSize, qint64(m_body.size()) - m_off);
        if (n <= 0) return -1;
        memcpy(data, m_body.constData() + m_off, size_t(n));
        m_off += n;
        return n;
    }

private:
    QByteArray m_body;
    qint64 m_off = 0;
};

class StubNam : public QNetworkAccessManager {
public:
    QByteArray body;      // 返回内容（GBK 字节）
    bool manual = false;  // true：不自动 finished，等 flush()
    QStringList urls;     // 收到的请求 URL

    void flush() {
        const QVector<StubReply*> pending = m_pending;  // 先取副本：回调里可能再发请求
        m_pending.clear();
        for (StubReply* r : pending) r->finishNow();
    }

protected:
    QNetworkReply* createRequest(Operation, const QNetworkRequest& req, QIODevice*) override {
        urls << req.url().toString();
        auto* r = new StubReply(req, body, !manual, this);
        if (manual) m_pending << r;
        return r;
    }

private:
    QVector<StubReply*> m_pending;
};

static QByteArray gbk(const QString& s) {
    QTextCodec* codec = QTextCodec::codecForName("GB18030");
    return codec ? codec->fromUnicode(s) : s.toUtf8();
}

// sh600030 中信证券 / sh600519 贵州茅台 / sz000001 平安银行（真实新浪格式，见 kTwoLines）
static QByteArray nameBody() {
    return gbk(QString::fromUtf8(
        "var hq_str_sh600030=\"中信证券,10.100,10.000,10.200,10.300,9.900,"
        "10.190,10.200,8000,81600.000,"
        "100,10.180,200,10.170,300,10.160,400,10.150,500,10.140,"
        "600,10.210,700,10.220,800,10.230,900,10.240,1000,10.250,"
        "2026-09-11,15:00:00,00\";\n"
        "var hq_str_sh600519=\"贵州茅台,10.100,10.000,10.200,10.300,9.900,"
        "10.190,10.200,8000,81600.000,"
        "100,10.180,200,10.170,300,10.160,400,10.150,500,10.140,"
        "600,10.210,700,10.220,800,10.230,900,10.240,1000,10.250,"
        "2026-09-11,15:00:00,00\";\n"
        "var hq_str_sz000001=\"平安银行,10.000,10.000,9.500,10.100,9.400,"
        "9.490,9.500,5000,47500.000,"
        "100,9.480,200,9.470,300,9.460,400,9.450,500,9.440,"
        "600,9.510,700,9.520,800,9.530,900,9.540,1000,9.550,"
        "2026-09-11,15:00:00,00\";\n"));
}
```

- [ ] **Step 2: 既有用例统一注入替身**

`tests/test_ui.cpp` 里所有构造并 `show()` 设置对话框的用例（`settingsNameLengthComboWritesConfig`、`settingsListShowsAliasAndApplyCodeEditWritesMap`、`settingsListNameColumnFallsBackToQuoteName`、`settingsListNameColumnUpdatesWhenQuotesArrive`、`doubleClickListItemEditsAlias`、`settingsDialogBuildsAllTabs`），把

```cpp
        SettingsDialog dlg(&w, &w);
        dlg.show();
```

改为

```cpp
        StubNam nam;  // 防止 showEvent 触发真实联网；空 body ⇒ 不产生名称
        SettingsDialog dlg(&w, &w);
        dlg.nameSource()->setNetworkAccessManager(&nam);
        dlg.show();
```

- [ ] **Step 3: 写新用例**

在 `settingsListNameColumnUpdatesWhenQuotesArrive()` 之后追加：

```cpp
    void settingsNamesFetchedOnOpen() {
        QJsonObject cfg = baseConfig();
        cfg["codes"] = QJsonArray{"sh600030", "sz000001"};
        cfg["checked_codes"] = cfg["codes"];
        ProbeWindow w(cfg);
        w.show();
        QTest::qWait(100);

        StubNam nam;
        nam.body = nameBody();
        SettingsDialog dlg(&w, &w);
        dlg.nameSource()->setNetworkAccessManager(&nam);
        dlg.show();
        QTest::qWait(200);

        QCOMPARE(nam.urls.size(), 1);  // 一次批量请求
        QVERIFY(nam.urls.first().contains("sh600030"));
        QVERIFY(nam.urls.first().contains("sz000001"));

        auto* tree = dlg.findChild<QTreeWidget*>("codeList");
        QVERIFY(tree);
        QCOMPARE(tree->topLevelItem(0)->text(1), QString("中信证券"));
        dlg.close();
    }

    void settingsNamesSkipAliasedRows() {
        QJsonObject cfg = baseConfig();
        cfg["codes"] = QJsonArray{"sh600030", "sz000001"};
        cfg["checked_codes"] = cfg["codes"];
        cfg["name_map"] = QJsonObject{{"sz000001", "平安(老仓)"}};
        ProbeWindow w(cfg);
        w.show();
        QTest::qWait(100);

        StubNam nam;
        nam.body = nameBody();
        SettingsDialog dlg(&w, &w);
        dlg.nameSource()->setNetworkAccessManager(&nam);
        dlg.show();
        QTest::qWait(200);

        QCOMPARE(nam.urls.size(), 1);
        QVERIFY(nam.urls.first().contains("sh600030"));
        QVERIFY2(!nam.urls.first().contains("sz000001"), "有自定义名称的代码不该进请求");

        auto* tree = dlg.findChild<QTreeWidget*>("codeList");
        QVERIFY(tree);
        QCOMPARE(tree->topLevelItem(1)->text(1), QString("平安(老仓)"));
        dlg.close();
    }

    void settingsNameRequestedOnManualAdd() {
        ProbeWindow w(baseConfig());
        w.show();
        QTest::qWait(100);

        StubNam nam;
        nam.body = nameBody();
        SettingsDialog dlg(&w, &w);
        dlg.nameSource()->setNetworkAccessManager(&nam);
        dlg.show();
        QTest::qWait(200);
        const int before = nam.urls.size();

        QVERIFY(dlg.applyCodeEdit("600519", ""));
        QTest::qWait(200);
        QVERIFY2(nam.urls.size() > before, "手输新代码应触发一次名称请求");
        QVERIFY(nam.urls.last().contains("sh600519"));

        auto* tree = dlg.findChild<QTreeWidget*>("codeList");
        QVERIFY(tree);
        QString name;
        for (int i = 0; i < tree->topLevelItemCount(); ++i) {
            if (tree->topLevelItem(i)->data(0, Qt::UserRole).toString() == "sh600519")
                name = tree->topLevelItem(i)->text(1);
        }
        QCOMPARE(name, QString("贵州茅台"));
        dlg.close();
    }

    void settingsNamesQueuedWhileFetching() {
        ProbeWindow w(baseConfig());
        w.show();
        QTest::qWait(100);

        StubNam nam;
        nam.body = nameBody();
        nam.manual = true;  // 第一次请求挂起不返回
        SettingsDialog dlg(&w, &w);
        dlg.nameSource()->setNetworkAccessManager(&nam);
        dlg.show();
        QTest::qWait(100);
        QCOMPARE(nam.urls.size(), 1);

        QVERIFY(dlg.applyCodeEdit("600519", ""));
        QTest::qWait(50);
        QCOMPARE(nam.urls.size(), 1);  // 仍在等第一次

        nam.flush();
        QTest::qWait(200);
        QCOMPARE(nam.urls.size(), 2);  // 第一次返回后立刻补发
        QVERIFY(nam.urls.last().contains("sh600519"));
        dlg.close();
    }
```

- [ ] **Step 4: 构建确认失败**

Run:
```
scripts\build.cmd
set "PATH=C:/1/Qt/6.11.1/msvc2022_64/bin;%PATH%"
build\test_ui.exe -o report.txt,txt
```
Expected: 编译失败 `error C2039: "nameSource": is not a member of "SettingsDialog"`

- [ ] **Step 5: 实现 `SettingsDialog.h`**

```cpp
class SinaQuoteSource;  // 与既有前置声明放在一起
```

`public:` 区追加：

```cpp
    // 名称请求源（生产：内部自建；测试：替换其 NAM 以避免联网）
    SinaQuoteSource* nameSource() const { return m_names; }
```

`private:` 区追加：

```cpp
    void ensureNamesFor(const QStringList& codes);
    void pumpNameQueue();
    void applyNames(const QVector<Quote>& quotes);
    QString nameFor(const QString& code) const;
    void refreshItemName(QTreeWidgetItem* it);
    QStringList currentCodes() const;
```

`protected:` 区追加：

```cpp
    void showEvent(QShowEvent* event) override;
```

成员区追加：

```cpp
    SinaQuoteSource* m_names = nullptr;
    QHash<QString, QString> m_nameCache;  // code -> 本页抓到的行情名
    QStringList m_nameQueue;
    bool m_nameFetching = false;
```

并补 include：`#include <QHash>`、`#include "data/Quote.h"`（`QVector<Quote>`）；`#include <QShowEvent>` 可选。

- [ ] **Step 6: 实现 `SettingsDialog.cpp`**

1) 构造函数里 `ensureTab(0);` **之前**创建请求源并接线（切页重试也在这里）：

```cpp
    m_names = new SinaQuoteSource(this);
    connect(m_names, &SinaQuoteSource::quotesReady, this, &SettingsDialog::applyNames);
    connect(m_names, &SinaQuoteSource::error, this, [this](const QString&) {
        m_nameFetching = false;  // 该批丢弃（静默），继续队列里剩下的
        pumpNameQueue();
    });
    connect(m_tabs, &QTabWidget::currentChanged, this, [this](int index) {
        ensureTab(index);
        // 切回「自选列表」时补一次缺失名（也是失败后的重试入口）
        if (index == 0 && isVisible()) ensureNamesFor(currentCodes());
    });
    ensureTab(0);
```

（原来的 `connect(m_tabs, &QTabWidget::currentChanged, this, &SettingsDialog::ensureTab);` 删除，避免重复。）

2) 新增实现（放在 `refreshNameColumn()` 之前）：

```cpp
QString SettingsDialog::nameFor(const QString& code) const {
    const auto n = StockCode::normalize(code);
    if (!n) return QString();
    const QString cached = m_nameCache.value(*n);
    if (!cached.isEmpty()) return cached;
    return m_win ? m_win->quoteNameFor(*n) : QString();  // 浮窗兜底
}

void SettingsDialog::refreshItemName(QTreeWidgetItem* it) {
    if (!it) return;
    const QString code = it->data(0, Qt::UserRole).toString();
    const QString alias = it->data(0, Qt::UserRole + 1).toString().trimmed();
    it->setText(1, alias.isEmpty() ? nameFor(code) : alias);
}

QStringList SettingsDialog::currentCodes() const {
    QStringList out;
    if (!m_codeList) return out;
    for (int i = 0; i < m_codeList->topLevelItemCount(); ++i)
        out << m_codeList->topLevelItem(i)->data(0, Qt::UserRole).toString();
    return out;
}

void SettingsDialog::ensureNamesFor(const QStringList& codes) {
    if (!m_names) return;
    for (const QString& raw : codes) {
        const auto n = StockCode::normalize(raw);
        if (!n) continue;
        if (!nameFor(*n).isEmpty()) continue;   // 别名/缓存/浮窗已有
        if (m_nameQueue.contains(*n)) continue;
        m_nameQueue << *n;
    }
    pumpNameQueue();
}

void SettingsDialog::pumpNameQueue() {
    if (m_nameFetching || m_nameQueue.isEmpty()) return;   // 忙时不抢，返回后再 pump
    m_nameFetching = true;
    const QStringList batch = m_nameQueue;
    m_nameQueue.clear();
    m_names->fetch(batch);   // 一次请求多个代码（新浪批量接口）
}

void SettingsDialog::applyNames(const QVector<Quote>& quotes) {
    m_nameFetching = false;
    for (const Quote& q : quotes) m_nameCache.insert(q.code, q.name);
    refreshNameColumn();
    pumpNameQueue();
}

void SettingsDialog::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
    ensureNamesFor(currentCodes());  // 「上来的时候请求一次」
}
```

3) `refreshNameColumn()` 改为走 `refreshItemName()`：

```cpp
void SettingsDialog::refreshNameColumn() {
    if (!m_codeList) return;
    const QSignalBlocker blocker(m_codeList);  // 只改显示，不能触发 commitCodes()
    for (int i = 0; i < m_codeList->topLevelItemCount(); ++i)
        refreshItemName(m_codeList->topLevelItem(i));
}
```

4) 名称列的三处写入点改为调用 `refreshItemName()`：

- 初始填充循环：`item->setText(1, alias.isEmpty() ? m_win->quoteNameFor(code) : alias);` → `refreshItemName(item);`
- `addCode` lambda 新建分支：`it->setText(1, m_win->quoteNameFor(*n));` → `refreshItemName(it);`，并在该分支 `commit();` 之后加 `ensureNamesFor({*n});`（已存在分支同样加）
- `applyCodeEdit()`：`item->setText(1, value.isEmpty() ? m_win->quoteNameFor(*n) : value);` → `refreshItemName(item);`，并在 `commitCodes();` 之前加 `ensureNamesFor({*n});`

- [ ] **Step 7: 构建 + 跑测试确认通过**

Run:
```
scripts\build.cmd
set "PATH=C:/1/Qt/6.11.1/msvc2022_64/bin;%PATH%"
build\test_ui.exe -o report.txt,txt
grep -E "^FAIL|^Totals" report.txt
```
Expected: `Totals: 34 passed, 0 failed, 0 skipped`（30 + 4 新用例）

> 若 `StubReply` 拿到空 body（`readAll()` 为空）导致用例失败，改 `StubReply` 的投递顺序：先 `QMetaObject::invokeMethod(this, [this]{ emit readyRead(); })` 再延迟一帧 `emit finished()`；不要改生产代码。

- [ ] **Step 8: 提交**

```
git add src/ui/SettingsDialog.h src/ui/SettingsDialog.cpp tests/test_ui.cpp
git commit -m "feat(ui): 设置面板自主拉取名称（不受请求时段限制，含请求队列）"
```

---

### Task 3: 联想结果名称复用（零请求）

**Files:**
- Modify: `src/ui/SettingsDialog.h`（`addCode` 提为 public 成员）
- Modify: `src/ui/SettingsDialog.cpp`（lambda → 成员方法；联想项存名称；调用点传名称）
- Modify: `tests/test_ui.cpp`

- [ ] **Step 1: 写失败测试**

在 `settingsNamesQueuedWhileFetching()` 之后追加：

```cpp
    void settingsNameFromSuggestionNeedsNoRequest() {
        ProbeWindow w(baseConfig());
        w.show();
        QTest::qWait(100);

        StubNam nam;
        nam.body = nameBody();
        SettingsDialog dlg(&w, &w);
        dlg.nameSource()->setNetworkAccessManager(&nam);
        dlg.show();
        QTest::qWait(200);
        const int before = nam.urls.size();

        QVERIFY(dlg.addCode("sh601318", "中国平安"));  // 联想结果自带名称
        QTest::qWait(50);
        QCOMPARE(nam.urls.size(), before);  // 不产生任何请求

        auto* tree = dlg.findChild<QTreeWidget*>("codeList");
        QVERIFY(tree);
        QString name;
        for (int i = 0; i < tree->topLevelItemCount(); ++i)
            if (tree->topLevelItem(i)->data(0, Qt::UserRole).toString() == "sh601318")
                name = tree->topLevelItem(i)->text(1);
        QCOMPARE(name, QString("中国平安"));
        QCOMPARE(w.currentConfig().value("codes").toArray().size(), 2);
        dlg.close();
    }
```

- [ ] **Step 2: 构建确认失败**

Run: `scripts\build.cmd`
Expected: `error C2039: "addCode": is not a member of "SettingsDialog"`

- [ ] **Step 3: 实现**

`src/ui/SettingsDialog.h` `public:` 区追加：

```cpp
    // 添加（或勾选已存在的）代码；knownName 非空时直接用作名称并跳过请求
    bool addCode(const QString& codeIn, const QString& knownName = QString());
```

`src/ui/SettingsDialog.cpp`：把 `buildCodesTab()` 里的 `auto addCode = [this, commit](const QString& codeIn) -> bool { ... };` 整块删除，改为成员方法（放在 `applyCodeEdit()` 之前）：

```cpp
bool SettingsDialog::addCode(const QString& codeIn, const QString& knownName) {
    if (!m_codeList) return false;
    const auto n = StockCode::normalize(codeIn);
    if (!n) return false;

    const QString known = knownName.trimmed();
    if (!known.isEmpty()) m_nameCache.insert(*n, known);  // 联想结果 → 零请求

    QTreeWidgetItem* found = nullptr;
    for (int i = 0; i < m_codeList->topLevelItemCount(); ++i) {
        if (m_codeList->topLevelItem(i)->data(0, Qt::UserRole).toString() == *n) {
            found = m_codeList->topLevelItem(i);
            break;
        }
    }
    if (found) {
        found->setCheckState(0, Qt::Checked);
        m_codeList->setCurrentItem(found);
        refreshItemName(found);
    } else {
        found = new QTreeWidgetItem(m_codeList);
        found->setFlags((found->flags() | Qt::ItemIsUserCheckable) & ~Qt::ItemIsEditable);
        found->setData(0, Qt::UserRole, *n);
        found->setText(0, *n);
        found->setCheckState(0, Qt::Checked);
        m_codeList->setCurrentItem(found);
        refreshItemName(found);
    }
    commitCodes();
    ensureNamesFor({*n});
    return true;
}
```

调用点更新（`buildCodesTab()`）：

```cpp
    auto submitSearch = [this] {                       // 原 lambda 捕获 addCode，现直接调成员
        const QString text = m_searchEdit->text();
        ...
        if (StockCode::normalize(text)) {
            ok = addCode(text);
        } else {
            const QListWidgetItem* sel = m_suggestList->currentItem();
            if (!sel && m_suggestList->count() > 0) sel = m_suggestList->item(0);
            if (sel)
                ok = addCode(sel->data(Qt::UserRole).toString(),
                             sel->data(Qt::UserRole + 1).toString());   // 复用联想名称
        }
        ...
    };
```

联想列表填充时保存名称：

```cpp
                    it->setData(Qt::UserRole, s.code);
                    it->setData(Qt::UserRole + 1, s.name);
```

`itemActivated` 连接：

```cpp
    connect(m_suggestList, &QListWidget::itemActivated, this, [this](QListWidgetItem* it) {
        if (!it) return;
        if (addCode(it->data(Qt::UserRole).toString(), it->data(Qt::UserRole + 1).toString())) {
            m_searchEdit->clear();
            m_suggestList->clear();
            m_suggestList->setVisible(false);
        }
    });
```

（`submitSearch` 的捕获列表由 `[this, addCode]` 改为 `[this]`；`connect(m_searchEdit, &QLineEdit::returnPressed, this, [submitSearch] { submitSearch(); });` 等保持。）

- [ ] **Step 4: 构建 + 跑测试确认通过**

Run:
```
scripts\build.cmd
set "PATH=C:/1/Qt/6.11.1/msvc2022_64/bin;%PATH%"
build\test_ui.exe -o report.txt,txt
grep -E "^FAIL|^Totals" report.txt
```
Expected: `Totals: 35 passed, 0 failed, 0 skipped`

- [ ] **Step 5: 提交**

```
git add src/ui/SettingsDialog.h src/ui/SettingsDialog.cpp tests/test_ui.cpp
git commit -m "feat(ui): 搜索联想自带的名称直接复用（零请求）"
```

---

### Task 4: 收尾

**Files:**
- Modify: `README.md`
- Modify: `TODO.md`

- [ ] **Step 1: 全量测试**

Run: `scripts\test.cmd`
Expected: `100% tests passed, 0 tests failed out of 9`

- [ ] **Step 2: README**

「⚙️ 设置面板」的「自选列表」条目末尾追加一句：

```markdown
  名称列**不受「请求时段」限制**：打开设置时批量拉一次、添加代码时拉一次、切回该页再补一次；从搜索联想里选中的股票**直接带出名称**（零额外请求）。
```

- [ ] **Step 3: TODO.md 追加执行记录**

```markdown
## 2026-09-20 追加需求 2：设置面板名称自主取数

- [x] 探索/澄清 — 根因：名称列只读浮窗（受请求时段限制）+ 联想结果名称被丢弃
- [x] 设计 — `docs/superpowers/specs/2026-09-20-settings-name-fetch-design.md`（用户确认「可以的」）
- [x] 计划 — `docs/superpowers/plans/2026-09-20-settings-name-fetch.md`
- [x] 实现 — 见提交记录；`scripts\test.cmd` 9/9 通过（`test_ui` 35 用例）
- 偏差：（实现后填写）
```

- [ ] **Step 4: 提交**

```
git add README.md TODO.md
git commit -m "docs: README/TODO 同步（设置面板名称自主取数）"
```
