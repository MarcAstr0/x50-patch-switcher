#include "ui/ResultTable.h"

#include "ui/ResultHeader.h"
#include "ui/Theme.h"

#include <QPainter>
#include <QTransform>

ResultTable::ResultTable(QWidget *parent)
    : QTableView(parent)
    , m_font(Theme::roleFont(Theme::Role::Body))
    , m_emptyText(QStringLiteral("No sounds match."))
{
    setHorizontalHeader(new ResultHeader(this));
    m_static.setTextFormat(Qt::PlainText);
    m_static.setPerformanceHint(QStaticText::AggressiveCaching);
    QTextOption opt(Qt::AlignHCenter);
    opt.setWrapMode(QTextOption::WordWrap);
    m_static.setTextOption(opt);
    layoutEmptyText();
}

void ResultTable::setEmptyText(const QString &text)
{
    if (text == m_emptyText)
        return;
    m_emptyText = text;
    layoutEmptyText();
    if (model() && model()->rowCount() == 0)
        viewport()->update();
}

void ResultTable::layoutEmptyText()
{
    m_static.setText(m_emptyText);
    m_static.setTextWidth(qMax(1, viewport()->width() - 48));
    m_static.prepare(QTransform(), m_font);
}

void ResultTable::resizeEvent(QResizeEvent *e)
{
    QTableView::resizeEvent(e);
    layoutEmptyText();
}

void ResultTable::paintEvent(QPaintEvent *e)
{
    QTableView::paintEvent(e);
    if (model() && model()->rowCount() == 0) {
        QPainter p(viewport());
        p.setFont(m_font);
        p.setPen(QColor(Theme::textDim));
        const QSizeF sz = m_static.size();
        p.drawStaticText(QPointF((viewport()->width() - m_static.textWidth()) / 2.0,
                                 (viewport()->height() - sz.height()) / 2.0), m_static);
    }
}
