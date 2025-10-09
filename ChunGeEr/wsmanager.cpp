#include "hv/EventLoop.h"
#include "hv/htime.h"
#include "hv/hssl.h"
#include "wsmanager.h"
#include <QDebug>
#include "hv/WebSocketClient.h"
#include "hv/WebSocketServer.h"
#include "nlohmann/json.hpp"
using json =  nlohmann::json;
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
    server.port = 8888;
    server.ws = &ws;
    ws.onopen = [this](const WebSocketChannelPtr &channel, const HttpRequestPtr &req) {
        // 转发到类成员
        clientList.push_back(channel);
    };

    ws.onmessage = [this](const WebSocketChannelPtr &channel, const std::string &msg) {
        if(msg.empty())
        {
            return;
        }
        json data = json::parse(msg);
        qDebug()<<QString::fromStdString(msg);
    };

    ws.onclose = [this](const WebSocketChannelPtr &channel) {
        for (int i = 0; i < clientList.size(); ++i)
        {
            if(clientList[i].get() == channel.get())
            {
                clientList.removeAt(i);
                break;
            }
        }
    };

    // client 端的回调（通常 onopen/onmessage/onclose 的签名不同，按 hv 的定义写）
    wsClient.onopen = [this]()
    {

    };

    wsClient.onmessage = [this](const std::string &msg)
    {
        if(msg.empty())
        {
            return;
        }
        json data = json::parse(msg);
        qDebug()<<QString::fromStdString(msg);
    };

    wsClient.onclose = [this]()
    {
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
    wsClient.open(url.toStdString().data());
}

void WsManager::stopClient()
{
    wsClient.close();
}
