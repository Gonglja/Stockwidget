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
