#include "ocrmnager.h"
#include "paddleocr.h"
#include "tesseractocr.h"
#include <QDebug>
#include <QCoreApplication>
#include <QDir>
#include "XuLog.h"

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
    QString baseDir = QCoreApplication::applicationDirPath() + "/models/ocr";

    // ── PaddleOCR ────────────────────────────
    QString detPath  = baseDir + "/ch_PP-OCRv3_det_infer.onnx";
    QString recPath  = baseDir + "/ch_PP-OCRv2_rec_infer.onnx";
    QString dictPath = baseDir + "/ppocr_keys_v1.txt";

    infof("[OcrMnager] 初始化 PaddleOCR...");
    m_paddleOcr = std::make_unique<PaddleOcr>();
    if (m_paddleOcr->init(detPath.toStdString(), recPath.toStdString(), dictPath.toStdString())) {
        infof("[OcrMnager] PaddleOCR OK");
    } else {
        errorf("[OcrMnager] PaddleOCR 失败");
    }

    // ── Tesseract ────────────────────────────
    // 使用系统安装的 Tesseract（PATH 中或固定路径）
    QString tessExe = "D:/Program Files/Tesseract-OCR/tesseract.exe";
    QString tessData = "D:/Program Files/Tesseract-OCR/tessdata";
    infof("[OcrMnager] 初始化 Tesseract: {}", tessExe.toStdString());
    m_tessOcr = std::make_unique<TesseractOcr>();
    if (m_tessOcr->init(tessExe.toStdString(), tessData.toStdString(), "chi_sim")) {
        infof("[OcrMnager] Tesseract OK");
    } else {
        errorf("[OcrMnager] Tesseract 失败");
    }

    infof("[OcrMnager] 当前引擎: {}", engineName().toStdString());
}

bool OcrMnager::isReady() const
{
    if (m_engine == EnginePaddleOCR)
        return m_paddleOcr && m_paddleOcr->isReady();
    else
        return m_tessOcr && m_tessOcr->isReady();
}

void OcrMnager::setEngine(OcrEngine engine)
{
    if (m_engine != engine) {
        m_engine = engine;
        infof("[OcrMnager] 切换到: {}", engineName().toStdString());
        emit engineChanged(engine);
    }
}

QString OcrMnager::engineName() const
{
    return m_engine == EnginePaddleOCR ? "PaddleOCR" : "Tesseract";
}

QString OcrMnager::identify(cv::Mat &pic)
{
    if (m_engine == EnginePaddleOCR) {
        if (!m_paddleOcr || !m_paddleOcr->isReady()) {
            infof("[OcrMnager] PaddleOCR 未就绪");
            return {};
        }
        return m_paddleOcr->identify(pic);
    } else {
        if (!m_tessOcr || !m_tessOcr->isReady()) {
            infof("[OcrMnager] Tesseract 未就绪");
            return {};
        }
        return m_tessOcr->identify(pic);
    }
}
