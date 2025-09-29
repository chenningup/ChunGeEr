#include "mainwindow.h"
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>
#include <QApplication>
#include <QDebug>
#include "ScreenCapture.h"
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
//    tesseract::TessBaseAPI tess;
//    if (tess.Init("D:\\Program Files\\Tesseract-OCR\\tessdata", "chi_sim") != 0) {
//        return 0;
//    }
//    // 识别图片
//    QString path ="123123.jpg";
//    Pix *image = pixRead(path.toLocal8Bit().data());
//    tess.SetImage(image);
//    char *text = tess.GetUTF8Text();
//    QString ocrResult = QString::fromUtf8(text);
//    qDebug()<<ocrResult;
//    // 显示结果

//    // 释放资源
//    pixDestroy(&image);
//    delete[] text;
//    tess.End();

    ScreenCaptureCore::ScreenCapture capture;

    capture.CaptureToFile("D:\\123.png");
    MainWindow w;
    w.show();
    return a.exec();
}
