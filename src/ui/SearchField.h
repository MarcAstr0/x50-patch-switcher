#pragma once

#include <QLineEdit>

// Search field with the painted magnifier (DESIGN 7.4). The clear cross is a line edit
// action added by the owner.
class SearchField : public QLineEdit
{
    Q_OBJECT
public:
    explicit SearchField(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
};
