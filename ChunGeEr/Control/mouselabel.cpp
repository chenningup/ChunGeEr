#include "mouselabel.h"
#include "../wsmanager.h"
#include "../screenshare.h"
#include"../keyboardlistener.h"
#include <QGuiApplication>
extern bool isMaster;
MouseLabel::MouseLabel(QWidget *parent)
    : QLabel(parent)
{
    this->setMouseTracking(true);
    connect(&Keyboardlistener::Instance(),&Keyboardlistener::keyPressEvent,this,&MouseLabel::keyPressEventSlot,Qt::QueuedConnection);
    connect(&Keyboardlistener::Instance(),&Keyboardlistener::keyReleaseEvent,this,&MouseLabel::keyReleaseEventSlot,Qt::QueuedConnection);
}

void MouseLabel::wheelEvent(QWheelEvent *event)
{
    // 优先检查像素增量（适用于高精度触摸板）
    QPoint pixelDelta = event->pixelDelta();
    if (!pixelDelta.isNull())
    {
        // 使用像素增量
        int scrollPixels = pixelDelta.y();
        if(isMaster && ScreenShare::Instance().isRunning())
        {
            json cmd ;
            cmd["cmd"] = "MousewheelSync";
            json data;
            data["dis"] = scrollPixels;
            cmd["data"] = data;
            WsManager::Instance().sendMsgToClient(cmd.dump());
        }
    }
    // 如果没有像素增量（如使用普通鼠标），则检查角度增量
    else {
        QPoint angleDelta = event->angleDelta();
        if (!angleDelta.isNull())
        {
            int scrollDegrees = angleDelta.y();
            if(isMaster && ScreenShare::Instance().isRunning())
            {
                json cmd ;
                cmd["cmd"] = "MousewheelSync";
                json data;
                data["dis"] = scrollDegrees;
                cmd["data"] = data;
                WsManager::Instance().sendMsgToClient(cmd.dump());
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
    //qDebug()<<"mouseMove" <<x <<y;
    if(isMaster && ScreenShare::Instance().isRunning())
    {
        json cmd ;
        cmd["cmd"] = "MouseMoveSync";
        json data;
        data["x"] = x;
        data["y"] = y;
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
        cmd["cmd"] = "MousePressSync";
        json data;
        data["x"] = x;
        data["y"] = y;
        data["type"] = buttonText.toStdString();
        cmd["data"] = data;
        WsManager::Instance().sendMsgToClient(cmd.dump());
    }
    QLabel::mousePressEvent(event);
}

void MouseLabel::mouseReleaseEvent(QMouseEvent *event)
{
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
        cmd["cmd"] = "MouseReleaseSync";
        json data;
        data["x"] = x;
        data["y"] = y;
        data["type"] = buttonText.toStdString();
        cmd["data"] = data;
        WsManager::Instance().sendMsgToClient(cmd.dump());
    }
    QLabel::mouseReleaseEvent(event);
}

void MouseLabel::keyPressEventSlot(int vkCode)
{
    if(isMaster  && ScreenShare::Instance().isRunning() && QGuiApplication::applicationState() == Qt::ApplicationActive)
    {
        json cmd ;
        cmd["cmd"] = "KeybordPressSync";
        json data;
        data["Key"] = vkCode;
        cmd["data"] = data;
        WsManager::Instance().sendMsgToClient(cmd.dump());
    }
}

void MouseLabel::keyReleaseEventSlot(int vkCode)
{
    if(isMaster  && ScreenShare::Instance().isRunning() && QGuiApplication::applicationState() == Qt::ApplicationActive)
    {
        json cmd ;
        cmd["cmd"] = "KeybordReleaseSync";
        json data;
        data["Key"] = vkCode;
        cmd["data"] = data;
        WsManager::Instance().sendMsgToClient(cmd.dump());
    }
}
