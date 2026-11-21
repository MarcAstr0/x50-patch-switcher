#include "ui/PanelFrame.h"

#include "ui/Theme.h"

#include <QPainter>

PanelFrame::PanelFrame(QWidget *parent)
    : QFrame(parent)
{
    setFrameShape(QFrame::NoFrame);
}

void PanelFrame::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(Theme::panel));
    p.drawRoundedRect(QRectF(rect()), Theme::radiusPanel, Theme::radiusPanel);
}
