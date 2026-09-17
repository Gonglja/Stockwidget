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
}  // namespace QuoteSort
