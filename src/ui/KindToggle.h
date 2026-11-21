#pragma once

#include <QFontMetrics>
#include <QToolButton>

// Kind toggle (DESIGN 7.5): exactly Theme::kindToggleWidth x browseToolbarHeight, with the
// painted check mark left of the text when on, so the state is not shown by colour alone.
// The QSS reserves 18 px of left padding for the mark. The text width is measured once.
class KindToggle : public QToolButton
{
    Q_OBJECT
public:
    explicit KindToggle(const QString &text, QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    int m_textWidth = 0;  // bold body-strong advance of text()
};
