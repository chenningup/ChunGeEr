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
    switch(vkCode)
    {
    case 192:
    {
        json cmd ;
        cmd["cmd"] = "KeyboardSync";
        json data;
        data["key"] = vkCode;
        cmd["data"] = data;
        WsManager::Instance().sendMsgToClient(cmd.dump());
    }
    break;
    case 70:
    {
        json cmd ;
        cmd["cmd"] = "FollowLeader";
        WsManager::Instance().sendMsgToClient(cmd.dump());
    }
    break;
    case 71:
    {
        json cmd ;
        cmd["cmd"] = "UseSkill";
        WsManager::Instance().sendMsgToClient(cmd.dump());
    }
    break;
    case 72:
    {
        json cmd ;
        cmd["cmd"] = "FollowSup";
        WsManager::Instance().sendMsgToClient(cmd.dump());
    }
    break;


    }

}


ClientDungeonService::ClientDungeonService(QObject *parent) : BaseService{parent}
{

}

ClientDungeonService::~ClientDungeonService()
{
    toRun = false;
}

void ClientDungeonService::run()
{
    while(toRun)
    {
        if(!tasks.isEmpty())
        {
            QString task = tasks[0];
            if(task == "FollowLeader")
            {
                qDebug()<<"FollowLeader";
            }
            if(task == "UseSkill")
            {
                qDebug()<<"UseSkill";
            }
            if(task == "FollowSup")
            {
                qDebug()<<"FollowSup";
            }
            tasks.pop_front();
        }
        QThread::msleep(300);
    }
}

void ClientDungeonService::startService()
{
    toRun = true;
    start();
}

void ClientDungeonService::stopService()
{
    toRun = false;
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
            return;
        }
        if(cmd == "FollowLeader")
        {
            tasks.push_back("FollowLeader");
            return;
        }
        if(cmd == "UseSkill")
        {
            tasks.push_back("UseSkill");
            return;
        }
        if(cmd == "FollowSup")
        {
            tasks.push_back("FollowSup");
            return;
        }
    }
}
