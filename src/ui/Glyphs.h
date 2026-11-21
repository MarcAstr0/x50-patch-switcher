#pragma once

#include <QColor>
#include <QIcon>
#include <QPainter>
#include <QPointF>
#include <QPolygonF>
#include <QRectF>
#include <QSize>

// Painted glyphs of the browse view (DESIGN 7.4, 7.5, 7.7, 7.8): star, check mark,
// chevrons, magnifier, clear cross. Nothing is a bitmap asset. Every function paints
// into the box `r` with the given colour and restores the painter's state.
namespace Glyphs {

enum class Kind { StarOutline, StarFilled, ChevronLeft, ChevronRight, Magnifier, Cross };

// Five-point star in the unit square (0..1), computed once; scale and translate to use.
const QPolygonF &starPolygon();

// Filled star, or a stroked outline of width `stroke` (px) when not filled.
void star(QPainter &p, const QRectF &r, bool filled, const QColor &color, qreal stroke = 1.5);
void checkMark(QPainter &p, const QRectF &r, const QColor &color, qreal stroke = 2.0);
void chevron(QPainter &p, const QRectF &r, bool left, const QColor &color, qreal stroke = 2.0);
void magnifier(QPainter &p, const QRectF &r, const QColor &color, qreal stroke = 2.0);
void cross(QPainter &p, const QRectF &r, const QColor &color, qreal stroke = 1.5);
// Solid right-pointing triangle filling `r`.
void triangleRight(QPainter &p, const QRectF &r, const QColor &color);

// A QIcon of `kind` at the logical `size`, painted at 1x, 2x and 3x so Qt picks a crisp
// pixmap at any device pixel ratio.
QIcon icon(Kind kind, const QSize &size, const QColor &color);

} // namespace Glyphs
