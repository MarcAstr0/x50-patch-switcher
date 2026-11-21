#include "ui/Glyphs.h"

#include <QPainterPath>
#include <QPen>
#include <QPixmap>
#include <QtMath>

namespace Glyphs {

namespace {
// RAII: antialiasing on, state restored.
struct Scope {
    explicit Scope(QPainter &painter) : p(painter) {
        p.save();
        p.setRenderHint(QPainter::Antialiasing, true);
    }
    ~Scope() { p.restore(); }
    QPainter &p;
};
} // namespace

const QPolygonF &starPolygon()
{
    static const QPolygonF poly = [] {
        QPolygonF s;
        const qreal outer = 0.5, inner = 0.5 * 0.42;
        for (int i = 0; i < 10; ++i) {
            const qreal a = -M_PI / 2 + i * M_PI / 5;
            const qreal r = (i % 2 == 0) ? outer : inner;
            // Centre the star's visual box: the top point is at -0.5, the bottom points at ~0.405.
            s << QPointF(0.5 + r * qCos(a), 0.5 + 0.05 + r * qSin(a));
        }
        return s;
    }();
    return poly;
}

void star(QPainter &p, const QRectF &r, bool filled, const QColor &color, qreal stroke)
{
    Scope scope(p);
    p.translate(r.topLeft());
    p.scale(r.width(), r.height());
    if (filled) {
        p.setPen(Qt::NoPen);
        p.setBrush(color);
    } else {
        QPen pen(color, stroke / qMax<qreal>(1.0, r.width()));
        pen.setJoinStyle(Qt::MiterJoin);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
    }
    p.drawPolygon(starPolygon());
}

void checkMark(QPainter &p, const QRectF &r, const QColor &color, qreal stroke)
{
    Scope scope(p);
    QPen pen(color, stroke);
    pen.setCapStyle(Qt::SquareCap);
    pen.setJoinStyle(Qt::MiterJoin);
    p.setPen(pen);
    QPolygonF line;
    line << QPointF(r.left() + 0.08 * r.width(), r.top() + 0.55 * r.height())
         << QPointF(r.left() + 0.38 * r.width(), r.top() + 0.85 * r.height())
         << QPointF(r.left() + 0.95 * r.width(), r.top() + 0.18 * r.height());
    p.drawPolyline(line);
}

void chevron(QPainter &p, const QRectF &r, bool left, const QColor &color, qreal stroke)
{
    Scope scope(p);
    QPen pen(color, stroke);
    pen.setCapStyle(Qt::SquareCap);
    pen.setJoinStyle(Qt::MiterJoin);
    p.setPen(pen);
    const qreal tipX = left ? r.left() + 0.25 * r.width() : r.right() - 0.25 * r.width();
    const qreal backX = left ? r.right() - 0.25 * r.width() : r.left() + 0.25 * r.width();
    QPolygonF line;
    line << QPointF(backX, r.top() + 0.1 * r.height()) << QPointF(tipX, r.center().y())
         << QPointF(backX, r.bottom() - 0.1 * r.height());
    p.drawPolyline(line);
}

void magnifier(QPainter &p, const QRectF &r, const QColor &color, qreal stroke)
{
    Scope scope(p);
    QPen pen(color, stroke);
    pen.setCapStyle(Qt::RoundCap);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    const qreal d = 0.62 * qMin(r.width(), r.height());
    const QRectF lens(r.left() + stroke / 2, r.top() + stroke / 2, d, d);
    p.drawEllipse(lens);
    const QPointF from(lens.center().x() + 0.36 * d, lens.center().y() + 0.36 * d);
    p.drawLine(from, QPointF(r.right() - stroke / 2, r.bottom() - stroke / 2));
}

void cross(QPainter &p, const QRectF &r, const QColor &color, qreal stroke)
{
    Scope scope(p);
    QPen pen(color, stroke);
    pen.setCapStyle(Qt::SquareCap);
    p.setPen(pen);
    const QRectF q = r.adjusted(r.width() * 0.2, r.height() * 0.2, -r.width() * 0.2, -r.height() * 0.2);
    p.drawLine(q.topLeft(), q.bottomRight());
    p.drawLine(q.topRight(), q.bottomLeft());
}

void clock(QPainter &p, const QRectF &r, const QColor &color, qreal stroke)
{
    Scope scope(p);
    QPen pen(color, stroke);
    pen.setCapStyle(Qt::RoundCap);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    const qreal d = qMin(r.width(), r.height());
    const QPointF c = r.center();
    p.drawEllipse(QRectF(c.x() - d / 2 + stroke / 2, c.y() - d / 2 + stroke / 2, d - stroke, d - stroke));
    const qreal k = d / 14.0;
    p.drawLine(c, QPointF(c.x(), c.y() - 4 * k));
    p.drawLine(c, QPointF(c.x() + 3 * k, c.y()));
}

void triangleRight(QPainter &p, const QRectF &r, const QColor &color)
{
    Scope scope(p);
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    QPolygonF tri;
    tri << r.topLeft() << QPointF(r.right(), r.center().y()) << r.bottomLeft();
    p.drawPolygon(tri);
}

QIcon icon(Kind kind, const QSize &size, const QColor &color)
{
    QIcon ic;
    for (int scale = 1; scale <= 3; ++scale) {
        QPixmap pm(size * scale);
        pm.setDevicePixelRatio(scale);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        const QRectF box(0, 0, size.width(), size.height());
        switch (kind) {
        case Kind::StarOutline: star(p, box, false, color); break;
        case Kind::StarFilled: star(p, box, true, color); break;
        case Kind::ChevronLeft: chevron(p, box, true, color); break;
        case Kind::ChevronRight: chevron(p, box, false, color); break;
        case Kind::Magnifier: magnifier(p, box, color); break;
        case Kind::Cross: cross(p, box, color); break;
        case Kind::Clock: clock(p, box, color); break;
        }
        p.end();
        ic.addPixmap(pm);
    }
    return ic;
}

} // namespace Glyphs
