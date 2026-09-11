#include "ui/KLineDelegate.h"
#include "ui/QuoteModel.h"
#include <QPainter>

namespace {
const QColor kUp(0xdd, 0x21, 0x00);
const QColor kDown(0x01, 0x99, 0x33);
const QColor kNeutral(0x49, 0x49, 0x49);
}

KLineDelegate::KLineDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

void KLineDelegate::setColorScheme(bool defaultColor, const QColor& fg) {
    m_defaultColor = defaultColor;
    m_fg = fg;
}

void KLineDelegate::setPointSize(int pt) {
    m_scale = qBound(0.5, static_cast<double>(pt) / 12.0, 1.5);
}

void KLineDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                          const QModelIndex& index) const {
    const QVariantList k = index.data(QuoteModel::KLineRole).toList();
    if (k.size() != 5) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }
    double o = k[0].toDouble(), c = k[1].toDouble(), h = k[2].toDouble();
    double l = k[3].toDouble(), p = k[4].toDouble();
    if (h < l) std::swap(h, l);

    const QRect cell = option.rect;
    const QRect rect = cell.adjusted(2, 2, -2, -2);
    const int vpad = qMax(2, int(rect.height() * (0.12 + 0.06 * (m_scale - 1))));
    const QRect krect(rect.left(), rect.top() + vpad, rect.width(),
                      qMax(2, rect.height() - 2 * vpad));

    const double lo = qMin(l, p), hi = qMax(h, p);
    auto yFor = [&](double v) {
        const double y = (hi == lo) ? 0.5 : (v - lo) / (hi - lo);
        return krect.top() + (1.0 - y) * krect.height();
    };
    const double yO = yFor(o), yC = yFor(c), yH = yFor(h), yL = yFor(l), yP = yFor(p);

    painter->save();
    painter->setClipRect(cell);
    painter->setRenderHint(QPainter::Antialiasing, true);

    const int bodyW = qMax(5, qMin(int(krect.width() * 0.4 * m_scale), 10));
    const double x = krect.center().x();

    QColor dash(m_defaultColor ? kNeutral : m_fg);
    dash.setAlpha(180);
    painter->setPen(QPen(dash, 1, Qt::DashLine));
    painter->drawLine(QPointF(x - bodyW, yP), QPointF(x + bodyW, yP));

    QColor kcolor = m_fg;
    if (m_defaultColor) {
        if (c > o) kcolor = kUp;
        else if (c < o) kcolor = kDown;
        else kcolor = kNeutral;
    }

    const double top = qMin(yO, yC), bot = qMax(yO, yC);
    const double bodyH = qMax(2.0, bot - top);
    const double bodyX = x - bodyW / 2.0;

    painter->setPen(QPen(kcolor, 1));
    if (c != o) painter->drawRect(QRectF(bodyX, top, bodyW, bodyH));
    else painter->drawLine(QPointF(bodyX, yC), QPointF(bodyX + bodyW, yC));
    if (yH < top) painter->drawLine(QPointF(x, yH), QPointF(x, top));
    if (yL > bot) painter->drawLine(QPointF(x, bot), QPointF(x, yL));
    if (c < o) painter->fillRect(QRectF(bodyX, top, bodyW, bodyH), kcolor);

    painter->restore();
}
