#include "mainwindow.h"
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>
#include <QApplication>
#include <QDebug>
#include "ScreenCapture.h"
#include <QThread>
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QThread::sleep(5);
    ScreenCaptureCore::ScreenCapture capture;

    capture.CaptureToFile("D:\\123.bmp");

   tesseract::TessBaseAPI tess;
   if (tess.Init("F:\\Program Files\\Tesseract-OCR\\tessdata", "chi_sim") != 0) {
       return 0;
   }
   QString path ="D:\\123.bmp";
   Pix *image = pixRead(path.toLocal8Bit().data());
   tess.SetImage(image);
   char *text = tess.GetUTF8Text();
   QString ocrResult = QString::fromUtf8(text);
   qDebug()<<ocrResult;
   // 显示结果

   // 释放资源
   pixDestroy(&image);
   delete[] text;
   tess.End();
    MainWindow w;
    w.show();
    return a.exec();
}
