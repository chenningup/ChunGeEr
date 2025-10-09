#ifndef SCREENCAPTUREMANAGER_H
#define SCREENCAPTUREMANAGER_H
//#include "ScreenCapture.h"
#include <QObject>
#include <QThread>
#include <QTimer>
#include "pch.h"
class ScreenCaptureManager : public QThread
{
    Q_OBJECT
public:
    struct ScreenData
    {
        ScreenData() {}
        D3D11_TEXTURE2D_DESC des;
        int RowPitch;
        std::shared_ptr<std::vector<uint8_t>> data;
    };
    explicit ScreenCaptureManager(QObject *parent = nullptr);

    ~ScreenCaptureManager();
    void init();

    static ScreenCaptureManager &Instance();

    void run();

    void startCapture();
    void stopCapture();
signals:
    void capturedScreen(ScreenData data);
public slots:
    void capTureTimerSlot();
private:
    bool isCapture;
};

#endif // SCREENCAPTUREMANAGER_H
