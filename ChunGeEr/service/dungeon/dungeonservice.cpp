#include "dungeonservice.h"
#include "../../keyboardlistener.h"
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
}

void ServerDungeonService::stopService()
{
    Keyboardlistener::Instance().stopListen();
}

void ClientDungeonService::run()
{

}

void ClientDungeonService::clientHandleRecMsg(const json &data)
{

}
