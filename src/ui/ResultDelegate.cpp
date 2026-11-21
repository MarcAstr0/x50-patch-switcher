#include "ui/ResultDelegate.h"

#include "ui/Glyphs.h"
#include "ui/ResultModel.h"
#include "ui/Theme.h"

#include <QEvent>
#include <QMouseEvent>
#include <QPainter>

namespace {
QFont pillFont()
{
    QFont f = Theme::roleFont(Theme::Role::Label);
    f.setPixelSize(11);  // DESIGN 7.7: `label` style at 11 px
    return f;
}
} // namespace

ResultDelegate::ResultDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
    , m_address(Theme::roleFont(Theme::Role::Address))
    , m_body(Theme::roleFont(Theme::Role::Body))
    , m_small(Theme::roleFont(Theme::Role::Small))
    , m_pill(pillFont())
    , m_bodyMetrics(m_body)
    , m_smallMetrics(m_small)
    , m_pillMetrics(m_pill)
{
    m_pillWidth = m_pillMetrics.horizontalAdvance(QStringLiteral("EDITED")) + 12;
}

QSize ResultDelegate::sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const
{
    return QSize(0, Theme::resultRowHeight);
}

QRect ResultDelegate::starHitRect(const QRect &cell)
{
    const int s = Theme::resultStarSize;
    // 36 px star column, right of the 12 px row padding; the hit box is a little larger than the star.
    const QRect star(cell.left() + Theme::resultCellPadding + (Theme::colStar - s) / 2, cell.center().y() - s / 2, s, s);
    return star.adjusted(-6, -6, 6, 6);
}

void ResultDelegate::paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    const QRect r = option.rect;
    const bool selected = option.state & QStyle::State_Selected;
    const bool hover = option.state & QStyle::State_MouseOver;
    const int pad = Theme::resultCellPadding;

    if (selected)
        p->fillRect(r, QColor(Theme::selRow));
    else if (hover)
        p->fillRect(r, QColor(Theme::panelRaised));
    if (!selected)
        p->fillRect(QRect(r.left(), r.bottom(), r.width(), 1), QColor(Theme::line));

    // The 12 px row padding applies to the row's outer edges (star, used) only.
    switch (index.column()) {
    case ResultModel::ColStar: {
        if (selected)
            p->fillRect(QRect(r.left(), r.top(), Theme::selectedRowAccent, r.height()), QColor(Theme::cyan));
        if (index.data(ResultModel::CurrentRole).toBool())
            Glyphs::triangleRight(*p, QRectF(r.left() + 5, r.center().y() - 4.5, 8, 10), QColor(Theme::cyan));
        const int s = Theme::resultStarSize;
        const QRectF star(r.left() + pad + (Theme::colStar - s) / 2.0, r.center().y() - s / 2.0 + 0.5, s, s);
        const bool fav = index.data(ResultModel::FavouriteRole).toBool();
        Glyphs::star(*p, star, fav, QColor(fav ? Theme::amber : Theme::textDim));
        break;
    }
    case ResultModel::ColAddress:
        p->setFont(m_address);
        p->setPen(QColor(selected ? Theme::text : Theme::textDim));
        p->drawText(r, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, index.data().toString());
        break;
    case ResultModel::ColName: {
        p->setFont(m_body);
        p->setPen(QColor(Theme::text));
        const QString name = index.data().toString();
        const QRect area = r.adjusted(0, 0, -Theme::s2, 0);  // elide 8 px short of the column's right edge
        if (m_bodyMetrics.horizontalAdvance(name) > area.width())
            p->drawText(area, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                        m_bodyMetrics.elidedText(name, Qt::ElideRight, area.width()));
        else
            p->drawText(area, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, name);
        break;
    }
    case ResultModel::ColEdited:
        if (index.data(ResultModel::EditedRole).toBool()) {
            const QRectF pill(r.left(), r.center().y() - 10, m_pillWidth, 20);
            p->save();
            p->setRenderHint(QPainter::Antialiasing, true);
            p->setBrush(Qt::NoBrush);
            p->setPen(QPen(QColor(Theme::amber), 1));
            p->drawRoundedRect(pill.adjusted(0.5, 0.5, -0.5, -0.5), Theme::radiusPill, Theme::radiusPill);
            p->setFont(m_pill);
            p->drawText(pill, Qt::AlignCenter | Qt::TextSingleLine, QStringLiteral("EDITED"));
            p->restore();
        }
        break;
    case ResultModel::ColCategory: {
        p->setFont(m_small);
        p->setPen(QColor(Theme::textDim));
        const QString cat = index.data().toString();
        const QRect area = r.adjusted(0, 0, -Theme::s2, 0);
        if (m_smallMetrics.horizontalAdvance(cat) > area.width())
            p->drawText(area, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                        m_smallMetrics.elidedText(cat, Qt::ElideRight, area.width()));
        else
            p->drawText(area, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, cat);
        break;
    }
    case ResultModel::ColUsed:
        p->setFont(m_small);
        p->setPen(QColor(Theme::textDim));
        p->drawText(r.adjusted(0, 0, -pad, 0), Qt::AlignRight | Qt::AlignVCenter | Qt::TextSingleLine, index.data().toString());
        break;
    default:
        break;
    }
}

bool ResultDelegate::editorEvent(QEvent *event, QAbstractItemModel *, const QStyleOptionViewItem &option,
                                 const QModelIndex &index)
{
    if (index.column() != ResultModel::ColStar || event->type() != QEvent::MouseButtonRelease)
        return false;
    const auto *me = static_cast<QMouseEvent *>(event);
    if (me->button() != Qt::LeftButton || !starHitRect(option.rect).contains(me->position().toPoint()))
        return false;
    emit starClicked(index.row());
    return true;
}
