#include "mainwindow.h"

#include <QApplication>
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    tesseract::TessBaseAPI tess;
    if (tess.Init("D:\\Program Files\\Tesseract-OCR\\tessdata", "datang+chi_sim") != 0) {
        return 0;
    }
    QDir dir(QApplication::applicationDirPath());

    // 设置名称过滤器，只查找.bmp文件
    QStringList filters;
    filters << "*.png";

    // 获取所有匹配的文件
    QStringList bmpFiles = dir.entryList(filters, QDir::Files | QDir::NoDotAndDotDot);

    // 输出结果
    qDebug() << "Found BMP files:";
    foreach (const QString &file, bmpFiles) {
        Mat imageMat = imread(file.toStdString()); // 请替换为你的图片路径
        cv::imshow("imageMat", imageMat);
        cv::waitKey(0);
        qDebug()<<file << imageMat.channels();
        Mat gray;
        if (imageMat.empty())
        {
            cout << "Could not open or find the image!" << endl;
            return -1;
        }
        if (imageMat.channels() == 4) {
            cvtColor(imageMat, gray, COLOR_BGRA2GRAY);
        } else {
            cvtColor(imageMat, gray, COLOR_BGR2GRAY);
        }
        //        cv::imshow("gray", gray);
        //        cv::waitKey(0);
        // Otsu 自动阈值二值化
        Mat binary;
        cv::threshold(gray, binary, 105, 255, THRESH_BINARY);

        // // 保存结果
        QString output = QString("output_%1.png").arg(file);
        //        cv::imshow("binary", binary);
        //        cv::waitKey(0);
        //cv::imwrite(output.toStdString(), binary);
        qDebug()<<binary.channels();
        PIX* image = nullptr;
        if (binary.channels() == 1) {  // 灰度图
            image = pixCreate(binary.cols, binary.rows, 8); // 8位深度
            for (int y = 0; y < binary.rows; y++) {
                for (int x = 0; x < binary.cols; x++) {
                    pixSetPixel(image, x, y, binary.at<uchar>(y, x));
                }
            }
        }
        tess.SetImage(image);
        char *text = tess.GetUTF8Text();
        QString ocrResult = QString::fromUtf8(text);
        qDebug()<<ocrResult;
        pixDestroy(&image);
        delete[] text;
    }
    MainWindow w;
    w.show();
    return a.exec();
}
