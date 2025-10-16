#include "screenshare.h"
#include "hv/EventLoop.h"
#include "hv/htime.h"
#include "hv/hssl.h"
#include <QDebug>
#include "hv/WebSocketClient.h"
#include "hv/WebSocketServer.h"
#include "encodingmanager.h"
#include <QImage>
#include "Detector/detectormanager.h"
static WebSocketService ws;
static websocket_server_t server;
static hv::WebSocketClient wsClient;
static QList<WebSocketChannelPtr>clientList;

ScreenShare::ScreenShare(QObject *parent)
    : QThread{parent}
{

}

ScreenShare &ScreenShare::Instance()
{
    static ScreenShare mScreenShare;
    return mScreenShare;
}

void ScreenShare::init()
{
    server.port = 9999;
    server.ws = &ws;
    ws.onopen = [this](const WebSocketChannelPtr &channel, const HttpRequestPtr &req) {
        // 转发到类成员
        qDebug()<<"connect";
        clientList.push_back(channel);
    };

    ws.onmessage = [this](const WebSocketChannelPtr &channel, const std::string &msg) {

        const char* p = (const  char*)msg.data();
        std::shared_ptr<QByteArray> data = std::make_shared<QByteArray>(p,msg.size());
        mScreenMutex.lock();
        screenData.push_back(data);
        mScreenMutex.unlock();
        mScreenSem.release();
    };

    ws.onclose = [this](const WebSocketChannelPtr &channel) {
        for (int i = 0; i < clientList.size(); ++i)
        {
            if(clientList[i].get() == channel.get())
            {
                clientList.removeAt(i);
                break;
            }
        }
    };
    // client 端的回调（通常 onopen/onmessage/onclose 的签名不同，按 hv 的定义写）
    wsClient.onopen = [this]()
    {

    };

    wsClient.onmessage = [this](const std::string &msg)
    {
        if(msg.empty())
        {
            return;
        }
        json msgData= json::parse(msg);
        //emit clientRecMeg(msgData);
        // json data = json::parse(msg);
        // qDebug()<<QString::fromStdString(msg);
    };

    wsClient.onclose = [this]()
    {
    };

    // 1. 查找H.264解码器
    const AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec)
    {
        qDebug() << "H.264解码器未找到";
        return;
    }

    // 2. 创建解码器上下文
    codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        qDebug() << "无法分配解码器上下文";
        return;
    }

    // 3. 设置解码器参数（如果是网络流，可能需要手动设置SPS/PPS）
    // 示例：假设你的流包含Annex B格式的SPS/PPS
