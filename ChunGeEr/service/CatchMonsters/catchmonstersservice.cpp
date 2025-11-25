#include "catchmonstersservice.h"
#include <QImage>
#include <opencv2/opencv.hpp>
#include "../../Detector/detectormanager.h"
#include <QDebug>
#include "../../Ocr/ocrmnager.h"
#include <QDateTime>
#include "../../LeoControl/mousekeyboardmanager.h"
#include "XuLog.h"
CatchMonstersService::CatchMonstersService(QObject *parent):BaseService(parent)
{

}
int index = 0;
void CatchMonstersService::run()
{
    //cv::namedWindow("Live", cv::WINDOW_AUTOSIZE);
    //cv::namedWindow("identify", cv::WINDOW_AUTOSIZE);
    QThread::sleep(5);
     chooseLeftGame();
    while(toRun)
    {
        if(curPic.data)
        {
            // uint32_t dataSize = curPic.RowPitch * curPic.des.Height;
            // // // std::vector<uint8_t>> buffer
            // // //     = std::vector<uint8_t>(dataSize);
            // picMutex.lock();
            // uint8_t *buffer = new uint8_t[dataSize];
            // memcpy(buffer,curPic.data->data(), dataSize);
            // picMutex.unlock();
            // // QImage image(buffer,
            // //              curPic.des.Width,
            // //              curPic.des.Height,
            // //              QImage::Format_ARGB32);
            // // QRect  cropRect(0, 0, 1024, 800);
            // // // 截取图片
            // // QImage croppedImage = image.copy(cropRect);
            // // croppedImage.save(QString("xuanyuan_%1.png").arg(index));
            // // index++;

            // cv::Mat img1(curPic.des.Height,curPic.des.Width, CV_8UC4, buffer, curPic.RowPitch);
            // cv::Mat bgr;
            // cv::cvtColor(img1, bgr, cv::COLOR_BGR2RGB);
            // cv::Rect roi_rect(0, 0, 1024, 800); // 从 (100,50) 开始，截取 200x150 的区域

            // // 截取 ROI
            // cv::Mat img = bgr(roi_rect).clone();
            // std::vector<DL_RESULT> res = DetectorManager::Instance().detector(img);
            // for (auto& re : res)
            // {
            //     cv::RNG rng(cv::getTickCount());
            //     cv::Scalar color(rng.uniform(0, 256), rng.uniform(0, 256), rng.uniform(0, 256));

            //     cv::rectangle(img, re.box, color, 3);

            //     float confidence = floor(100 * re.confidence) / 100;
            //     std::cout << std::fixed << std::setprecision(2);
            //     std::string label = DetectorManager::Instance().yoloDetector->classes[re.classId] + " " +
            //                         std::to_string(confidence).substr(0, std::to_string(confidence).size() - 4);

            //     cv::rectangle(
            //         img,
            //         cv::Point(re.box.x, re.box.y - 25),
            //         cv::Point(re.box.x + label.length() * 15, re.box.y),
            //         color,
            //         cv::FILLED
            //         );

            //     cv::putText(
            //         img,
            //         label,
            //         cv::Point(re.box.x, re.box.y - 5),
            //         cv::FONT_HERSHEY_SIMPLEX,
            //         0.75,
            //         cv::Scalar(0, 0, 0),
            //         2
            //         );
            // }
            // bool haswrite = false;
            // if(res.size() != 0 )
            // {
            //     chooseLeftGame();
            //     MouseKeyboardManager::Instance().clickButton(9);
            //     QThread::msleep(200);
            //     cv::Rect ocr_rect(320, 60, 83, 23); // 从 (100,50) 开始，截取 200x150 的区域
            //     // 截取 ROI
            //     cv::Mat cormat = img1(ocr_rect).clone();
            //     QString res = OcrMnager::Instance().identify(cormat);
            //     // cv::imshow("identify", cormat);
            //     // cv::waitKey(1);
            //     if(!res.replace(" ","").contains("轩辕禁卫"))
            //     {
            //         QString output = QString("output_%1.png").arg(index);
            //         cv::Mat gray;
            //         if (cormat.channels() == 4)
            //         {
            //             cv::cvtColor(cormat, gray, cv::COLOR_BGRA2GRAY);
            //         }
            //         else
            //         {
            //             cv::cvtColor(cormat, gray, cv::COLOR_BGR2GRAY);
            //         }
            //         cv::Mat binary;
            //         cv::threshold(gray, binary, 105, 255, cv::THRESH_BINARY);
            //         cv::imwrite(output.toStdString(), binary);
            //         emit SignalSlotConnector::Instance().log("未识别到 禁卫 写入,:"+res);
            //         index++;
            //     }
            //     else
            //     {
            //         if(detectNameColor(cormat) == NAME_WHITE)
            //         {
            //             MouseKeyboardManager::Instance().clickButton('0');
            //             QThread::sleep(15);
            //         }
            //         else
            //         {
            //             MouseKeyboardManager::Instance().clickButton('1');
            //             infof("click 1");
            //         }
            //     }
            //     qDebug()<<"res" << res;
            // }
            // cv::imshow("Live", img);
            // cv::waitKey(1);

            // delete [] buffer;
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

            cv::Rect ocr_rect(320, 60, 83, 23); // 从 (100,50) 开始，截取 200x150 的区域
            // 截取 ROI
            cv::Mat cormat = img1(ocr_rect).clone();
            if(detectNameColor(cormat) == NAME_WHITE)
            {
                chooseLeftGame();
                MouseKeyboardManager::Instance().clickButton('0');
                QThread::sleep(20);
            }
            else if(detectNameColor(cormat) == NAME_RED){


                MouseKeyboardManager::Instance().clickButton('1');
            }
            else{


            }
            delete [] buffer;
            QThread::sleep(1);
        }
    }
}

void CatchMonstersService::startService()
{
    toRun = true;
    start();
}

void CatchMonstersService::stopService()
{
    toRun = false;
}

void CatchMonstersService::test()
{
                            MouseKeyboardManager::Instance().clickButton('0');
    return;
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
    cv::Rect ocr_rect(320, 60, 83, 23); // 从 (100,50) 开始，截取 200x150 的区域
    // 截取 ROI
    cv::Mat cormat = img1(ocr_rect).clone();
    if(detectNameColor(cormat) == NAME_WHITE)
    {
        infof("white");
    }
    else
    {
        infof("other");
    }
}

