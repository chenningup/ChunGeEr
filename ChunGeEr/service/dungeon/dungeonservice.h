#ifndef DUNGEONSERVICE_H
#define DUNGEONSERVICE_H

#include <QWidget>
#include "../baseservice.h"
class ServerDungeonService : public BaseService
{
    Q_OBJECT
public:
    explicit ServerDungeonService(QObject *parent = nullptr);

    void run();

    void startService();

    void stopService();

signals:

};

#endif // DUNGEONSERVICE_H
