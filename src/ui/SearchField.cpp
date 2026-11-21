#include "ui/SearchField.h"

#include "ui/Glyphs.h"
#include "ui/Theme.h"

#include <QPainter>

SearchField::SearchField(QWidget *parent)
    : QLineEdit(parent)
{
}

void SearchField::paintEvent(QPaintEvent *e)
{
    QLineEdit::paintEvent(e);
    QPainter p(this);
    Glyphs::magnifier(p, QRectF(12, (height() - 16) / 2.0, 16, 16), QColor(Theme::textDim));
}
