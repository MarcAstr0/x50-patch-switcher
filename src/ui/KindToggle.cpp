#include "ui/KindToggle.h"

#include "ui/Glyphs.h"
#include "ui/Theme.h"

#include <QPainter>

namespace {
constexpr int kMarkReserve = 18;  // left padding in app.qss
}

KindToggle::KindToggle(const QString &text, QWidget *parent)
    : QToolButton(parent)
{
    setText(text);
    setObjectName(QStringLiteral("kindToggle"));
    setCheckable(true);
    setChecked(true);
    setFixedSize(Theme::kindToggleWidth, Theme::browseToolbarHeight);
    m_textWidth = QFontMetrics(Theme::roleFont(Theme::Role::BodyStrong)).horizontalAdvance(text);
    // app.qss sizes the button by its content box (88 px of content, plus padding and border),
    // which would make it wider than DESIGN's 100 x 40. Local override, border-aware: the
    // checked state has a 2 px border, the unchecked a 1 px border.
    const int unchecked = Theme::kindToggleWidth - kMarkReserve - 2 * Theme::borderControl;
    const int checked = Theme::kindToggleWidth - kMarkReserve - 2 * Theme::borderFocus;
    const int hUnchecked = Theme::browseToolbarHeight - 2 * Theme::borderControl;
    const int hChecked = Theme::browseToolbarHeight - 2 * Theme::borderFocus;
    setStyleSheet(QStringLiteral("QToolButton#kindToggle { min-width: %1px; max-width: %1px; min-height: %2px; max-height: %2px; }"
                                 "QToolButton#kindToggle:checked { min-width: %3px; max-width: %3px; min-height: %4px; max-height: %4px; }")
                      .arg(unchecked)
                      .arg(hUnchecked)
                      .arg(checked)
                      .arg(hChecked));
}

void KindToggle::paintEvent(QPaintEvent *e)
{
    QToolButton::paintEvent(e);
    if (!isChecked())
        return;
    const int contentLeft = Theme::borderFocus + kMarkReserve, contentW = width() - 2 * Theme::borderFocus - kMarkReserve;
    const int textLeft = contentLeft + (contentW - m_textWidth) / 2;
    QPainter p(this);
    Glyphs::checkMark(p, QRectF(textLeft - 6 - 12, (height() - 12) / 2.0, 12, 12), QColor(Theme::text));
}
