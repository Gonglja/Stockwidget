#include "ui/QuoteModel.h"

QuoteModel::QuoteModel(QObject* parent) : QAbstractTableModel(parent) {}

void QuoteModel::setColumns(const QVector<ColumnSpec>& cols) {
    beginResetModel();
    m_cols = cols;
    rebuildCache();
    endResetModel();
}

void QuoteModel::setQuotes(const QVector<Quote>& quotes) {
    if (quotes.size() != m_quotes.size()) {
        beginResetModel();
        m_quotes = quotes;
        rebuildCache();
        endResetModel();
        return;
    }
    QVector<QVector<QString>> newText(quotes.size());
    QVector<QVector<int>> newSign(quotes.size());
    for (int r = 0; r < quotes.size(); ++r) {
        newText[r].resize(m_cols.size());
        newSign[r].resize(m_cols.size());
        for (int c = 0; c < m_cols.size(); ++c) {
            newText[r][c] = m_cols[c].text ? m_cols[c].text(quotes[r]) : QString();
            newSign[r][c] = m_cols[c].sign ? m_cols[c].sign(quotes[r]) : 0;
        }
    }
    m_quotes = quotes;
    for (int r = 0; r < quotes.size(); ++r) {
        int first = -1, last = -1;
        for (int c = 0; c < m_cols.size(); ++c) {
            const bool changed = newText[r][c] != m_cellCache[r][c] ||
                                 newSign[r][c] != m_signCache[r][c];
            if (changed && first < 0) first = c;
            if (changed) last = c;
        }
        if (first >= 0) emit dataChanged(index(r, first), index(r, last));
    }
    m_cellCache = newText;
    m_signCache = newSign;
}

void QuoteModel::rebuildCache() {
    m_cellCache.clear();
    m_signCache.clear();
    m_cellCache.resize(m_quotes.size());
    m_signCache.resize(m_quotes.size());
    for (int r = 0; r < m_quotes.size(); ++r) {
        m_cellCache[r].resize(m_cols.size());
        m_signCache[r].resize(m_cols.size());
        for (int c = 0; c < m_cols.size(); ++c) {
            m_cellCache[r][c] = m_cols[c].text ? m_cols[c].text(m_quotes[r]) : QString();
            m_signCache[r][c] = m_cols[c].sign ? m_cols[c].sign(m_quotes[r]) : 0;
        }
    }
}

void QuoteModel::setColorScheme(bool defaultColor, const QColor& fg, qreal opacity) {
    m_defaultColor = defaultColor;
    m_fg = fg;
    m_opacity = qBound(0.0, opacity, 1.0);
    if (rowCount() > 0 && columnCount() > 0)
        emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1),
                         {Qt::ForegroundRole});
}

QColor QuoteModel::scaled(const QColor& c) const {
    QColor out(c);
    out.setAlpha(qBound(1, qRound(c.alpha() * m_opacity), 255));
    return out;
}

int QuoteModel::klineColumn() const {
    for (int c = 0; c < m_cols.size(); ++c)
        if (m_cols[c].isKLine) return c;
    return -1;
}

QString QuoteModel::quoteCodeAt(int row) const {
    return (row >= 0 && row < m_quotes.size()) ? m_quotes.at(row).code : QString();
}

QString QuoteModel::quoteNameAt(int row) const {
    return (row >= 0 && row < m_quotes.size()) ? m_quotes.at(row).name : QString();
}

int QuoteModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : m_quotes.size();
}

int QuoteModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : m_cols.size();
}

QVariant QuoteModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= m_quotes.size() || index.column() >= m_cols.size())
        return {};
    const ColumnSpec& col = m_cols.at(index.column());
    const int r = index.row();
    switch (role) {
        case Qt::DisplayRole:
            return col.isKLine ? QString() : m_cellCache[r][index.column()];
        case Qt::TextAlignmentRole:
            return int((col.rightAlign ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter);
        case Qt::ForegroundRole: {
            if (!m_defaultColor || !col.colored) return scaled(m_fg);
            const int sign = m_signCache[r][index.column()];
            if (sign > 0) return scaled(QColor(0xdd, 0x21, 0x00));
            if (sign < 0) return scaled(QColor(0x01, 0x99, 0x33));
            return scaled(QColor(0x49, 0x49, 0x49));
        }
        case KLineRole: {
            if (!col.isKLine) return {};
            const Quote& q = m_quotes.at(r);
            return QVariantList{q.kOpen, q.kClose, q.kHigh, q.kLow, q.kPrev};
        }
        default:
            return {};
    }
}

QVariant QuoteModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal && section < m_cols.size())
        return m_cols.at(section).header;
    return {};
}
