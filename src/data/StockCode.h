#pragma once
#include <QString>
#include <QStringList>
#include <optional>

namespace StockCode {
std::optional<QString> normalize(const QString& input);
QStringList normalizeList(const QStringList& inputs);
}
