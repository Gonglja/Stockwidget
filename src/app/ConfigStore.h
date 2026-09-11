#pragma once
#include <QJsonObject>
#include <QString>

namespace ConfigStore {
QString configFilePath();
QJsonObject load();
bool save(const QJsonObject& cfg);
QJsonObject normalize(const QJsonObject& raw);
}
