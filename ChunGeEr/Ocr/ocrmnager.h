#ifndef OCRMNAGER_H
#define OCRMNAGER_H
#include <opencv2/opencv.hpp>
#include <QObject>
#include <memory>

#include "paddleocr.h"
#include "tesseractocr.h"

class OcrMnager : public QObject
{
    Q_OBJECT
public:
    enum OcrEngine {
        EngineTesseract,
        EnginePaddleOCR
    };

    explicit OcrMnager(QObject *parent = nullptr);

    static OcrMnager&Instance();

    void init();
    bool isReady() const;

    void setEngine(OcrEngine engine);
    OcrEngine engine() const { return m_engine; }
    QString engineName() const;

    QString identify(cv::Mat &pic);

signals:
    void engineChanged(OcrEngine engine);

private:
    OcrEngine m_engine = EnginePaddleOCR;  // 默认 PaddleOCR
    std::unique_ptr<PaddleOcr> m_paddleOcr;
    std::unique_ptr<TesseractOcr> m_tessOcr;
};

#endif // OCRMNAGER_H
