#ifndef OCRMNAGER_H
#define OCRMNAGER_H
#include <opencv2/opencv.hpp>
#include <QObject>
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>
class OcrMnager : public QObject
{
    Q_OBJECT
public:
    explicit OcrMnager(QObject *parent = nullptr);

    static OcrMnager&Instance();

    void init();

    QString identify(cv::Mat &pic);
signals:

private:

    tesseract::TessBaseAPI tess;
};

#endif // OCRMNAGER_H
