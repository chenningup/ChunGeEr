#include "ocrmnager.h"
#include <QDebug>
using namespace cv;
OcrMnager::OcrMnager(QObject *parent)
    : QObject{parent}
{

}

OcrMnager &OcrMnager::Instance()
{
    static OcrMnager mOcrMnager;
    return mOcrMnager;
}

void OcrMnager::init()
{
    if (tess.Init("D:\\Program Files\\Tesseract-OCR\\tessdata", "chi_sim_custom") != 0)
    {
        qDebug()<< "tesseract init error";
        return ;
    }
    qDebug()<< "tesseract init success";
}
static int index = 0;
QString OcrMnager::identify(cv::Mat &pic)
{
    Mat gray;
    if (pic.empty())
    {
        //cout << "Could not open or find the image!" << endl;
        return "";
    }
    if (pic.channels() == 4)
    {
        cvtColor(pic, gray, cv::COLOR_BGRA2GRAY);
    }
    else
    {
        cvtColor(pic, gray, cv::COLOR_BGR2GRAY);
    }
    Mat binary;
    threshold(gray, binary, 105, 255, THRESH_BINARY);
    QString fineName = QString("binary_%1.png").arg(index);
    cv::imwrite(fineName.toStdString(), binary);
    index++;
    PIX* image = nullptr;
    if (binary.channels() == 1)
    {  // 灰度图
        image = pixCreate(binary.cols, binary.rows, 8); // 8位深度
        for (int y = 0; y < binary.rows; y++)
        {
            for (int x = 0; x < binary.cols; x++)
            {
                pixSetPixel(image, x, y, binary.at<uchar>(y, x));
            }
        }
    }
    tess.SetImage(image);
    char *text = tess.GetUTF8Text();
    QString ocrResult = QString::fromUtf8(text);
    pixDestroy(&image);
    delete[] text;
    return ocrResult;
}
