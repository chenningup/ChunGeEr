#ifndef WSMANAGER_H
#define WSMANAGER_H

#include <QObject>
#include "nlohmann/json.hpp"
using json =  nlohmann::json;
//{
//    "cmd": "KeyboardSync",
//    "data": {
//        "key": 80
//    }
//}
class WsManager : public QObject
{
    Q_OBJECT
public:
    explicit WsManager(QObject *parent = nullptr);

    static WsManager&Instance();

    void init();

    void startServer();
    void stopServer();

    void startClient(const QString &url);
    void stopClient();

    void sendMsgToClient(const std::string &msg);

    void sendMsgToServer(const std::string &msg);
signals:
    void clientConnectToServer();
    void clientDisConnectToServer();
    void ServerRecClientConnect(QString ip);
    void ServerRecClientDisConnect(QString ip);
    void clientRecMeg(const json &msg);
private:

};

#endif // WSMANAGER_H
