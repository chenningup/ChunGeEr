#include "mouselabel.h"
#include "../wsmanager.h"
#include "../screenshare.h"
#include"../keyboardlistener.h"
extern bool isMaster;
MouseLabel::MouseLabel(QWidget *parent)
    : QLabel(parent)
{
    this->setMouseTracking(true);
    connect(&Keyboardlistener::Instance(),&Keyboardlistener::keyPressEvent,this,&MouseLabel::keyPressEventSlot,Qt::QueuedConnection);
}

void MouseLabel::wheelEvent(QWheelEvent *event)
{
    // 优先检查像素增量（适用于高精度触摸板）
    QPoint pixelDelta = event->pixelDelta();
    if (!pixelDelta.isNull()) {
        // 使用像素增量
        int scrollPixels = pixelDelta.y();
        if (scrollPixels > 0) {
            qDebug() << "向上滑动了（像素）:" << scrollPixels << "像素";
        } else if (scrollPixels < 0) {
            qDebug() << "向下滑动了（像素）:" << -scrollPixels << "像素";
        }
    }
    // 如果没有像素增量（如使用普通鼠标），则检查角度增量
    else {
        QPoint angleDelta = event->angleDelta();
        if (!angleDelta.isNull()) {
            int scrollDegrees = angleDelta.y();
            if (scrollDegrees > 0) {
                qDebug() << "向上滑动了（角度）:" << scrollDegrees << "度";
                // 对于常见的鼠标，可以将其转换为“格数”
                int numSteps = scrollDegrees / 120; // 每格通常为120度
                qDebug() << "相当于向上滑动了" << numSteps << "格";
            } else if (scrollDegrees < 0) {
                qDebug() << "向下滑动了（角度）:" << -scrollDegrees << "度";
                int numSteps = -scrollDegrees / 120;
                qDebug() << "相当于向下滑动了" << numSteps << "格";
            }
        }
    }

    // 标记事件已被处理，防止继续传递
    event->accept();//注释
    //QLabel::wheelEvent(event);
}


void MouseLabel::enterEvent(QEnterEvent *event)
{
    Q_UNUSED(event)
}

void MouseLabel::leaveEvent(QEvent *event)
{
    Q_UNUSED(event)
}

void MouseLabel::mouseMoveEvent(QMouseEvent *event)
{
    // 获取鼠标相对于当前QLabel的坐标
    QPoint pos = event->pos();
    int x = pos.x();
    int y = pos.y();

    if(isMaster && ScreenShare::Instance().isRunning())
    {
        json cmd ;
        cmd["cmd"] = "MouseMoveSync";
        json data;
        data["x"] = x;
        data["y"] = x;
        cmd["data"] = data;
        WsManager::Instance().sendMsgToClient(cmd.dump());
    }
}

void MouseLabel::mousePressEvent(QMouseEvent *event)
{
    // 获取点击位置的坐标
    QPoint pos = event->pos();
    int x = pos.x();
    int y = pos.y();

    // 判断按下的鼠标按键
    QString buttonText;
    if (event->button() == Qt::LeftButton) {
        buttonText = "left";
    } else if (event->button() == Qt::RightButton) {
        buttonText = "right";
    } else {
        buttonText = "other";
    }
    qDebug() << buttonText;
    if(isMaster && ScreenShare::Instance().isRunning())
    {
        json cmd ;
        cmd["cmd"] = "MouseClickSync";
        json data;
        data["x"] = x;
        data["y"] = y;
        data["type"] = buttonText.toStdString();
        cmd["data"] = data;
        WsManager::Instance().sendMsgToClient(cmd.dump());
    }
    QLabel::mousePressEvent(event);
}

void MouseLabel::keyPressEventSlot(int vkCode)
{
    if(isMaster  && ScreenShare::Instance().isRunning())
    {
        json cmd ;
        cmd["cmd"] = "KeybordSync";
        json data;
        data["Key"] = vkCode;
        cmd["data"] = data;
        WsManager::Instance().sendMsgToClient(cmd.dump());
    }
}
