#include "screencapturemanager.h"
#include <QDateTime>
ScreenCaptureManager::ScreenCaptureManager(QObject *parent)
    : QThread{parent}
{}

void ScreenCaptureManager::init()
{
    connect(&mCapTureTimer,&QTimer::timeout,this,&ScreenCaptureManager::capTureTimerSlot);
}

ScreenCaptureManager &ScreenCaptureManager::Instance()
{
    static ScreenCaptureManager mScreenCaptureManager;
    return mScreenCaptureManager;
}

void ScreenCaptureManager::run()
{
    ScreenCaptureCore::ScreenData data;
    //std::vector<uint8_t>data;
    QDateTime start = QDateTime::currentDateTime();
    if(capture.CaptureToMemory(data,false,false) == ScreenCaptureCore::ErrorCode::Success)
    {
        QDateTime end = QDateTime::currentDateTime();
        qDebug()<<"cost"<<start.msecsTo(end);
        emit capturedScreen(data);
    }
}

void ScreenCaptureManager::startCapture()
{
    mCapTureTimer.start(30);
}

void ScreenCaptureManager::stopCapture()
{
    mCapTureTimer.stop();
}

void ScreenCaptureManager::capTureTimerSlot()
{
    start();
}
