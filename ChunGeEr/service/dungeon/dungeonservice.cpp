#include "dungeonservice.h"
#include "../../keyboardlistener.h"
#include "screencapturemanager.h"
#include <QDebug>
ServerDungeonService::ServerDungeonService(QObject *parent)
    : BaseService{parent}
{

}

void ServerDungeonService::run()
{

}

void ServerDungeonService::startService()
{
    Keyboardlistener::Instance().startListen();
    ScreenCaptureManager::Instance().startCapture();
}

void ServerDungeonService::stopService()
{
    Keyboardlistener::Instance().stopListen();
}

void ServerDungeonService::handlePressEvent(int vkCode)
{
    qDebug()<<"handlePressEvent"<<vkCode;
}

ClientDungeonService::ClientDungeonService(QObject *parent) : BaseService{parent}
{

}

void ClientDungeonService::run()
{

}

void ClientDungeonService::clientHandleRecMsg(const json &data)
{

}
