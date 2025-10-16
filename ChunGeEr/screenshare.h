#ifndef SCREENSHARE_H
#define SCREENSHARE_H

#include <QObject>
#include <QThread>
#include "nlohmann/json.hpp"
#include "safepacket.h"
#include <QMutex>
#include <QSemaphore>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}
using json =  nlohmann::json;
class ScreenShare : public QThread
{
    Q_OBJECT
public:
    explicit ScreenShare(QObject *parent = nullptr);

    static ScreenShare&Instance();

    void init();

    void run();

    void startShare();

    void stopShare();

signals:
    void showScreen(QImage pic);
public slots:
    void receiveCaptureScreen(std::shared_ptr<SafePacket> data);
public:
    bool isShare = false;
    QList<std::shared_ptr<QByteArray>>screenData;
    QSemaphore mScreenSem;
    QMutex mScreenMutex;

    // FFmpeg资源
    AVCodecContext *codec_ctx = nullptr;
    AVFrame *frame = nullptr;
    AVPacket *pkt = nullptr;
    SwsContext *sws_ctx = nullptr;
};

#endif // SCREENSHARE_H
