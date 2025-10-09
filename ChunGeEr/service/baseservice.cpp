#include "baseservice.h"

BaseService::BaseService(QObject *parent)
    : QThread{parent}
{
    connect(&ScreenCaptureManager::Instance(),&ScreenCaptureManager::capturedScreen,this,&BaseService::receiveCaptureScreen,Qt::QueuedConnection);
}

void BaseService::run()
{

}

void BaseService::receiveCaptureScreen(ScreenCaptureManager::ScreenData data)
{
    curPic = data.data;
}
