#include "dungeonservice.h"
#include "../../keyboardlistener.h"
#include "screencapturemanager.h"
#include "../../wsmanager.h"
#include <QDebug>
//{
//    "cmd": "KeyboardSync",
//    "data": {
//        "key": 80
//    }
//}

//{
//    "cmd": "StartService",
//    "data": {
//        "name": "DungeonService"
//    }
//}


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
    json cmd ;
    cmd["cmd"] = "StartService";
    json data;
    data["ServiceName"] = "DungeonService";
    data["DungeonName"] = 30;
    cmd["data"] = data;
    WsManager::Instance().sendMsgToClient(cmd.dump());
}

void ServerDungeonService::stopService()
{
    Keyboardlistener::Instance().stopListen();
    ScreenCaptureManager::Instance().stopCapture();
}

void ServerDungeonService::handlePressEvent(int vkCode)
{
    qDebug()<<"handlePressEvent"<<vkCode;
    if(vkCode == 192)
    {
        json cmd ;
        cmd["cmd"] = "KeyboardSync";
        json data;
        data["key"] = vkCode;
        cmd["data"] = data;
        WsManager::Instance().sendMsgToClient(cmd.dump());
    }
}


ClientDungeonService::ClientDungeonService(QObject *parent) : BaseService{parent}
{

}

void ClientDungeonService::run()
{

}

void ClientDungeonService::startService()
{

}

void ClientDungeonService::stopService()
{

}

void ClientDungeonService::clientHandleRecMsg(const json &data)
{
    if(data.contains("cmd"))
    {
        std::string cmd = data["cmd"].get<std::string>();

        if(cmd == "KeyboardSync")
        {
            int key = data["data"]["key"].get<int>();
            qDebug()<<"click key "<< key;
        }
    }
}
