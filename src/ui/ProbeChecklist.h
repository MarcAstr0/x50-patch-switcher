#pragma once

#include <memory>
#include <vector>

#include <QString>
#include <QTextDocument>
#include <QVector>
#include <QWidget>

// The probe checklist of the settings dialog (DESIGN 8.1): a list of lines, each a painted
// glyph (disc, square, diamond, hollow circle: a different shape per state, never a font
// character) and a message, optionally followed by a fix hint with a coloured left rule.
// Paints itself from Theme; QSS sets only the font family. No timers, no animation: a repaint
// happens only when the rows or the width change.
class ProbeChecklist : public QWidget
{
    Q_OBJECT
public:
    enum class State { Pass, Fail, Unknown, Running };
    struct Row {
        State state = State::Pass;
        QString text;
        QString hint;  // empty: none. Text between double quotes is drawn in the strong weight.
        bool operator==(const Row &o) const { return state == o.state && text == o.text && hint == o.hint; }
    };

    explicit ProbeChecklist(QWidget *parent = nullptr);

    // Replaces every row. An empty list shows the "Run Check X50" placeholder.
    void setRows(const QVector<Row> &rows);
    const QVector<Row> &rows() const { return m_rows; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override;

    // Placeholder text (DESIGN 8.1 "Disabled-until-first-run state").
    static QString placeholderText();
    // The geometry of one row for the tests: top y and total height (message + hint block).
    struct RowGeometry { int top = 0; int height = 0; };
    RowGeometry rowGeometry(int index) const;

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void relayout();
    int layoutFor(int width, bool apply) const;
    void buildHintDocuments(int width) const;

    QVector<Row> m_rows;
    // Layout cache, rebuilt on setRows and on a width change, never in paintEvent.
    mutable std::vector<std::unique_ptr<QTextDocument>> m_hints;  // one per row; null when no hint
    mutable QVector<int> m_tops, m_heights, m_hintHeights;
    mutable int m_layoutWidth = -1;
    mutable int m_totalHeight = 0;
};
