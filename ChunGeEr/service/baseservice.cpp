#include "baseservice.h"

BaseService::BaseService(QObject *parent)
    : QThread{parent}
{
    connect(&ScreenCaptureManager::Instance(),&ScreenCaptureManager::capturedScreen,this,&BaseService::receiveCaptureScreen,Qt::QueuedConnection);
    connect(&WsManager::Instance(),&WsManager::clientRecMeg,this,&BaseService::clientRecMegSlot,Qt::QueuedConnection);
}

void BaseService::run()
{

}

void BaseService::receiveCaptureScreen(ScreenCaptureManager::ScreenData data)
{
    curPic = data.data;
}

void BaseService::clientRecMegSlot(const std::string &msg)
{
    json  data = json::parse(msg);
    clientHandleRecMsg(data);
}
