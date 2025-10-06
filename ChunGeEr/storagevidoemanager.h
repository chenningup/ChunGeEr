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
}
#include "screencapturemanager.h"
using namespace ScreenCaptureCore;
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
    void receiveCaptureScreen(ScreenCaptureCore::ScreenData data);
private:
    QSemaphore mScreenSem;
    QMutex mScreenMutex;
    QList<ScreenData>mScreenList;
    bool isSaving;

    AVFormatContext* m_formatContext = nullptr;
    AVCodecContext* m_codecContext = nullptr;
    AVStream* m_stream = nullptr;
    SwsContext* m_swsContext = nullptr;
};


#endif // STORAGEVIDOEMANAGER_H
