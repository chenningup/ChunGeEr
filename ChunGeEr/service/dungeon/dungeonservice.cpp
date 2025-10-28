#include "dungeonservice.h"
#include "../../KeyboardListener/keyboardlistener.h"
#include "screencapturemanager.h"
#include "../../WsManager/wsmanager.h"
#include <QDebug>
#include "../../LeoControl//mousekeyboardmanager.h"
#include "../signalslotconnector.h"
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
    json cmd ;
    cmd["cmd"] = "StopService";
    json data;
    data["ServiceName"] = "DungeonService";
    data["DungeonName"] = 30;
    cmd["data"] = data;
    WsManager::Instance().sendMsgToClient(cmd.dump());
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
        emit SignalSlotConnector::Instance().log("start do task" + task);
        if(task == "PickUp")
        {
            qDebug()<<"PickUp";
            chooseLeftGame();
            QThread::msleep(500);
            MouseKeyboardManager::Instance().clickButton(192);
            QThread::msleep(500);
            chooseRightGame();
            QThread::msleep(500);
            MouseKeyboardManager::Instance().clickButton(192);
        }
        if(task == "FollowLeader")
        {
            qDebug()<<"FollowLeader";
            chooseLeftGame();
            MouseKeyboardManager::Instance().mouseMoveDirect(42,192);
            QThread::msleep(500);
            MouseKeyboardManager::Instance().mouseRightClick();
            QThread::msleep(500);
            MouseKeyboardManager::Instance().mouseMoveDirect(42 + 61,192 + 55);
            QThread::msleep(500);
            MouseKeyboardManager::Instance().mouseClick();

            chooseRightGame();

            MouseKeyboardManager::Instance().mouseMoveDirect(932,192);
            QThread::msleep(500);
            MouseKeyboardManager::Instance().mouseRightClick();
            QThread::msleep(500);
            MouseKeyboardManager::Instance().mouseMoveDirect(932 + 61,192 + 55);
            QThread::msleep(500);
            MouseKeyboardManager::Instance().mouseClick();
            qDebug()<<"FollowLeader";
        }
        if(task == "UseSkill")
        {
            qDebug()<<"UseSkill";
            chooseLeftGame();
            MouseKeyboardManager::Instance().keyPress('3');
            QThread::msleep(200);
            MouseKeyboardManager::Instance().mouseMoveDirect(700,400);
            QThread::msleep(500);
            MouseKeyboardManager::Instance().mouseClick();
            QThread::msleep(1000);
            MouseKeyboardManager::Instance().keyPress('4');
            QThread::msleep(500);
            MouseKeyboardManager::Instance().mouseClick();

            chooseRightGame();

            MouseKeyboardManager::Instance().keyPress('3');
            QThread::msleep(200);
            MouseKeyboardManager::Instance().mouseMoveDirect(1920 - 1400 + 700 ,400);
            QThread::msleep(500);
            MouseKeyboardManager::Instance().mouseClick();
            QThread::msleep(1000);
            MouseKeyboardManager::Instance().keyPress('4');
            QThread::msleep(500);
            MouseKeyboardManager::Instance().mouseClick();
        }
        if(task == "FollowSup")
        {
            chooseLeftGame();
            MouseKeyboardManager::Instance().mouseMoveDirect(42 + 50,192);
            QThread::msleep(500);
            MouseKeyboardManager::Instance().mouseRightClick();
            QThread::msleep(500);
            MouseKeyboardManager::Instance().mouseMoveDirect(42 + 61,192 + 55 + 50);
            QThread::msleep(500);
            MouseKeyboardManager::Instance().mouseClick();

            chooseRightGame();

            MouseKeyboardManager::Instance().mouseMoveDirect(932 + 50,192);
            QThread::msleep(500);
            MouseKeyboardManager::Instance().mouseRightClick();
            QThread::msleep(500);
            MouseKeyboardManager::Instance().mouseMoveDirect(932 + 61,192 + 55 + 50);
            QThread::msleep(500);
            MouseKeyboardManager::Instance().mouseClick();
            qDebug()<<"FollowSup";
        }
        emit SignalSlotConnector::Instance().log("finish do task" + task);
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
