#pragma once

#include <QMainWindow>

class Catalogue;
class DisplayStrip;

// Browse window. T3.1 hosts only the display strip at the top (DESIGN 7.1 layout:
// 16 px margin, 12 px gap); the rest of the browse layout arrives with T3.2.
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    // `catalogue` is not owned and may be null (names then show as "---").
    explicit MainWindow(const Catalogue *catalogue = nullptr, QWidget *parent = nullptr);

    DisplayStrip *displayStrip() const { return m_strip; }

private:
    // DESIGN 7.1: the minimum height grows with the strip when k > 1; recomputed when it changes.
    void updateMinimumHeight();

    DisplayStrip *m_strip = nullptr;
};
