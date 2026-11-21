#include "ui/SlotCell.h"

#include "ui/Glyphs.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFocusEvent>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QPixmap>

namespace SlotMime {
QString slot() { return QStringLiteral("application/x-x50-slot"); }
QString sound() { return QStringLiteral("application/x-x50-sound"); }
} // namespace SlotMime

namespace {
constexpr int kPad = 8, kDigitBox = 24, kDigitGap = 8, kNameLine = 20, kSmallLine = 17;
}

SlotCell::SlotCell(int index, QWidget *parent)
    : QWidget(parent)
    , m_index(index)
    , m_digit(QString::number(index + 1))
    , m_fontName(Theme::roleFont(Theme::Role::BodyStrong))
    , m_fontSmall(Theme::roleFont(Theme::Role::Small))
    , m_fontLabel(Theme::roleFont(Theme::Role::Label))
    , m_fontDigit(Theme::roleFont(Theme::Role::Address))
{
    setObjectName(QStringLiteral("slotCell"));
    setFocusPolicy(Qt::StrongFocus);
    setAcceptDrops(true);
    setAttribute(Qt::WA_OpaquePaintEvent, false);
}

void SlotCell::setContent(bool empty, const QString &line1, const QString &line2)
{
    const QString l1 = empty ? QString() : line1, l2 = empty ? QString() : line2;
    if (empty == m_empty && l1 == m_line1 && l2 == m_line2)
        return;
    m_empty = empty;
    m_line1 = l1;
    m_line2 = l2;
    setToolTip(empty ? QString() : (l2.isEmpty() ? l1 : l1 + QLatin1Char('\n') + l2));
    relayout();
    update();
}

void SlotCell::setSelected(bool on)
{
    if (m_selected != on) {
        m_selected = on;
        update();
    }
}
void SlotCell::setCurrent(bool on)
{
    if (m_current != on) {
        m_current = on;
        update();
    }
}
void SlotCell::setDropTarget(bool on)
{
    if (m_drop != on) {
        m_drop = on;
        update();
    }
}

QRect SlotCell::textArea() const
{
    const int left = kPad + kDigitBox + kDigitGap;
    return QRect(left, 0, qMax(0, width() - left - kPad), height());
}

void SlotCell::relayout()
{
    const int w = textArea().width();
    m_elided1 = QFontMetrics(m_fontName).elidedText(m_line1, Qt::ElideRight, w);
    m_elided2 = QFontMetrics(m_fontSmall).elidedText(m_line2, Qt::ElideRight, w);
    if (m_edit)
        m_edit->setGeometry(textArea().left(), (height() - 28) / 2, textArea().width(), 28);
}

void SlotCell::resizeEvent(QResizeEvent *)
{
    relayout();
}

void SlotCell::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const qreal radius = Theme::radiusControl;
    QRectF box(rect());

    QColor fill(Theme::panelRaised);
    QColor edge(Theme::border);
    qreal bw = Theme::borderControl;
    Qt::PenStyle style = Qt::SolidLine;
    if (m_drop) {
        fill = QColor(Theme::selRow);
        edge = QColor(Theme::cyan);
        bw = Theme::borderFocus;
        style = Qt::DashLine;
    } else if (m_selected) {
        edge = QColor(Theme::cyan);
        bw = Theme::borderFocus;
    } else if (m_current) {
        edge = QColor(Theme::cyan);
    } else if (m_empty) {
        style = Qt::DashLine;
    }
    box.adjust(bw / 2, bw / 2, -bw / 2, -bw / 2);
    p.setBrush(fill);
    QPen pen(edge, bw, style);
    p.setPen(pen);
    p.drawRoundedRect(box, radius, radius);

    // Slot digit in a rounded square.
    const QColor digitCol(m_empty ? Theme::disabledText : Theme::textDim);
    const QRectF digitBox(kPad, (height() - kDigitBox) / 2.0, kDigitBox, kDigitBox);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(digitCol, 1));
    p.drawRoundedRect(digitBox.adjusted(0.5, 0.5, -0.5, -0.5), Theme::radiusPill, Theme::radiusPill);
    p.setFont(m_fontDigit);
    p.setPen(m_empty ? digitCol : QColor(Theme::text));
    p.drawText(digitBox, Qt::AlignCenter | Qt::TextSingleLine, m_digit);

    const QRect ta = textArea();
    if (m_empty) {
        p.setFont(m_fontLabel);
        p.setPen(QColor(Theme::disabledText));
        p.drawText(ta, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, QStringLiteral("EMPTY"));
    } else {
        const int top = (height() - kNameLine - kSmallLine) / 2;
        if (!m_edit || !m_edit->isVisible()) {
            p.setFont(m_fontName);
            p.setPen(QColor(Theme::text));
            p.drawText(QRect(ta.left(), top, ta.width(), kNameLine), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, m_elided1);
        }
        p.setFont(m_fontSmall);
        p.setPen(QColor(Theme::textDim));
        p.drawText(QRect(ta.left(), top + kNameLine, ta.width(), kSmallLine), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, m_elided2);
    }

    if (m_current)
        Glyphs::triangleRight(p, QRectF(width() - 6 - 8, 4, 6, 8), QColor(Theme::cyan));

    if (hasFocus()) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(Theme::cyan), Theme::borderFocus));
        p.drawRoundedRect(QRectF(rect()).adjusted(3, 3, -3, -3), radius - 2, radius - 2);
    }
}

