#include "storagevidoemanager.h"
#include <QDebug>
#include "../Encode/encodingmanager.h"
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
    connect(&EncodingManager::Instance(),&EncodingManager::encodedAVPacket,this,&StorageVidoeManager::receiveCaptureScreen,Qt::QueuedConnection);
}
static long m_frameIndex = 0;
void StorageVidoeManager::run()
{
    while (isSaving)
    {
        mScreenSem.acquire();
        mScreenMutex.lock();
        std::shared_ptr<SafePacket> tmp;
        if(!mScreenList.empty())
        {
            tmp =  mScreenList[0];
        }
        mScreenMutex.unlock();
        if(tmp && tmp->valid())
        {
            // 成功接收到编码包，写入文件
            av_packet_rescale_ts(tmp->get(), EncodingManager::Instance().getAVCodecContext()->time_base, m_stream->time_base);
            tmp->get()->stream_index = m_stream->index;
            int write_ret =  av_interleaved_write_frame(m_formatContext,tmp->get());
            if (write_ret < 0) {
                // 处理写入错误：可以打印日志、中断录制等
                qDebug() << "Error writing packet to file:" << write_ret;
                // 注意：即使写入失败，也需要释放包资源，否则会内存泄漏
            }
            avio_flush(m_formatContext->pb);
            qDebug()<< "write" <<m_stream->index;



//            AVFrame* frame = av_frame_alloc();
//            frame->width = m_codecContext->width;
//            frame->height = m_codecContext->height;
//            frame->format = AV_PIX_FMT_YUV420P;
//            av_frame_get_buffer(frame, 0);

//            // 设置源数据指针（来自DXGI的BGRA数据）
//            uint8_t* srcData[1] = { (uint8_t*)tmp.data->data() };
//            int srcLinesize[1] = { tmp.RowPitch };

//            // 使用 SwsContext 进行颜色空间和格式转换
//            sws_scale(m_swsContext, srcData, srcLinesize, 0, tmp.des.Height,
//                      frame->data, frame->linesize);

//            frame->pts = m_frameIndex++; // 设置时间戳

            mScreenMutex.lock();
            mScreenList.pop_front();
            mScreenMutex.unlock();
        }
    }
}

void StorageVidoeManager::startSaveVideo(const QString &filePath)
{
    avformat_alloc_output_context2(&m_formatContext, nullptr, nullptr, filePath.toStdString().c_str());
    if (!m_formatContext)
        return;
    m_stream = avformat_new_stream(m_formatContext, nullptr);

    avcodec_parameters_from_context(m_stream->codecpar, EncodingManager::Instance().getAVCodecContext());

    // 打开输出文件
    if (avio_open(&m_formatContext->pb, filePath.toStdString().c_str(), AVIO_FLAG_WRITE) < 0)
        return ;
    if (avformat_write_header(m_formatContext, nullptr) < 0)
        return ;
    isSaving = true;
    start();

}

void StorageVidoeManager::stopSaveVideo()
{
    isSaving = false;
    mScreenSem.release(); // 确保线程退出等待

    wait(); // 等待线程结束

    // 关键：写入文件尾
    if (m_formatContext) {
        av_write_trailer(m_formatContext);
    }

    if (m_formatContext) {
        avio_closep(&m_formatContext->pb);
        avformat_free_context(m_formatContext);
        m_formatContext = nullptr;
    }
}

void StorageVidoeManager::receiveCaptureScreen(std::shared_ptr<SafePacket> data)
{
    //return;
    if(!isSaving)
    {
        return;
    }
    mScreenMutex.lock();
    mScreenList.push_back(data);
    mScreenMutex.unlock();
    mScreenSem.release();
}

