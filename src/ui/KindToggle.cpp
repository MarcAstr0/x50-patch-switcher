#include "ui/KindToggle.h"

#include "ui/Glyphs.h"
#include "ui/Theme.h"

#include <QPainter>

KindToggle::KindToggle(const QString &text, QWidget *parent)
    : QToolButton(parent)
{
    setText(text);
    setObjectName(QStringLiteral("kindToggle"));
    setCheckable(true);
    setChecked(true);
    setFixedSize(Theme::kindToggleWidth, Theme::browseToolbarHeight);
    m_textWidth = QFontMetrics(Theme::roleFont(Theme::Role::BodyStrong)).horizontalAdvance(text);
    // app.qss writes min-width: 110px, which Qt takes as the content box: 110 + 20 padding + 2 border
    // = 132, not DESIGN's 112 (the padding was not counted). Until app.qss says 90, a widget-level
    // rule gives the content width 112 - padding - border (the checked state has a 2 px border).
    const int mark = Theme::kindToggleMarkReserve;
    const int unchecked = Theme::kindToggleWidth - mark - 2 * Theme::borderControl;
    const int checked = Theme::kindToggleWidth - mark - 2 * Theme::borderFocus;
    setStyleSheet(QStringLiteral("QToolButton#kindToggle { min-width: %1px; max-width: %1px; }"
                                 "QToolButton#kindToggle:checked { min-width: %2px; max-width: %2px; }")
                      .arg(unchecked)
                      .arg(checked));
}

void KindToggle::paintEvent(QPaintEvent *e)
{
    QToolButton::paintEvent(e);
    if (!isChecked())
        return;
    const int contentLeft = Theme::borderFocus + Theme::kindToggleMarkReserve, contentW = width() - 2 * Theme::borderFocus - Theme::kindToggleMarkReserve;
    const int textLeft = contentLeft + (contentW - m_textWidth) / 2;
    QPainter p(this);
    Glyphs::checkMark(p, QRectF(textLeft - Theme::kindToggleMarkReserve, (height() - 12) / 2.0, 12, 12), QColor(Theme::text));
}
