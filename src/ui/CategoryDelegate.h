#pragma once

#include <QFont>
#include <QStyledItemDelegate>

// Category rows (DESIGN 7.6): 32 high, text 12 px from the row's left, selected = `sel-row`
// fill, 4 px `cyan` left bar, bold text; hover = `panel-raised`. Painted here rather than by
// QSS, whose item padding does not reach the text of unselected rows in a list widget.
class CategoryDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit CategoryDelegate(QObject *parent = nullptr);

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

private:
    QFont m_regular, m_bold;
};
