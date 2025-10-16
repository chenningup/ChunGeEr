#ifndef CATCHMONSTERSSERVICE_H
#define CATCHMONSTERSSERVICE_H

#include "../baseservice.h"
class CatchMonstersService : public BaseService
{
    Q_OBJECT
public:
    explicit CatchMonstersService(QObject *parent = nullptr);

    void run();

    void startService();

    void stopService();

signals:

};

#endif // CATCHMONSTERSSERVICE_H
