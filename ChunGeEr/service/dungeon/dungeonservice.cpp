#include "dungeonservice.h"
#include "../../keyboardlistener.h"
#include "screencapturemanager.h"
#include "../../wsmanager.h"
#include <QDebug>
#include "../../mousekeyboardmanager.h"
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
}

void ServerDungeonService::handlePressEvent(int vkCode)
{
    qDebug()<<"handlePressEvent"<<vkCode;
    switch(vkCode)
    {
    case 112:
    {
        json cmd ;
        cmd["cmd"] = "PickUp";
        WsManager::Instance().sendMsgToClient(cmd.dump());
    }
    break;
    case 113:
    {
        json cmd ;
        cmd["cmd"] = "FollowLeader";
        WsManager::Instance().sendMsgToClient(cmd.dump());
    }
    break;
    case 114:
    {
        json cmd ;
        cmd["cmd"] = "UseSkill";
        WsManager::Instance().sendMsgToClient(cmd.dump());
    }
    break;
    case 115:
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
        taskSem.acquire();
        QString task;
        taskMutex.lock();
        if(!tasks.isEmpty())
        {
            task = tasks[0];
            tasks.pop_front();
        }
        taskMutex.unlock();
        if(task == "PickUp")
        {
            MouseKeyboardManager::Instance().mouseMoveDirect(200,10);
            MouseKeyboardManager::Instance().mousePress(MOUSE_LEFT);
            MouseKeyboardManager::Instance().mouseRelease(MOUSE_LEFT);
            MouseKeyboardManager::Instance().keyPress(192);
            MouseKeyboardManager::Instance().keyRelease(192);

            MouseKeyboardManager::Instance().mouseMoveDirect(1500,10);
            MouseKeyboardManager::Instance().mousePress(MOUSE_LEFT);
            MouseKeyboardManager::Instance().mouseRelease(MOUSE_LEFT);
            MouseKeyboardManager::Instance().keyPress(192);
            MouseKeyboardManager::Instance().keyRelease(192);
        }
        if(task == "FollowLeader")
        {
            MouseKeyboardManager::Instance().mouseMoveDirect(200,10);
            MouseKeyboardManager::Instance().mousePress(MOUSE_LEFT);
            MouseKeyboardManager::Instance().mouseRelease(MOUSE_LEFT);

            MouseKeyboardManager::Instance().mouseMoveDirect(42,192);
            MouseKeyboardManager::Instance().mousePress(MOUSE_RIGHT);
            MouseKeyboardManager::Instance().mouseRelease(MOUSE_RIGHT);

            MouseKeyboardManager::Instance().mouseMoveDirect(42 + 61,192 + 55);
            MouseKeyboardManager::Instance().mousePress(MOUSE_LEFT);
            MouseKeyboardManager::Instance().mouseRelease(MOUSE_LEFT);


            MouseKeyboardManager::Instance().mouseMoveDirect(1500,10);
            MouseKeyboardManager::Instance().mousePress(MOUSE_LEFT);
            MouseKeyboardManager::Instance().mouseRelease(MOUSE_LEFT);

            MouseKeyboardManager::Instance().mouseMoveDirect(932,192);
            MouseKeyboardManager::Instance().mousePress(MOUSE_RIGHT);
            MouseKeyboardManager::Instance().mouseRelease(MOUSE_RIGHT);

            MouseKeyboardManager::Instance().mouseMoveDirect(932 + 61,192 + 55);
            MouseKeyboardManager::Instance().mousePress(MOUSE_LEFT);
            MouseKeyboardManager::Instance().mouseRelease(MOUSE_LEFT);
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
    taskSem.release();
}

void ClientDungeonService::clientHandleRecMsg(const json &data)
{
    if(data.contains("cmd"))
    {
        std::string cmd = data["cmd"].get<std::string>();
        taskMutex.lock();
        tasks.push_back(QString::fromStdString(cmd));
        taskMutex.unlock();
        taskSem.release();
        return;
    }
}
