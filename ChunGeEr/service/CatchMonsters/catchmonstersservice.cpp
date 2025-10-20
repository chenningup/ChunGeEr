#include "catchmonstersservice.h"
#include <QImage>
#include <opencv2/opencv.hpp>
#include "../../Detector/detectormanager.h"
#include <QDebug>
#include "../../Ocr/ocrmnager.h"
CatchMonstersService::CatchMonstersService(QObject *parent):BaseService(parent)
{

}
int index = 51;
void CatchMonstersService::run()
{

    cv::namedWindow("Live", cv::WINDOW_AUTOSIZE);
    while(toRun)
    {
        if(curPic.data)
        {
            uint32_t dataSize = curPic.RowPitch * curPic.des.Height;
            // // std::vector<uint8_t>> buffer
            // //     = std::vector<uint8_t>(dataSize);
            picMutex.lock();
            uint8_t *buffer = new uint8_t[dataSize];
            memcpy(buffer,curPic.data->data(), dataSize);
            picMutex.unlock();
            // QImage image(buffer,
            //              curPic.des.Width,
            //              curPic.des.Height,
            //              QImage::Format_ARGB32);
            // QRect  cropRect(0, 0, 1024, 800);
            // // 截取图片
            // QImage croppedImage = image.copy(cropRect);
            // croppedImage.save(QString("xuanyuan_%1.png").arg(index));
            // index++;

            cv::Mat img1(curPic.des.Height,curPic.des.Width, CV_8UC4, buffer, curPic.RowPitch);
            cv::Mat bgr;
            cv::cvtColor(img1, bgr, cv::COLOR_BGR2RGB);
            cv::Rect roi_rect(0, 0, 1024, 800); // 从 (100,50) 开始，截取 200x150 的区域

            // 截取 ROI
            cv::Mat img = bgr(roi_rect).clone();
            std::vector<DL_RESULT> res = DetectorManager::Instance().detector(img);
            for (auto& re : res)
            {
                cv::RNG rng(cv::getTickCount());
                cv::Scalar color(rng.uniform(0, 256), rng.uniform(0, 256), rng.uniform(0, 256));

                cv::rectangle(img, re.box, color, 3);

                float confidence = floor(100 * re.confidence) / 100;
                std::cout << std::fixed << std::setprecision(2);
                std::string label = DetectorManager::Instance().yoloDetector->classes[re.classId] + " " +
                                    std::to_string(confidence).substr(0, std::to_string(confidence).size() - 4);

                cv::rectangle(
                    img,
                    cv::Point(re.box.x, re.box.y - 25),
                    cv::Point(re.box.x + label.length() * 15, re.box.y),
                    color,
                    cv::FILLED
                    );

                cv::putText(
                    img,
                    label,
                    cv::Point(re.box.x, re.box.y - 5),
                    cv::FONT_HERSHEY_SIMPLEX,
                    0.75,
                    cv::Scalar(0, 0, 0),
                    2
                    );
            }
            if(res.size() != 0)
            {
                // 25  160 27
                cv::Rect ocr_rect(300, 25, 160, 27); // 从 (100,50) 开始，截取 200x150 的区域
                // 截取 ROI
                cv::Mat cormat = img1(ocr_rect).clone();
                QString res = OcrMnager::Instance().identify(cormat);
                qDebug()<<res;
            }
            cv::imshow("Live", img);
            cv::waitKey(1);

            delete [] buffer;
        }
        //QThread::msleep(10);
    }
}

void CatchMonstersService::startService()
{
    toRun = true;
    start();
}

void CatchMonstersService::stopService()
{

}

