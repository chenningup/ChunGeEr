#ifndef BASESERVICE_H
#define BASESERVICE_H

#include <QThread>
#include "screencapturemanager.h"
#include "../WsManager/wsmanager.h"
#include <QMutex>
class BaseService : public QThread
{
    Q_OBJECT
public:
    explicit BaseService(QObject *parent = nullptr);

    virtual void run();

    virtual void clientHandleRecMsg(const json &data);

    virtual void handlePressEvent(int vkCode);

    virtual void startService()=0;

    virtual void stopService()=0;

    void chooseLeftGame();

    void chooseRightGame();
signals:

public slots:
    void receiveCaptureScreen(ScreenCaptureManager::ScreenData data);
    void clientRecMegSlot(const json &msg);
    void keyPressEventSlot(int vkCode);
public:
    ScreenCaptureManager::ScreenData curPic;
    //std::shared_ptr<std::vector<uint8_t>> curPic;
    bool toRun;
    QStringList tasks;
    QMutex picMutex;
};

#endif // BASESERVICE_H
