#include "screensharewidget.h"
#include "ui_screensharewidget.h"
#include "../screenshare.h"
ScreenShareWidget::ScreenShareWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ScreenShareWidget)
{
    ui->setupUi(this);
    connect(&ScreenShare::Instance(),&ScreenShare::showScreen,this,&ScreenShareWidget::screenShowSlot,Qt::QueuedConnection);
}

ScreenShareWidget::~ScreenShareWidget()
{
    delete ui;
}

void ScreenShareWidget::screenShowSlot(QImage pic)
{
    ui->label->setPixmap(QPixmap::fromImage(pic));
}
