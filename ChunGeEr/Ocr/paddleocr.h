#ifndef PADDLEOCR_H
#define PADDLEOCR_H

#include <opencv2/opencv.hpp>
#include <onnxruntime_cxx_api.h>
#include <QString>
#include <QStringList>
#include <QDebug>
#include <vector>
#include <memory>

// ── DB 检测后处理用的数据结构 ──
struct TextBox {
    std::vector<cv::Point> box;  // 4个角点
    float score;
};

class PaddleOcr
{
public:
    PaddleOcr();
    ~PaddleOcr();

    // 初始化：加载 det/rec ONNX 模型 + 字典
    bool init(const std::string &detModelPath,
              const std::string &recModelPath,
              const std::string &dictPath);

    bool isReady() const { return m_ready; }

    // 核心 OCR：输入 BGR 图像，返回识别文本（行用 \n 分隔）
    QString identify(const cv::Mat &bgr);

private:
    // ── 检测 ──
    std::vector<TextBox> detect(const cv::Mat &bgr);
    cv::Mat detPreprocess(const cv::Mat &bgr);
    void detPostprocess(const std::vector<Ort::Value> &outputs,
                        float ratioW, float ratioH,
                        int originW, int originH,
                        std::vector<TextBox> &boxes);

    // ── 排序 ──
    static std::vector<TextBox> sortBoxes(const std::vector<TextBox> &boxes);

    // ── 裁剪 ──
    cv::Mat getCropImage(const cv::Mat &bgr, const TextBox &box);

    // ── 识别 ──
    QString recognize(const cv::Mat &crop);
    cv::Mat recPreprocess(const cv::Mat &crop);

    // ── CTC 贪心解码 ──
    QString ctcDecode(const float *probs, int timeSteps, int numClasses);

    // ── ONNX Runtime 会话 ──
    Ort::Env m_env{nullptr};
    Ort::SessionOptions m_sessionOpts;
    std::unique_ptr<Ort::Session> m_detSession;
    std::unique_ptr<Ort::Session> m_recSession;

    // 字典
    std::vector<std::string> m_dict;

    // 输入名
    std::string m_detInputName;
    std::string m_recInputName;

    bool m_ready = false;

    // DB 后处理参数
    float m_dbThresh   = 0.1f;
    float m_dbBoxThresh = 0.3f;
    float m_unclipRatio = 1.5f;
    int   m_detLimitSide = 960;

    // 识别参数
    int m_recImgH = 32;
};

#endif // PADDLEOCR_H
