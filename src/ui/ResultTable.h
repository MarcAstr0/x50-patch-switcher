#pragma once

#include <QFont>
#include <QTableView>

// The result list (DESIGN 7.7): a table whose delegate paints the rows and whose header is
// a ResultHeader. Paints "No sounds match." over the empty viewport.
class ResultTable : public QTableView
{
    Q_OBJECT
public:
    explicit ResultTable(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QFont m_font;
};
