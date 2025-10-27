#include "baseservice.h"
#include "../KeyboardListener/keyboardlistener.h"
#include "../LeoControl/mousekeyboardmanager.h"
BaseService::BaseService(QObject *parent)
    : QThread{parent},toRun(false)
{
    connect(&ScreenCaptureManager::Instance(),&ScreenCaptureManager::capturedScreen,this,&BaseService::receiveCaptureScreen,Qt::QueuedConnection);
    connect(&WsManager::Instance(),&WsManager::clientRecMeg,this,&BaseService::clientRecMegSlot,Qt::QueuedConnection);
    connect(&Keyboardlistener::Instance(),&Keyboardlistener::keyPressEvent,this,&BaseService::keyPressEventSlot,Qt::QueuedConnection);
}

void BaseService::run()
{

}

void BaseService::clientHandleRecMsg(const json &data)
{

}

void BaseService::handlePressEvent(int vkCode)
{

}

void BaseService::chooseLeftGame()
{
    MouseKeyboardManager::Instance().mouseMoveDirect(200,10);
    QThread::sleep(1);
    MouseKeyboardManager::Instance().mousePress(MOUSE_LEFT);
    QThread::msleep(200);
    MouseKeyboardManager::Instance().mouseRelease(MOUSE_LEFT);
}

void BaseService::chooseRightGame()
{
    MouseKeyboardManager::Instance().mouseMoveDirect(1500,10);
    QThread::sleep(1);
    MouseKeyboardManager::Instance().mousePress(MOUSE_LEFT);
    QThread::msleep(200);
    MouseKeyboardManager::Instance().mouseRelease(MOUSE_LEFT);
}

void BaseService::receiveCaptureScreen(ScreenCaptureManager::ScreenData data)
{
    picMutex.lock();
    curPic = data;
    picMutex.unlock();
}

void BaseService::clientRecMegSlot(const json &msg)
{
    clientHandleRecMsg(msg);
}

void BaseService::keyPressEventSlot(int vkCode)
{
    handlePressEvent(vkCode);
}
