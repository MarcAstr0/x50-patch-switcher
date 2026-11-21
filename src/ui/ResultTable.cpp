#include "ui/ResultTable.h"

#include "ui/ResultHeader.h"
#include "ui/Theme.h"

#include <QPainter>

ResultTable::ResultTable(QWidget *parent)
    : QTableView(parent)
    , m_font(Theme::roleFont(Theme::Role::Body))
{
    setHorizontalHeader(new ResultHeader(this));
}

void ResultTable::paintEvent(QPaintEvent *e)
{
    QTableView::paintEvent(e);
    if (model() && model()->rowCount() == 0) {
        QPainter p(viewport());
        p.setFont(m_font);
        p.setPen(QColor(Theme::textDim));
        p.drawText(viewport()->rect(), Qt::AlignCenter, QStringLiteral("No sounds match."));
    }
}
