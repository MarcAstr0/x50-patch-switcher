#pragma once

#include <QFrame>

// A browse panel (DESIGN 7.6, 7.9): `panel` fill, radius 8, painted from Theme so no
// QSS is needed for it. Holds other widgets; paints only when Qt asks.
class PanelFrame : public QFrame
{
public:
    explicit PanelFrame(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
};
