#ifndef WSMANAGER_H
#define WSMANAGER_H

#include <QObject>

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
signals:

private:

};

#endif // WSMANAGER_H
