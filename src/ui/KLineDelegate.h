#pragma once
#include <QColor>
#include <QStyledItemDelegate>

class KLineDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit KLineDelegate(QObject* parent = nullptr);
    void setColorScheme(bool defaultColor, const QColor& fg, qreal opacity = 1.0);
    void setPointSize(int pt);
    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;

private:
    QColor scaled(const QColor& c) const;
    bool m_defaultColor = false;
    qreal m_opacity = 1.0;
    QColor m_fg = QColor(QStringLiteral("#FFFFFF"));
    double m_scale = 1.0;
};
