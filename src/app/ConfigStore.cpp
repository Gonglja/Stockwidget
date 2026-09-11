#include "app/ConfigStore.h"
#include "data/QuoteColumns.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>

namespace {

QString configDir() {
    const QString override = qEnvironmentVariable("SW_CONFIG_DIR");
    if (!override.isEmpty()) return override;
    QString base = qEnvironmentVariable("APPDATA");
    if (base.isEmpty()) base = QDir::homePath();
    return base + QStringLiteral("/StockWidget");
}

}  // namespace

QString ConfigStore::configFilePath() {
    return configDir() + QStringLiteral("/SW_config.json");
}

QJsonObject ConfigStore::load() {
    QFile f(configFilePath());
    if (!f.open(QIODevice::ReadOnly)) return {};
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) return {};
    return doc.object();
}

bool ConfigStore::save(const QJsonObject& cfg) {
    QDir().mkpath(configDir());
    QSaveFile f(configFilePath());
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(QJsonDocument(cfg).toJson(QJsonDocument::Indented));
    return f.commit();
}

QJsonObject ConfigStore::normalize(const QJsonObject& raw) {
    QJsonObject out = raw;

    if (!out.contains(QStringLiteral("checked_codes")) &&
        out.contains(QStringLiteral("visible_codes"))) {
        out.insert(QStringLiteral("checked_codes"), out.value(QStringLiteral("visible_codes")));
    }

    if (out.contains(QStringLiteral("flags"))) {
        QJsonObject oldFlags;
        const QJsonValue flags = out.value(QStringLiteral("flags"));
        const QStringList headers = QuoteColumns::allHeaders();
        if (flags.isArray()) {
            const QJsonArray arr = flags.toArray();
            for (int i = 0; i < headers.size() && i < arr.size(); ++i)
                oldFlags.insert(headers.at(i), arr.at(i).toBool());
        } else if (flags.isObject()) {
            const QJsonObject obj = flags.toObject();
            for (const QString& h : headers) oldFlags.insert(h, obj.value(h).toBool());
        }
        for (const QString& h : headers) {
            const QString key = QuoteColumns::configKeyFor(h);
            if (key.isEmpty() || key == QStringLiteral("b1s1_visible")) continue;
            if (!out.contains(key)) out.insert(key, oldFlags.value(h).toBool(false));
        }
        if (!out.contains(QStringLiteral("b1s1_visible"))) {
            out.insert(QStringLiteral("b1s1_visible"),
                       oldFlags.value(QStringLiteral("买一")).toBool(false) ||
                           oldFlags.value(QStringLiteral("卖一")).toBool(false));
        }
        out.remove(QStringLiteral("flags"));
    }

    if (!out.contains(QStringLiteral("b1s1_display"))) {
        out.insert(QStringLiteral("b1s1_display"),
                   out.value(QStringLiteral("b1s1_price")).toBool(false)
                       ? QStringLiteral("price")
                       : QStringLiteral("qty"));
    }
    return out;
}
