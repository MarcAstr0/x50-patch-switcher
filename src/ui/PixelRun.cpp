#include "ui/PixelRun.h"

#include <QPainter>

namespace PixelRun {

void draw(QPainter &p, const Run &run)
{
    for (const Glyph &g : run)
        p.drawText(g.pos, g.text);
}

} // namespace PixelRun
