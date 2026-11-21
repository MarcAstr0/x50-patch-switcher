#pragma once

#include <QFont>
#include <QPoint>
#include <QString>
#include <QWidget>

class QLineEdit;
class QMimeData;

namespace SlotMime {
// Drag payloads of the set editor: a slot being reordered (its 0-based index as text) and
// a sound dragged from the result list (SoundAddress::format() as text).
QString slot();
QString sound();
} // namespace SlotMime

// One cell of the set editor grid (DESIGN 7.9): painted from Theme, no QSS. It shows what
// it is told and reports what the user did; it knows nothing about Library or MIDI.
// States: normal, empty (dashed), selected (2 px cyan), drop (while a drag is over it) and
// `current` (a 6 x 8 corner triangle only: the sound the X50 is on).
// Texts are elided when set or resized, never in paintEvent. Repaints only on a state change.
class SlotCell : public QWidget
{
    Q_OBJECT
public:
    explicit SlotCell(int index, QWidget *parent = nullptr);

    int index() const { return m_index; }  // 0-based; the digit shown is index + 1
    // line1/line2 are the full texts; empty cells ignore them.
    void setContent(bool empty, const QString &line1, const QString &line2);
    void setSelected(bool on);
    void setCurrent(bool on);
    void setDropTarget(bool on);

    bool isEmpty() const { return m_empty; }
    bool isSelected() const { return m_selected; }
    bool isCurrent() const { return m_current; }
    bool isDropTarget() const { return m_drop; }
    QString line1() const { return m_line1; }
    QString line2() const { return m_line2; }

    // Drag support, factored for tests. The payload is the slot index as text; null for an empty cell.
    // The caller owns the result.
    QMimeData *makeDragMime() const;
    // True when a left press happened and `pos` is far enough from it to start a drag (never for an empty cell).
    bool dragWanted(const QPoint &pos) const;

    // Inline label editor (DESIGN 7.9: a field in `field` style).
    void beginEdit(const QString &text);
    bool isEditing() const;
    QLineEdit *editor() const { return m_edit; }

signals:
    void clicked(int index);
    void editRequested(int index);                    // double-click or F2
    void clearRequested(int index);                   // Del
    void contextMenuRequested(int index, const QPoint &globalPos);
    void labelCommitted(int index, const QString &text);
    void slotDropped(int from, int to);               // another cell was dropped here
    void soundDropped(int to, const QString &address);  // a result row was dropped here

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    static bool accepts(const QMimeData *mime);
    QRect textArea() const;
    void relayout();
    void finishEdit(bool commit);

    int m_index;
    bool m_empty = true, m_selected = false, m_current = false, m_drop = false;
    QString m_line1, m_line2, m_elided1, m_elided2;
    QString m_digit;
    QFont m_fontName, m_fontSmall, m_fontLabel, m_fontDigit;
    QPoint m_pressPos;
    bool m_pressed = false;
    QLineEdit *m_edit = nullptr;
};
