#ifndef BASESERVICE_H
#define BASESERVICE_H

#include <QThread>
#include "screencapturemanager.h"
#include "../wsmanager.h"
class BaseService : public QThread
{
    Q_OBJECT
public:
    explicit BaseService(QObject *parent = nullptr);

    virtual void run();

    virtual void clientHandleRecMsg(const json &data);

    virtual void handlePressEvent(int vkCode);
signals:

public slots:
    void receiveCaptureScreen(ScreenCaptureManager::ScreenData data);
    void clientRecMegSlot(const std::string&msg);
    void keyPressEventSlot(int vkCode);
public:
    std::shared_ptr<std::vector<uint8_t>> curPic;
};

#endif // BASESERVICE_H
