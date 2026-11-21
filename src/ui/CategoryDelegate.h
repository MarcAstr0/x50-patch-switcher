#pragma once

#include <QFont>
#include <QStyledItemDelegate>

// Category rows (DESIGN 7.6): 32 high, text 12 px from the row's left (34 px on the Favourites
// and Recent rows, which carry a 14 px painted glyph at x = 12), selected = `sel-row` fill,
// 4 px `cyan` left bar, bold text; hover = `panel-raised`. The separator row is 9 high: a 1 px
// `line` rule inset 12 px, 4 px empty above and below. Painted here rather than by QSS, whose
// item padding does not reach the text of unselected rows in a list widget.
class CategoryDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    // Stored in Qt::UserRole + 1 of every item of the category list.
    enum RowKind { Favourites, Recent, Separator, All, Category };
    static constexpr int KindRole = Qt::UserRole + 1;

    explicit CategoryDelegate(QObject *parent = nullptr);

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

private:
    QFont m_regular, m_bold;
};
