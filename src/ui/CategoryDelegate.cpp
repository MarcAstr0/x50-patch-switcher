#include "ui/CategoryDelegate.h"

#include "ui/Glyphs.h"
#include "ui/Theme.h"

#include <QPainter>

CategoryDelegate::CategoryDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
    , m_regular(Theme::roleFont(Theme::Role::Body))
    , m_bold(Theme::roleFont(Theme::Role::BodyStrong))
{
}

QSize CategoryDelegate::sizeHint(const QStyleOptionViewItem &, const QModelIndex &index) const
{
    if (index.data(KindRole).toInt() == Separator)
        return QSize(0, Theme::browseCategorySeparatorHeight);
    return QSize(0, Theme::browseCategoryRowHeight);
}

void CategoryDelegate::paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    const bool selected = option.state & QStyle::State_Selected;
    const QRect r = option.rect;
    const int kind = index.data(KindRole).toInt();
    if (kind == Separator) {
        p->fillRect(QRect(r.left() + Theme::s3, r.top() + 4, r.width() - 2 * Theme::s3, 1), QColor(Theme::line));
        return;
    }
    if (selected) {
        p->fillRect(r, QColor(Theme::selRow));
        p->fillRect(QRect(r.left(), r.top(), Theme::selectedRowAccent, r.height()), QColor(Theme::cyan));
    } else if (option.state & QStyle::State_MouseOver) {
        p->fillRect(r, QColor(Theme::panelRaised));
    }
    int textLeft = Theme::s3;
    if (kind == Favourites || kind == Recent) {
        const QRectF box(r.left() + Theme::s3, r.top() + (r.height() - 14) / 2.0, 14, 14);
        if (kind == Favourites)
            Glyphs::star(*p, box, true, QColor(Theme::amber));  // amber in every state
        else
            Glyphs::clock(*p, box, QColor(selected ? Theme::text : Theme::textDim));
        textLeft = Theme::s3 + 14 + Theme::s2;
    }
    p->setFont(selected ? m_bold : m_regular);
    p->setPen(QColor(Theme::text));
    p->drawText(r.adjusted(textLeft, 0, -Theme::s3, 0), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                index.data().toString());
}
