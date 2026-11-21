#include "ui/ResultHeader.h"

#include "ui/Theme.h"

#include <QPainter>

ResultHeader::ResultHeader(QWidget *parent)
    : QHeaderView(Qt::Horizontal, parent)
    , m_font(Theme::roleFont(Theme::Role::Label))
{
    setFont(m_font);
}

void ResultHeader::paintSection(QPainter *p, const QRect &rect, int logicalIndex) const
{
    p->fillRect(rect, QColor(Theme::panel));
    const QString title = model() ? model()->headerData(logicalIndex, Qt::Horizontal).toString() : QString();
    if (title.isEmpty())
        return;
    const int align = model()->headerData(logicalIndex, Qt::Horizontal, Qt::TextAlignmentRole).toInt();
    const bool right = (align & Qt::AlignRight) != 0;
    p->setFont(m_font);
    p->setPen(QColor(Theme::textDim));
    p->drawText(rect.adjusted(0, 0, right ? -Theme::resultCellPadding : 0, 0),
                (right ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter | Qt::TextSingleLine, title);
}
