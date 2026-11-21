#pragma once

#include <QFont>
#include <QStaticText>
#include <QString>
#include <QTableView>

// The result list (DESIGN 7.7): a table whose delegate paints the rows and whose header is
// a ResultHeader. Paints the empty-state text (DESIGN 7.7 "Favourites and Recent views")
// centred over the empty viewport: `body` 15 px `text-dim`, wrapped to the list width minus 48.
// The text is laid out once (QStaticText) when it or the width changes, never in paintEvent.
class ResultTable : public QTableView
{
    Q_OBJECT
public:
    explicit ResultTable(QWidget *parent = nullptr);

    // The text shown when the model has no rows. Defaults to "No sounds match."
    void setEmptyText(const QString &text);
    QString emptyText() const { return m_emptyText; }

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void layoutEmptyText();

    QFont m_font;
    QString m_emptyText;
    QStaticText m_static;
};
