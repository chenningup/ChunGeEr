#ifndef SCREENSHAREWIDGET_H
#define SCREENSHAREWIDGET_H

#include <QWidget>
//#include "ui_screensharewidget.h"
namespace Ui {
class ScreenShareWidget;
}

class ScreenShareWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ScreenShareWidget(QWidget *parent = nullptr);
    ~ScreenShareWidget();

public slots:
    void screenShowSlot(QImage pic);
private:
    Ui::ScreenShareWidget *ui;
};

#endif // SCREENSHAREWIDGET_H
