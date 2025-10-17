#include "encodingmanager.h"
#include <QDebug>
#include <QApplication>
#include <QScreen>
EncodingManager::EncodingManager(QObject *parent)
    : QThread{parent}
{

}

EncodingManager &EncodingManager::Instance()
{
    static EncodingManager mEncodingManager;
    return mEncodingManager;
}

void EncodingManager::init()
{
    // 查找 H.264 编码器（可使用硬件加速编码器，如 "h264_nvenc"）
    const AVCodec* codec = avcodec_find_encoder_by_name("libx264");

    //const AVCodec* codec = avcodec_find_encoder_by_name("libx265");

    if (!codec)
        return ;

    m_codecContext = avcodec_alloc_context3(codec);


    QScreen *primaryScreen = QGuiApplication::primaryScreen();
    if (primaryScreen)
    {
        qDebug() << "主屏幕分辨率:"
                 << primaryScreen->size().width() << "x"
                 << primaryScreen->size().height();
    }


    // 配置编码器参数 [5](@ref)
    m_codecContext->width = primaryScreen->size().width();
    m_codecContext->height = primaryScreen->size().height();
    m_codecContext->time_base = {1, 30};
    m_codecContext->framerate = {30, 1};
    m_codecContext->pix_fmt = AV_PIX_FMT_YUV420P;
    m_codecContext->bit_rate = 1000000; // 1 Mbps
    m_codecContext->max_b_frames = 0;          // 禁用B帧，减少内部缓存
    m_codecContext->has_b_frames = 0;
    m_codecContext->gop_size = 30;             // GOP长度
    m_codecContext->flags |= AV_CODEC_FLAG_LOW_DELAY; // 低延迟

//    av_opt_set(m_codecContext->priv_data, "preset", "fast", 0);      // 编码速度/质量权衡
//    av_opt_set(m_codecContext->priv_data, "tune", "zerolatency", 0);// 低延迟优化
//    av_opt_set(m_codecContext->priv_data, "x265-params", "crf=23:ref=3", 0);

    if (avcodec_open2(m_codecContext, codec, nullptr) < 0)
        return ;
    // 3. 创建用于数据转换的 SwsContext (将DXGI的BGRA转换为FFmpeg的YUV420P) [2](@ref)
//    m_swsContext = sws_getContext(3840, 2160, AV_PIX_FMT_BGRA,
//                                  3840, 2160, AV_PIX_FMT_YUV420P,
//                                  SWS_BILINEAR, nullptr, nullptr, nullptr);
    m_swsContext = sws_getContext(
        m_codecContext->width, m_codecContext->height, AV_PIX_FMT_BGRA,
        m_codecContext->width, m_codecContext->height, AV_PIX_FMT_YUV420P,
        SWS_BILINEAR, nullptr, nullptr, nullptr);
    connect(&ScreenCaptureManager::Instance(),&ScreenCaptureManager::capturedScreen,this,&EncodingManager::receiveCaptureScreen,Qt::QueuedConnection);
}
//static long m_frameIndex = 0;
void EncodingManager::run()
{
    AVFrame* frame = av_frame_alloc();
    frame->width  = m_codecContext->width;
    frame->height = m_codecContext->height;
    frame->format = m_codecContext->pix_fmt;

    if (av_frame_get_buffer(frame, 32) < 0) {
        //qWarning() << "Failed to allocate frame buffer";
        return;
    }
    // 2️⃣ 循环使用一个 AVPacket
    AVPacket* packet = av_packet_alloc();
    if (!packet) {
        qWarning() << "Failed to allocate AVPacket";
        return;
    }
    // 帧计数
    static long m_frameIndex = 0;
    while (isEncoding)
    {
        mScreenSem.acquire();
        mScreenMutex.lock();
        ScreenCaptureManager::ScreenData tmp;
        if(!mScreenList.empty())
        {
            tmp =  mScreenList[0];
            mScreenList.pop_front();
        }
        mScreenMutex.unlock();
        if(tmp.data)
        {
            av_frame_make_writable(frame);
            // 设置源数据指针（来自DXGI的BGRA数据）
            uint8_t* srcData[1] = { (uint8_t*)tmp.data->data() };
            int srcLinesize[1] = { tmp.RowPitch };

            // 使用 SwsContext 进行颜色空间和格式转换
            sws_scale(m_swsContext, srcData, srcLinesize, 0, tmp.des.Height,
                      frame->data, frame->linesize);

            frame->pts = m_frameIndex++; // 设置时间戳


            if (avcodec_send_frame(m_codecContext, frame) < 0) {
                qWarning() << "avcodec_send_frame failed";
                continue;
            }

            while (true) {
                // 5️⃣ 复用同一个 packet，每次清空旧数据
                av_packet_unref(packet);

                int ret = avcodec_receive_packet(m_codecContext, packet);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF || ret < 0) {
                    break;
                }

                // 6️⃣ SafePacket 接管 ownership
               emit encodedAVPacket(SafePacket::fromRaw(packet));
               //av_packet_unref(packet);
            }
        }
        //()<<mScreenList.size();
    }
    av_frame_free(&frame);
    av_packet_free(&packet);
}

void EncodingManager::startEncodeing()
{
    isEncoding = true;
    start();
}

void EncodingManager::stopEncodeing()
{
    isEncoding = false;
    mScreenSem.release(); // 确保线程退出等待

    //wait(); // 等待线程结束

    // 按分配顺序逆序释放资源
//    if (m_swsContext) {
//        sws_freeContext(m_swsContext);
//        m_swsContext = nullptr;
//    }

//    if (m_codecContext) {
//        avcodec_free_context(&m_codecContext);
//    }

}

void EncodingManager::receiveCaptureScreen(ScreenCaptureManager::ScreenData data)
{
    if(!isEncoding)
    {
        return;
    }
    mScreenMutex.lock();
    mScreenList.push_back(data);
    mScreenMutex.unlock();
    mScreenSem.release();
}
