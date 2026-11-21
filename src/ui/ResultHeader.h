#pragma once

#include <QFont>
#include <QHeaderView>

// Header of the result list (DESIGN 7.7): `label` style titles (Barlow SemiBold 12 px,
// 1.5 px tracking) in `text-dim` on `panel`. QSS cannot set letter spacing, so the sections
// are painted here. Titles of the middle columns start at the column's left edge (the row
// padding of 12 applies to the row's outer edges only); the right-aligned title keeps the 12.
class ResultHeader : public QHeaderView
{
    Q_OBJECT
public:
    explicit ResultHeader(QWidget *parent = nullptr);

protected:
    void paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const override;

private:
    QFont m_font;
};
