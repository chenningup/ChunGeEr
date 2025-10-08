#ifndef STORAGEVIDOEMANAGER_H
#define STORAGEVIDOEMANAGER_H

#include <QObject>
#include <QThread>
#include <QMutex>
#include <QSemaphore>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>
#include <libavutil/hwcontext.h>
}
#include "screencapturemanager.h"
class StorageVidoeManager : public QThread
{
    Q_OBJECT
public:
    explicit StorageVidoeManager(QObject *parent = nullptr);

    static StorageVidoeManager&Instance();

    void init();

    void run();

    void stopSaveVideo();
signals:

public slots:
    void receiveCaptureScreen(ScreenCaptureManager::ScreenData data);
private:
    QSemaphore mScreenSem;
    QMutex mScreenMutex;
    QList<ScreenCaptureManager::ScreenData>mScreenList;
    bool isSaving;

    AVFormatContext* m_formatContext = nullptr;
    AVCodecContext* m_codecContext = nullptr;
    AVStream* m_stream = nullptr;
    SwsContext* m_swsContext = nullptr;
    AVBufferRef* hw_device_ctx = nullptr;
};


#endif // STORAGEVIDOEMANAGER_H
