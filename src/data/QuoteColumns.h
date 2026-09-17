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
QString sortKeyFor(const QString& header);
bool isVisible(const QJsonObject& cfg, const QString& header);
QVector<ColumnSpec> activeColumns(const QJsonObject& cfg);
}
