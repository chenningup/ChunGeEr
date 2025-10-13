#include "mainwindow.h"
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>
#include <QApplication>
#include <QDebug>

#include <QThread>
#include <opencv2/opencv.hpp>
using namespace cv;
using namespace std;

unsigned char buffer[100];
int pr = 0;

enum pasreDataStatus
{
    findHeadFirtst,
    findHeadSecond,
    findEndFirtst,
    findEndSecond,
};
pasreDataStatus status = findHeadFirtst;
uint16_t crc_16(uint8_t *data, uint16_t len)
{
    uint16_t crc_reg = 0xffff;
    for (uint16_t i = 0; i < len; i++)
    {
        //infof(" crc_16{:x} i {}", data[i],i);
        crc_reg ^= data[i];
        for (uint16_t j = 0; j < 8; j++)
        {
            if (crc_reg & 0x01)
            {
                crc_reg = ((crc_reg >> 1) ^ 0xa001);
            }
            else
            {
                crc_reg = crc_reg >> 1;
            }
        }
    }
    return crc_reg;
}
void loop() {
    // 检查串口是否有数据到达[1](@ref)
    QByteArray data;
    data.append(0x66);
    data.append(0x68);
    data.append(0x02);
    data.append(0x01);
    data.append(0x9c);
    data.append(0xff);
    data.append(0xff);
    data.append(0xff);
    data.append(0x9c);
    data.append(0xff);
    data.append(0xff);
    data.append(0xff);
    data.append(0xC5);
    data.append(0xA0);
    data.append(0x5B);
    data.append(0x81);
    for (int var = 0; var < data.size(); ++var)
    {
        unsigned char inChar = data[var];
        switch(status)
        {
        case findHeadFirtst:
        {
            if(inChar == 0X66)
            {
                buffer[pr] = 0X66;
                pr++;
                status = findHeadSecond;
            }
        }
        break;
        case findHeadSecond:
        {
            if(inChar == 0X68)
            {
                buffer[pr] = 0X68;
                pr++;
                status = findEndFirtst;
            }
            else
            {
                pr=0;
                status = findHeadFirtst;
            }
        }
        break;
        case findEndFirtst:
        {
            buffer[pr] = inChar;
            if(inChar == 0X5b)
            {
                status = findEndSecond;
            }
            pr++;
        }
        break;
        case findEndSecond:
        {
            buffer[pr] = inChar;
            pr++;
            if(inChar == 0X81)
            {
                uint16_t crc = crc_16(&buffer[2],pr-2-2-2);
                uint16_t recCrc;
                memcpy(&recCrc, &buffer[pr-2-2], 2);
                if (crc != recCrc)
                {
                    pr=0;
                    status = findHeadFirtst;
                    qDebug()<<"crc error";
                }
                else
                {
                    int cmd =  buffer[3];
                    if(cmd == 1)//键盘
                    {
                        int x;
                        memcpy(&x, &buffer[4], 4);
                        int y;
                        memcpy(&y, &buffer[8], 4);
                        //Mouse.move(x, y, 0);
                        //Mouse.move(y, y, 0);
                        //Keyboard.write(buffer[4]);
                    }
                    if(cmd == 2)
                    {

                    }
                    pr=0;
                    status = findHeadFirtst;
                }
            }
            else
            {
                status = findEndFirtst;
                pr=0;
            }
        }
        break;
        }
        // 读取来自串口的字符串，直到遇到换行符'\n'
    }
}
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    loop();
    //QThread::sleep(10);

    //  Mat imageMat = imread("test.png"); // 请替换为你的图片路径
    //  cv::imshow("imageMat", imageMat);
    //  cv::waitKey(0);
    //  Mat gray;
    //  if (imageMat.empty()) {
    //      cout << "Could not open or find the image!" << endl;
    //      return -1;
    //  }
    //  if (imageMat.channels() == 4) {
    //      cvtColor(imageMat, gray, COLOR_BGRA2GRAY);
    //  } else {
    //      cvtColor(imageMat, gray, COLOR_BGR2GRAY);
    //  }
    //  cv::imshow("gray", gray);
    //  cv::waitKey(0);
    //  // Otsu 自动阈值二值化
    //  Mat binary;
    //  threshold(gray, binary, 105, 255, THRESH_BINARY);

    //  // 保存结果
    //  cv::imshow("binary", binary);
    //  cv::waitKey(0);
    //  imwrite("binary_output.png", binary);
    // tesseract::TessBaseAPI tess;
    // if (tess.Init("D:\\Program Files\\Tesseract-OCR\\tessdata", "chi_sim_custom") != 0) {
    //     return 0;
    // }




    //QString path ="D:\\123.bmp";
    //   QString path ="test.png";
    //   Pix *image = pixRead(path.toLocal8Bit().data());




    // PIX* image = nullptr;
    // if (binary.channels() == 1) {  // 灰度图
    //     image = pixCreate(binary.cols, binary.rows, 8); // 8位深度
    //     for (int y = 0; y < binary.rows; y++) {
    //         for (int x = 0; x < binary.cols; x++) {
    //             pixSetPixel(image, x, y, binary.at<uchar>(y, x));
    //         }
    //     }
    // }
    // tess.SetImage(image);
    // char *text = tess.GetUTF8Text();
    // QString ocrResult = QString::fromUtf8(text);
    // qDebug()<<ocrResult;
    // // 显示结果

    // // 释放资源
    // pixDestroy(&image);
    // delete[] text;
    // tess.End();
    MainWindow w;
    w.show();
    return a.exec();
}
