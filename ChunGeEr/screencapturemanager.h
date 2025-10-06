#ifndef SCREENCAPTUREMANAGER_H
#define SCREENCAPTUREMANAGER_H
#include "ScreenCapture.h"
#include <QObject>
#include <QThread>
#include <QTimer>


class ScreenCaptureManager : public QThread
{
    Q_OBJECT
public:
    explicit ScreenCaptureManager(QObject *parent = nullptr);

    void init();

    static ScreenCaptureManager &Instance();

    void run();

    void startCapture();
    void stopCapture();
signals:
    void capturedScreen(ScreenCaptureCore::ScreenData data);
public slots:
    void capTureTimerSlot();
private:
    QTimer mCapTureTimer;
    ScreenCaptureCore::ScreenCapture capture;
};

#endif // SCREENCAPTUREMANAGER_H
