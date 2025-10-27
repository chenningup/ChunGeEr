#include "hv/EventLoop.h"
#include "hv/htime.h"
#include "hv/hssl.h"
#include "wsmanager.h"
#include <QDebug>
#include "hv/WebSocketClient.h"
#include "hv/WebSocketServer.h"
#include "../signalslotconnector.h"
static WebSocketService ws;
static websocket_server_t server;
static hv::WebSocketClient wsClient;
static QList<WebSocketChannelPtr>clientList;
WsManager::WsManager(QObject *parent)
    : QObject{parent}
{

}

WsManager &WsManager::Instance()
{
    static WsManager mWsManager;
    return mWsManager;
}

void WsManager::init()
{
    //memset(&server, 0, sizeof(server));
    server.port = 7777;
    server.ws = &ws;
    ws.onopen = [this](const WebSocketChannelPtr &channel, const HttpRequestPtr &req) {
        // 转发到类成员
        //qDebug()<<"connect";
        std::string ip = req->client_addr.ip;
        clientList.push_back(channel);
        emit ServerRecClientConnect(QString::fromStdString(ip));
    };

    ws.onmessage = [this](const WebSocketChannelPtr &channel, const std::string &msg) {
        if(msg.empty())
        {
            return;
        }
        json data = json::parse(msg);
        emit serverRecMeg(data);
        emit SignalSlotConnector::Instance().log("receive: "+ QString::fromStdString(msg));
    };

    ws.onclose = [this](const WebSocketChannelPtr &channel) {
        for (int i = 0; i < clientList.size(); ++i)
        {
            if(clientList[i].get() == channel.get())
            {
                emit ServerRecClientDisConnect("");
                clientList.removeAt(i);
                break;
            }
        }
    };
    // client 端的回调（通常 onopen/onmessage/onclose 的签名不同，按 hv 的定义写）
    wsClient.onopen = [this]()
    {
        emit clientConnectToServer();
    };

    wsClient.onmessage = [this](const std::string &msg)
    {
        if(msg.empty())
        {
            return;
        }
        json msgData= json::parse(msg);
        emit clientRecMeg(msgData);
        emit SignalSlotConnector::Instance().log("receive: "+ QString::fromStdString(msg));
        // json data = json::parse(msg);
        // qDebug()<<QString::fromStdString(msg);
    };

    wsClient.onclose = [this]()
    {
        emit clientDisConnectToServer();
    };

}



void WsManager::startServer()
{
    websocket_server_run(&server, 0);
}

void WsManager::stopServer()
{
    websocket_server_stop(&server);
}

void WsManager::startClient(const QString &url)
{
    reconn_setting_t reconn;
    reconn_setting_init(&reconn);
    reconn.min_delay = 1000;
    reconn.max_delay = 5000;
    wsClient.setReconnect(&reconn);
    wsClient.open(url.toStdString().data());
}

void WsManager::stopClient()
{
    wsClient.close();
}

void WsManager::sendMsgToClient(const std::string &msg)
{
    for (auto &client : clientList)
    {
        emit SignalSlotConnector::Instance().log("send: "+QString::fromStdString(msg));
        client->send(msg.c_str());
    }
}

void WsManager::sendMsgToServer(const std::string &msg)
{
    emit SignalSlotConnector::Instance().log("send: "+QString::fromStdString(msg));
    wsClient.send(msg.c_str());
}
