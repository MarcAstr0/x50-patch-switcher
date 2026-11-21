#pragma once

#include <QFont>
#include <QFontMetrics>
#include <QStyledItemDelegate>

// Paints one result cell per DESIGN 7.7 (row 36 high, columns Star, Address, Name,
// Edited, Category, Used) from Theme constants. Fonts and metrics are built once;
// paint() only draws, and builds an elided string only when a text really overflows.
// A click inside the star column's star toggles the favourite: starClicked(row).
class ResultDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit ResultDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option,
                     const QModelIndex &index) override;

    // The star's hit box inside a row cell of column 0 (cell rectangle in, box out).
    static QRect starHitRect(const QRect &cell);

signals:
    void starClicked(int row);

private:
    QFont m_address, m_body, m_small, m_pill;
    QFontMetrics m_bodyMetrics, m_smallMetrics, m_pillMetrics;
    int m_pillWidth = 0;
};
