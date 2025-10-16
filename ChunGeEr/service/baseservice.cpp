#include "baseservice.h"
#include "../keyboardlistener.h"
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
