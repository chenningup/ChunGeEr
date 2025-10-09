#ifndef BASESERVICE_H
#define BASESERVICE_H

#include <QThread>
#include "screencapturemanager.h"
class BaseService : public QThread
{
    Q_OBJECT
public:
    explicit BaseService(QObject *parent = nullptr);

    virtual void run();

signals:

public slots:
    void receiveCaptureScreen(ScreenCaptureManager::ScreenData data);
public:
    std::shared_ptr<std::vector<uint8_t>> curPic;
};

#endif // BASESERVICE_H
