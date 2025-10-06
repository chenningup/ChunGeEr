#include "storagevidoemanager.h"
#include <QDebug>

StorageVidoeManager::StorageVidoeManager(QObject *parent)
    : QThread{parent},isSaving(false)
{

}

StorageVidoeManager &StorageVidoeManager::Instance()
{
    static StorageVidoeManager mStorageVidoeManager;
    return mStorageVidoeManager;
}

void StorageVidoeManager::init()
{
    isSaving = true;
    start();
    QString fileName = "D:\\asdfasdf.mp4";
    avformat_alloc_output_context2(&m_formatContext, nullptr, nullptr, fileName.toStdString().c_str());
    if (!m_formatContext)
        return;

    // 查找 H.264 编码器（可使用硬件加速编码器，如 "h264_nvenc"）
    const AVCodec* codec = avcodec_find_encoder_by_name("libx264");
    if (!codec)
        return ;

    m_stream = avformat_new_stream(m_formatContext, nullptr);
    m_codecContext = avcodec_alloc_context3(codec);

    // 配置编码器参数 [5](@ref)
    m_codecContext->width = 1920;
    m_codecContext->height = 1080;
    m_codecContext->time_base = {1, 30};
    m_codecContext->framerate = {30, 1};
    m_codecContext->pix_fmt = AV_PIX_FMT_YUV420P;
    m_codecContext->bit_rate = 1000000; // 1 Mbps

    if (avcodec_open2(m_codecContext, codec, nullptr) < 0)
        return ;
    avcodec_parameters_from_context(m_stream->codecpar, m_codecContext);

    // 打开输出文件
    if (avio_open(&m_formatContext->pb, fileName.toStdString().c_str(), AVIO_FLAG_WRITE) < 0)
        return ;
    if (avformat_write_header(m_formatContext, nullptr) < 0)
        return ;

    // 3. 创建用于数据转换的 SwsContext (将DXGI的BGRA转换为FFmpeg的YUV420P) [2](@ref)
    m_swsContext = sws_getContext(1920, 1080, AV_PIX_FMT_BGRA,
                                  1920, 1080, AV_PIX_FMT_YUV420P,
                                  SWS_BILINEAR, nullptr, nullptr, nullptr);
    connect(&ScreenCaptureManager::Instance(),&ScreenCaptureManager::capturedScreen,this,&StorageVidoeManager::receiveCaptureScreen);
}
static long m_frameIndex = 0;
void StorageVidoeManager::run()
{
    while (isSaving)
    {
        mScreenSem.acquire();
        mScreenMutex.lock();
        ScreenData tmp;
        if(!mScreenList.empty())
        {
            tmp =  mScreenList[0];
        }
        mScreenMutex.unlock();
        if(tmp.data)
        {
            AVFrame* frame = av_frame_alloc();
            frame->width = m_codecContext->width;
            frame->height = m_codecContext->height;
            frame->format = AV_PIX_FMT_YUV420P;
            av_frame_get_buffer(frame, 0);

            // 设置源数据指针（来自DXGI的BGRA数据）
            uint8_t* srcData[1] = { (uint8_t*)tmp.data->data() };
            int srcLinesize[1] = { tmp.RowPitch };

            // 使用 SwsContext 进行颜色空间和格式转换
            sws_scale(m_swsContext, srcData, srcLinesize, 0, tmp.des.Height,
                      frame->data, frame->linesize);

            frame->pts = m_frameIndex++; // 设置时间戳

            // 6. 编码并写入文件 [2,5](@ref)
            AVPacket packet;
            av_init_packet(&packet);
            packet.data = nullptr;
            packet.size = 0;

            if (avcodec_send_frame(m_codecContext, frame) == 0) {
                qDebug()<<"send frame success";
                int ret = 0;
                while (ret >= 0) {
                    ret = avcodec_receive_packet(m_codecContext, &packet);

                    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                        break;
                    } else if (ret < 0) {
                        break;
                    }

                    // 成功接收到编码包，写入文件
                    av_packet_rescale_ts(&packet, m_codecContext->time_base, m_stream->time_base);
                    packet.stream_index = m_stream->index;
                    av_interleaved_write_frame(m_formatContext, &packet);
                    av_packet_unref(&packet);
                     qDebug()<< "write" <<m_frameIndex;
                }
            }


            // 7. 清理本帧资源
            av_frame_free(&frame);


            mScreenMutex.lock();
            mScreenList.pop_front();
            mScreenMutex.unlock();
        }
    }
}

void StorageVidoeManager::receiveCaptureScreen(ScreenCaptureCore::ScreenData data)
{
    mScreenMutex.lock();
    mScreenList.push_back(data);
    mScreenMutex.unlock();
    mScreenSem.release();
}
