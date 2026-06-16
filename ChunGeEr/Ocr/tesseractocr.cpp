#include "tesseractocr.h"
#include "XuLog.h"
#include <QCoreApplication>
#include <QDir>
#include <QTemporaryFile>

TesseractOcr::TesseractOcr()
{
}

TesseractOcr::~TesseractOcr()
{
}

bool TesseractOcr::init(const std::string &tesseractExePath,
                         const std::string &tessdataDir,
                         const std::string &lang)
{
    m_exePath = tesseractExePath;
    m_tessdataDir = tessdataDir;
    m_lang = lang;

    // verify tesseract.exe exists and works
    QProcess proc;
    proc.start(QString::fromStdString(m_exePath), {"--version"});
    if (!proc.waitForFinished(5000)) {
        errorf("[TesseractOcr] tesseract.exe 超时");
        return false;
    }
    QString out = proc.readAllStandardOutput();
    if (out.isEmpty()) out = proc.readAllStandardError();
    if (!out.contains("tesseract")) {
        errorf("[TesseractOcr] tesseract.exe 不可用: {}", out.toStdString());
        return false;
    }
    infof("[TesseractOcr] 初始化成功: {} ({})", m_exePath, m_lang);

    m_ready = true;
    return true;
}

QString TesseractOcr::identify(const cv::Mat &bgr)
{
    if (!m_ready || bgr.empty())
        return {};

    // save to temp file
    QString tempDir = QCoreApplication::applicationDirPath() + "/ocr_temp";
    QDir().mkpath(tempDir);
    QString imgPath = tempDir + "/tess_input.png";
    QString outPath = tempDir + "/tess_output";

    cv::imwrite(imgPath.toStdString(), bgr);

    // run tesseract CLI
    QProcess proc;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("TESSDATA_PREFIX", QString::fromStdString(m_tessdataDir));
    proc.setProcessEnvironment(env);

    QStringList args;
    args << imgPath << outPath << "-l" << QString::fromStdString(m_lang) << "--psm" << "3";

    proc.start(QString::fromStdString(m_exePath), args);
    if (!proc.waitForFinished(10000)) {
        errorf("[TesseractOcr] OCR 超时");
        proc.kill();
        return {};
    }

    // read output
    QFile file(outPath + ".txt");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        infof("[TesseractOcr] 无输出文件");
        return {};
    }
    QString text = QString::fromUtf8(file.readAll());
    file.close();

    // cleanup
    QFile::remove(imgPath);
    QFile::remove(outPath + ".txt");

    return text.trimmed();
}