void SlotCell::focusInEvent(QFocusEvent *e)
{
    QWidget::focusInEvent(e);
    update();
}
void SlotCell::focusOutEvent(QFocusEvent *e)
{
    QWidget::focusOutEvent(e);
    update();
}

void SlotCell::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        m_pressed = true;
        m_pressPos = e->pos();
        setFocus(Qt::MouseFocusReason);
        emit clicked(m_index);
    }
    e->accept();
}

void SlotCell::mouseMoveEvent(QMouseEvent *e)
{
    if (!m_pressed || !(e->buttons() & Qt::LeftButton) || m_empty)
        return;
    if ((e->pos() - m_pressPos).manhattanLength() < QApplication::startDragDistance())
        return;
    m_pressed = false;
    auto *mime = new QMimeData;
    mime->setData(SlotMime::slot(), QByteArray::number(m_index));
    auto *drag = new QDrag(this);
    drag->setMimeData(mime);
    drag->setPixmap(grab());
    drag->setHotSpot(e->pos());
    drag->exec(Qt::MoveAction);
}

void SlotCell::mouseDoubleClickEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton)
        emit editRequested(m_index);
}

void SlotCell::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Delete && e->modifiers() == Qt::NoModifier) {
        emit clearRequested(m_index);
        return;
    }
    if (e->key() == Qt::Key_F2 && e->modifiers() == Qt::NoModifier) {
        emit editRequested(m_index);
        return;
    }
    QWidget::keyPressEvent(e);
}

void SlotCell::contextMenuEvent(QContextMenuEvent *e)
{
    emit contextMenuRequested(m_index, e->globalPos());
}

bool SlotCell::accepts(const QMimeData *mime)
{
    return mime && (mime->hasFormat(SlotMime::slot()) || mime->hasFormat(SlotMime::sound()));
}

void SlotCell::dragEnterEvent(QDragEnterEvent *e)
{
    if (!accepts(e->mimeData()))
        return;
    setDropTarget(true);
    e->setDropAction(e->mimeData()->hasFormat(SlotMime::slot()) ? Qt::MoveAction : Qt::CopyAction);
    e->accept();
}

void SlotCell::dragMoveEvent(QDragMoveEvent *e)
{
    if (accepts(e->mimeData()))
        e->accept();
}

void SlotCell::dragLeaveEvent(QDragLeaveEvent *e)
{
    setDropTarget(false);
    e->accept();
}

void SlotCell::dropEvent(QDropEvent *e)
{
    setDropTarget(false);
    const QMimeData *mime = e->mimeData();
    if (!accepts(mime))
        return;
    if (mime->hasFormat(SlotMime::slot())) {
        bool ok = false;
        const int from = QString::fromLatin1(mime->data(SlotMime::slot())).toInt(&ok);
        e->setDropAction(Qt::MoveAction);
        e->accept();
        if (ok)
            emit slotDropped(from, m_index);
    } else {
        e->setDropAction(Qt::CopyAction);
        e->accept();
        emit soundDropped(m_index, QString::fromUtf8(mime->data(SlotMime::sound())));
    }
}

bool SlotCell::isEditing() const
{
    return m_edit && m_edit->isVisible();
}

void SlotCell::beginEdit(const QString &text)
{
    if (!m_edit) {
        m_edit = new QLineEdit(this);
        m_edit->setObjectName(QStringLiteral("slotLabelEdit"));
        m_edit->setFont(m_fontName);
        m_edit->setStyleSheet(QStringLiteral(
            "QLineEdit#slotLabelEdit { min-height: 26px; max-height: 26px; padding: 0 6px; background: #101417; "
            "color: #F2F5F7; border: 1px solid #5DB0D0; border-radius: 4px; }"));
        connect(m_edit, &QLineEdit::returnPressed, this, [this]() { finishEdit(true); });
        connect(m_edit, &QLineEdit::editingFinished, this, [this]() { finishEdit(true); });
        m_edit->installEventFilter(this);
        relayout();
    }
    m_edit->setText(text);
    m_edit->selectAll();
    m_edit->show();
    m_edit->setFocus(Qt::OtherFocusReason);
    update();
}

void SlotCell::finishEdit(bool commit)
{
    if (!m_edit || !m_edit->isVisible())
        return;
    const QString text = m_edit->text().trimmed();
    m_edit->hide();
    setFocus(Qt::OtherFocusReason);
    update();
    if (commit)
        emit labelCommitted(m_index, text);
}

bool SlotCell::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_edit && event->type() == QEvent::KeyPress && static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape) {
        finishEdit(false);
        return true;
    }
    return QWidget::eventFilter(watched, event);
}
