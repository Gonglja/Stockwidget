#pragma once
#include "data/Quote.h"
#include "data/QuoteColumns.h"
#include <QAbstractTableModel>
#include <QColor>

class QuoteModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Roles { KLineRole = Qt::UserRole + 1 };
    explicit QuoteModel(QObject* parent = nullptr);

    void setColumns(const QVector<ColumnSpec>& cols);
    void setQuotes(const QVector<Quote>& quotes);
    void setColorScheme(bool defaultColor, const QColor& fg, qreal opacity = 1.0);
    int klineColumn() const;
    QString quoteCodeAt(int row) const;
    QString quoteNameAt(int row) const;

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

private:
    void rebuildCache();
    QColor scaled(const QColor& c) const;
    QVector<ColumnSpec> m_cols;
    QVector<Quote> m_quotes;
    QVector<QVector<QString>> m_cellCache;
    QVector<QVector<int>> m_signCache;
    bool m_defaultColor = false;
    qreal m_opacity = 1.0;
    QColor m_fg = QColor(QStringLiteral("#FFFFFF"));
};
