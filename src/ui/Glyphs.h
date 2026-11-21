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

enum class Kind { StarOutline, StarFilled, ChevronLeft, ChevronRight, Magnifier, Cross, Clock };

// Five-point star in the unit square (0..1), computed once; scale and translate to use.
const QPolygonF &starPolygon();

// Filled star, or a stroked outline of width `stroke` (px) when not filled.
void star(QPainter &p, const QRectF &r, bool filled, const QColor &color, qreal stroke = 1.5);
void checkMark(QPainter &p, const QRectF &r, const QColor &color, qreal stroke = 2.0);
void chevron(QPainter &p, const QRectF &r, bool left, const QColor &color, qreal stroke = 2.0);
// Chevron whose INK box (stroke included) is exactly `ink`: round caps and joins, centre line
// inset by half the stroke (DESIGN 7.8: 7 x 12 ink, 2 px stroke).
void chevronInk(QPainter &p, const QRectF &ink, bool left, const QColor &color, qreal stroke = 2.0);
void magnifier(QPainter &p, const QRectF &r, const QColor &color, qreal stroke = 2.0);
void cross(QPainter &p, const QRectF &r, const QColor &color, qreal stroke = 1.5);
// Clock (DESIGN 7.6): circle filling `r` (outer diameter = the smaller side), `stroke` px, no fill,
// two hands from the centre (up 4 px, right 3 px at the 14 px design size, scaled with `r`), round caps.
void clock(QPainter &p, const QRectF &r, const QColor &color, qreal stroke = 1.5);
// Solid right-pointing triangle filling `r`.
void triangleRight(QPainter &p, const QRectF &r, const QColor &color);

// A QIcon of `kind` at the logical `size`, painted at 1x, 2x and 3x so Qt picks a crisp
// pixmap at any device pixel ratio.
QIcon icon(Kind kind, const QSize &size, const QColor &color);

} // namespace Glyphs
