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
#include "../Commons/safepacket.h"
#include <QFile>
class StorageVidoeManager : public QThread
{
    Q_OBJECT
public:
    explicit StorageVidoeManager(QObject *parent = nullptr);

    static StorageVidoeManager&Instance();

    void init();

    void run();

    void startSaveVideo(const QString &filePath);

    void stopSaveVideo();
signals:

public slots:
    void receiveCaptureScreen(std::shared_ptr<SafePacket> data);
private:
    QSemaphore mScreenSem;
    QMutex mScreenMutex;
    QList<std::shared_ptr<SafePacket>>mScreenList;
    bool isSaving;

    AVCodecContext* m_codecContext = nullptr;
    AVFormatContext* m_formatContext = nullptr;
//    AVCodecContext* m_codecContext = nullptr;
    AVStream* m_stream = nullptr;
//    SwsContext* m_swsContext = nullptr;
//    AVBufferRef* hw_device_ctx = nullptr;
};


#endif // STORAGEVIDOEMANAGER_H
