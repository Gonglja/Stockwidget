#pragma once
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QVector>

class QNetworkReply;

struct StockSuggestion {
    QString code;  // 形如 sh600000
    QString name;  // 形如 浦发银行
};

namespace SuggestParser {
// 解析新浪联想接口返回：var suggestvalue="名称,类型,代码,带前缀代码,拼音,;...";
QVector<StockSuggestion> parseSuggest(const QString& text);
}

class StockSuggestSource : public QObject {
    Q_OBJECT
public:
    explicit StockSuggestSource(QObject* parent = nullptr);
    void query(const QString& keyword);

signals:
    void suggestionsReady(const QVector<StockSuggestion>& items);
    void error(const QString& message);

private:
    QNetworkAccessManager m_nam;
    QNetworkReply* m_reply = nullptr;
};
