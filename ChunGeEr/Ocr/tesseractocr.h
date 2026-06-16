#ifndef TESSERACTOCR_H
#define TESSERACTOCR_H

#include <opencv2/opencv.hpp>
#include <QString>
#include <QProcess>
#include <QDebug>

class TesseractOcr
{
public:
    TesseractOcr();
    ~TesseractOcr();

    bool init(const std::string &tesseractExePath,
              const std::string &tessdataDir,
              const std::string &lang = "chi_sim");

    bool isReady() const { return m_ready; }

    QString identify(const cv::Mat &bgr);

private:
    bool m_ready = false;
    std::string m_exePath;
    std::string m_tessdataDir;
    std::string m_lang;
};

#endif // TESSERACTOCR_H
