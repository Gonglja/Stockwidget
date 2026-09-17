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
}  // namespace NameAlias
