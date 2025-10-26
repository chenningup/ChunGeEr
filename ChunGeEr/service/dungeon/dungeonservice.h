#ifndef DUNGEONSERVICE_H
#define DUNGEONSERVICE_H

#include <QWidget>
#include "../baseservice.h"
#include <QMutex>
#include <QSemaphore>
class ServerDungeonService : public BaseService
{
    Q_OBJECT
public:
    explicit ServerDungeonService(QObject *parent = nullptr);

    void run();

    void startService();

    void stopService();

    virtual void handlePressEvent(int vkCode);
signals:

};


class ClientDungeonService : public BaseService
{
    Q_OBJECT
public:
    explicit ClientDungeonService(QObject *parent = nullptr);

    ~ClientDungeonService();

    void run();

    void startService();

    void stopService();

    void clientHandleRecMsg(const json &data);

signals:

private:
    QMutex taskMutex;
    QSemaphore taskSem;
};







#endif // DUNGEONSERVICE_H