//    uint8_t extradata[] = {
//        0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0x80, 0x0d, // SPS
//        0x00, 0x00, 0x00, 0x01, 0x68, 0xce, 0x38, 0x80  // PPS
//    };
//    codec_ctx->extradata = (uint8_t*)av_malloc(sizeof(extradata));
//    memcpy(codec_ctx->extradata, extradata, sizeof(extradata));
//    codec_ctx->extradata_size = sizeof(extradata);

    // 4. 打开解码器
    if (avcodec_open2(codec_ctx, codec, nullptr) < 0)
    {
        qDebug() << "无法打开解码器";
        return;
    }

    // 5. 初始化帧和包
    frame = av_frame_alloc();
    pkt = av_packet_alloc();
    if (!frame || !pkt)
    {
        qDebug() << "内存分配失败";
        return;
    }

}
extern bool isMaster;
void ScreenShare::run()
{
    while(isShare)
    {
        mScreenSem.acquire();
        mScreenMutex.lock();
        std::shared_ptr<QByteArray> tmp;
        if(!screenData.isEmpty())
        {
            tmp = screenData[0];
            screenData.pop_front();
        }
        mScreenMutex.unlock();
        if(!tmp)
        {
            continue;
        }
        pkt->data = (uint8_t*)(tmp->data());
        pkt->size = tmp->size();

        // 2. 发送到解码器
        if (avcodec_send_packet(codec_ctx, pkt) < 0)
        {
            qDebug() << "发送数据包失败";
            return;
        }

        // 3. 接收解码后的帧
        while (avcodec_receive_frame(codec_ctx, frame) == 0)
        {
            if (!sws_ctx)
            {
                sws_ctx = sws_getContext(
                    frame->width, frame->height, (AVPixelFormat)frame->format,
                    frame->width, frame->height, AV_PIX_FMT_RGB32,
                    SWS_BILINEAR, nullptr, nullptr, nullptr);

                // cv::mat need AV_PIX_FMT_BGR24
            }

            // 2. 转换格式到QImage
            QImage image(frame->width, frame->height, QImage::Format_RGB32);
            uint8_t *dst[] = { image.bits() };
            int dst_linesize[] = { static_cast<int>(image.bytesPerLine()) };
            sws_scale(sws_ctx, frame->data, frame->linesize, 0, frame->height, dst, dst_linesize);


//            cv::Mat img(frame->height, frame->width, CV_8UC3);
//            uint8_t* dst[] = { img.data };
//            int dst_linesize[] = { static_cast<int>(img.step) };

//            // 6. 执行转换
//            sws_scale(sws_ctx, frame->data, frame->linesize,
//                      0, frame->height, dst, dst_linesize);

            //cv::imshow("Result of Detection", img);
            //cv::waitKey(0);
//            std::vector<DL_RESULT> res = DetectorManager::Instance().detector(img);
//            for (auto& re : res)
//            {
//                cv::RNG rng(cv::getTickCount());
//                cv::Scalar color(rng.uniform(0, 256), rng.uniform(0, 256), rng.uniform(0, 256));

//                cv::rectangle(img, re.box, color, 3);

//                float confidence = floor(100 * re.confidence) / 100;
//                std::cout << std::fixed << std::setprecision(2);
//                std::string label = DetectorManager::Instance().yoloDetector->classes[re.classId] + " " +
//                                    std::to_string(confidence).substr(0, std::to_string(confidence).size() - 4);


//                cv::rectangle(
//                    img,
//                    cv::Point(re.box.x, re.box.y - 25),
//                    cv::Point(re.box.x + label.length() * 15, re.box.y),
//                    color,
//                    cv::FILLED
//                    );

//                cv::putText(
//                    img,
//                    label,
//                    cv::Point(re.box.x, re.box.y - 5),
//                    cv::FONT_HERSHEY_SIMPLEX,
//                    0.75,
//                    cv::Scalar(0, 0, 0),
//                    2
//                    );
//            }
//            cv::imshow("Live", img);
//            cv::waitKey(1);


            emit showScreen(image);
            // 3. 显示图像（跨线程需用信号槽）
            //currentImage = image.scaled(videoLabel->size(), Qt::KeepAspectRatio);
            //videoLabel->setPixmap(QPixmap::fromImage(currentImage));
        }
    }
}
extern QString serverIp;
void ScreenShare::startShare()
{
    isShare = true;
    if(isMaster)
    {
        websocket_server_run(&server, 0);
        start();
    }
    else
    {
        QString url = "ws://"+serverIp+":9999";
        reconn_setting_t reconn;
        reconn_setting_init(&reconn);
        reconn.min_delay = 1000;
        reconn.max_delay = 5000;
        wsClient.setReconnect(&reconn);
        wsClient.open(url.toStdString().data());
        connect(&EncodingManager::Instance(),&EncodingManager::encodedAVPacket,this,&ScreenShare::receiveCaptureScreen,Qt::QueuedConnection);
    }
}

void ScreenShare::stopShare()
{
    if(isMaster)
    {
        websocket_server_stop(&server);
    }
    else
    {
        wsClient.close();
        disconnect(&EncodingManager::Instance(),&EncodingManager::encodedAVPacket,this,&ScreenShare::receiveCaptureScreen);
    }

}

void ScreenShare::receiveCaptureScreen(std::shared_ptr<SafePacket> data)
{
    if(data)
    {
        wsClient.send((const char*)data->get()->data, data->get()->size);
    }
}
