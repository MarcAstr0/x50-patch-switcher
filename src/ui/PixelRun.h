#pragma once

#include <QPoint>
#include <QString>
#include <QVector>

class QPainter;

// A line of Pixel Operator text laid out one character at an integer device position
// (DESIGN 2.2 rule 2). Qt 6.4's FreeType engine keeps unrounded advances, so a whole string
// drifts off the pixel grid after a few glyphs and grey fringes appear; placing every glyph
// at the sum of whole-pixel advances keeps the face crisp. Shared by the browse display
// strip, the live top-bar readout and the live button names.
namespace PixelRun {

struct Glyph
{
    QPoint pos;    // baseline origin, device px
    QString text;  // exactly one character
};
using Run = QVector<Glyph>;

// One drawText call per glyph with the painter's current font and pen. The painter must
// already be in device space (Theme::enterDeviceSpace).
void draw(QPainter &p, const Run &run);

} // namespace PixelRun
