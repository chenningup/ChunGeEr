#include "record.h"

Record::Record(QObject *parent)
    : BaseService{parent}
{
    connect(&recordTime,&QTimer::timeout,this,&Record::timeOutSlot);
}
static int index = 0;
void Record::run()
{
    if(curPic.data)
    {
        uint32_t dataSize = curPic.RowPitch * curPic.des.Height;
        picMutex.lock();
        uint8_t *buffer = new uint8_t[dataSize];
        memcpy(buffer,curPic.data->data(), dataSize);
        picMutex.unlock();
        cv::Mat img1(curPic.des.Height,curPic.des.Width, CV_8UC4, buffer, curPic.RowPitch);
        cv::Rect roi_rect(0, 0, 1024, 800); // 从 (100,50) 开始，截取 200x150 的区域
        cv::Mat img = img1(roi_rect).clone();
        QString file = QString("Image/recorsd_%1.png").arg(index);
        cv::imwrite(file.toStdString(), img);
        index++;
    }
}

void Record::startService()
{
    index = 0;
    recordTime.start(500);
}

void Record::stopService()
{
    recordTime.stop();
}

void Record::timeOutSlot()
{
    start();
}
