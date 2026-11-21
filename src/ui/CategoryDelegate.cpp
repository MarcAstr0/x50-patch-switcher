#include "ui/CategoryDelegate.h"

#include "ui/Theme.h"

#include <QPainter>

CategoryDelegate::CategoryDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
    , m_regular(Theme::roleFont(Theme::Role::Body))
    , m_bold(Theme::roleFont(Theme::Role::BodyStrong))
{
}

QSize CategoryDelegate::sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const
{
    return QSize(0, Theme::browseCategoryRowHeight);
}

void CategoryDelegate::paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    const bool selected = option.state & QStyle::State_Selected;
    const QRect r = option.rect;
    if (selected) {
        p->fillRect(r, QColor(Theme::selRow));
        p->fillRect(QRect(r.left(), r.top(), Theme::selectedRowAccent, r.height()), QColor(Theme::cyan));
    } else if (option.state & QStyle::State_MouseOver) {
        p->fillRect(r, QColor(Theme::panelRaised));
    }
    p->setFont(selected ? m_bold : m_regular);
    p->setPen(QColor(Theme::text));
    p->drawText(r.adjusted(Theme::s3, 0, -Theme::s3, 0), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                index.data().toString());
}
