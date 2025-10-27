#ifndef ENCODINGMANAGER_H
#define ENCODINGMANAGER_H

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
#include <libavutil/opt.h>
}
#include "screencapturemanager.h"
#include "../Commons/safepacket.h"
class EncodingManager : public QThread
{
    Q_OBJECT
public:
    explicit EncodingManager(QObject *parent = nullptr);

    static EncodingManager&Instance();

    void init();

    void run();

    void startEncodeing();

    void stopEncodeing();

    AVCodecContext*getAVCodecContext(){return m_codecContext;};
signals:
    void encodedAVPacket(std::shared_ptr<SafePacket> packet);
public slots:
    void receiveCaptureScreen(ScreenCaptureManager::ScreenData data);
private:
    QList<ScreenCaptureManager::ScreenData>mScreenList;
    QSemaphore mScreenSem;
    QMutex mScreenMutex;

    AVCodecContext* m_codecContext = nullptr;
    AVStream* m_stream = nullptr;
    SwsContext* m_swsContext = nullptr;
    AVBufferRef* hw_device_ctx = nullptr;
    bool isEncoding = false;
};

#endif // ENCODINGMANAGER_H
