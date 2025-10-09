#include "mainwindow.h"
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>
#include <QApplication>
#include <QDebug>

#include <QThread>
#include <opencv2/opencv.hpp>
using namespace cv;
using namespace std;
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
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
