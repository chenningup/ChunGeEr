#ifndef PADDLEOCRENGINE_H
#define PADDLEOCRENGINE_H

#include <opencv2/opencv.hpp>
#include <QObject>
#include <QStringList>
#include <QVector>
#include "onnxruntime_cxx_api.h"

/**
 * @brief PaddleOCR ONNX 引擎 — PP-OCRv4
 *
 * 管线: DB 文本检测 → 仿射变换 → CRNN 文字识别
 * 含完整前后处理 (DB 解码 / CTC 解码)
 *
 * Model 依赖 (从 ppocr-onnx 仓库下载):
 *   ch_PP-OCRv3_det_infer.onnx  (检测, ~2.4MB)
 *   ch_PP-OCRv2_rec_infer.onnx  (识别, ~8.3MB)
 *   cls-model.onnx              (方向分类, ~0.6MB, 可选)
 *   ppocr_keys_v1.txt           (中文字典, ~26KB)
 */
class PaddleOcrEngine : public QObject
{
    Q_OBJECT
public:
    explicit PaddleOcrEngine(QObject *parent = nullptr);
    ~PaddleOcrEngine();

    /** 单例 */
    static PaddleOcrEngine &Instance();

    /** 初始化: 加载模型 + 字典 */
    bool init(const QString &modelDir);

    /** 是否已就绪 */
    bool ready() const { return mReady; }

    // ── 与 OcrMnager 兼容的接口 ──

    /** 全图 → 整块文字 */
    QString identify(cv::Mat &pic);

    /** 全图 → 逐文本块 (含 bbox) */
    struct TextBlock {
        QString text;
        cv::Rect bbox;          // 在原图坐标系下的矩形
        double confidence;
    };
    QList<TextBlock> identifyBlocks(cv::Mat &pic);

    // ── 配置 ──
    void setDetThreshold(double thresh) { mDetThresh = qBound(0.01, thresh, 0.99); }
    void setDetBoxThreshold(double thresh) { mDetBoxThresh = thresh; }
    void setDetMaxSideLen(int len) { mDetMaxSideLen = qMax(32, len); }
    void setRecImgH(int h) { mRecImgH = qMax(16, h); }

private:
    // ── 模型加载 ──
    bool loadDetModel(const QString &path);
    bool loadRecModel(const QString &path);
    bool loadDict(const QString &path);

    // ── DB 检测前处理 ──
    cv::Mat detPreprocess(const cv::Mat &src, int &resizeH, int &resizeW, double &scale);

    // ── DB 后处理: 概率图 → 文本框 ──
    struct DetBox {
        std::vector<cv::Point2f> pts;   // 4 个角点
        float score;
    };
    std::vector<DetBox> detPostprocess(const float *probMap,
                                       int oh, int ow,
                                       int origH, int origW,
                                       double scale);

    // ── 文本框裁剪 + 矫正 ──
    cv::Mat cropTextImg(const cv::Mat &src, const DetBox &box);

    // ── 识别前处理 ──
    cv::Mat recPreprocess(const cv::Mat &textImg, int &resizeW);

    // ── CTC 解码: 概率矩阵 → 字符串 ──
    QString ctcDecode(const float *probs, int seqLen, int numClasses);

    // ── ONNX Runtime 资源 ──
    Ort::Env             mEnv{ORT_LOGGING_LEVEL_WARNING};
    Ort::SessionOptions  mSessionOpt;

    // 检测 session
    Ort::Session         mDetSession{nullptr};
    std::vector<const char *> mDetInputNames;
    std::vector<const char *> mDetOutputNames;
    std::vector<int64_t>      mDetInputDims;    // NCHW

    // 识别 session
    Ort::Session         mRecSession{nullptr};
    std::vector<const char *> mRecInputNames;
    std::vector<const char *> mRecOutputNames;
    std::vector<int64_t>      mRecInputDims;    // NCHW
    int                  mRecOutSeqLen = 0;     // 输出序列长度
    int                  mRecNumClasses = 0;    // 字典大小

    // 字典
    QStringList          mDict;                 // index → char
    Ort::MemoryInfo      mMemInfo{nullptr};

    // 状态
    bool mReady = false;

    // 参数
    double mDetThresh     = 0.3;    // DB 二值化阈值
    double mDetBoxThresh  = 0.5;    // 文本框置信度
    int    mDetMaxSideLen = 960;    // 检测输入长边
    int    mRecImgH       = 48;     // 识别输入高度
};

#endif // PADDLEOCRENGINE_H
