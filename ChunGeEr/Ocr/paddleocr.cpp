#include "paddleocr.h"
#include <numeric>
#include <algorithm>
#include <fstream>
#include <cmath>
#include <codecvt>
#include <locale>
#include <onnxruntime_cxx_api.h>
#include "XuLog.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ═══════════════════════════════════════════════
// 辅助：string → wstring
// ═══════════════════════════════════════════════
static std::wstring toWide(const std::string &s)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
    return conv.from_bytes(s);
}

// ═══════════════════════════════════════════════
// 构造 / 析构
// ═══════════════════════════════════════════════

PaddleOcr::PaddleOcr()
    : m_env(ORT_LOGGING_LEVEL_WARNING, "PaddleOcr")
{
    m_sessionOpts.SetIntraOpNumThreads(2);
    m_sessionOpts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);
}

PaddleOcr::~PaddleOcr() = default;

// ═══════════════════════════════════════════════
// 初始化
// ═══════════════════════════════════════════════

bool PaddleOcr::init(const std::string &detModelPath,
                     const std::string &recModelPath,
                     const std::string &dictPath)
{
    try {
        // 加载检测模型
        m_detSession = std::make_unique<Ort::Session>(m_env,
            toWide(detModelPath).c_str(), m_sessionOpts);
        // 获取输入名
        Ort::AllocatorWithDefaultOptions alloc;
        m_detInputName = m_detSession->GetInputNameAllocated(0, alloc).get();

        // 加载识别模型
        m_recSession = std::make_unique<Ort::Session>(m_env,
            toWide(recModelPath).c_str(), m_sessionOpts);
        m_recInputName = m_recSession->GetInputNameAllocated(0, alloc).get();
    }
    catch (const Ort::Exception &e) {
        errorf("[PaddleOcr] 模型加载失败: {}", e.what());
        return false;
    }

    // 加载字典
    std::ifstream dictFile(dictPath);
    if (!dictFile.is_open()) {
        errorf("[PaddleOcr] 字典加载失败: {}", dictPath);
        return false;
    }
    std::string line;
    while (std::getline(dictFile, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (!line.empty())
            m_dict.push_back(line);
    }
    m_ready = true;
    infof("[PaddleOcr] 初始化完成, 字典大小: {}", m_dict.size());
    return true;
}

// ═══════════════════════════════════════════════
// 主入口
// ═══════════════════════════════════════════════

QString PaddleOcr::identify(const cv::Mat &bgr)
{
    if (!m_ready || bgr.empty())
        return {};

    // 1. 文本检测
    auto boxes = detect(bgr);
    infof("[PaddleOcr] 检测到 {} 个文本框", boxes.size());
    if (boxes.empty())
        return {};

    // 2. 排序（从上到下，从左到右）
    boxes = sortBoxes(boxes);

    // 3. 逐行识别
    QStringList lines;
    for (auto &box : boxes) {
        cv::Mat crop = getCropImage(bgr, box);
        if (crop.empty()) continue;
        QString text = recognize(crop);
        if (!text.trimmed().isEmpty())
            lines.append(text.trimmed());
    }
    return lines.join("\n");
}

// ═══════════════════════════════════════════════
// 检测
// ═══════════════════════════════════════════════

std::vector<TextBox> PaddleOcr::detect(const cv::Mat &bgr)
{
    std::vector<TextBox> boxes;

    // 预处理
    cv::Mat input = detPreprocess(bgr);

    float ratioW = 1.0f, ratioH = 1.0f;
    {
        int h = bgr.rows, w = bgr.cols;
        float maxSide = (std::max)(h, w);
        ratioW = maxSide / (float)w;
        ratioH = maxSide / (float)h;
    }

    int64_t shape[] = {1, 3, input.rows, input.cols};
    Ort::MemoryInfo memInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    Ort::Value inputTensor = Ort::Value::CreateTensor<float>(memInfo,
        (float*)input.data, input.total() * 3,
        shape, 4);

    try {
        auto outNamesVec = m_detSession->GetOutputNames();
        std::vector<const char*> outNames;
        for (auto &n : outNamesVec) outNames.push_back(n.c_str());
        const char *inName = m_detInputName.c_str();
        auto outputs = m_detSession->Run(Ort::RunOptions{nullptr},
            &inName, &inputTensor, 1,
            outNames.data(), outNames.size());

        detPostprocess(outputs, ratioW, ratioH, bgr.cols, bgr.rows, boxes);
    }
    catch (const Ort::Exception &) {}

    return boxes;
}

cv::Mat PaddleOcr::detPreprocess(const cv::Mat &bgr)
{
    int h = bgr.rows, w = bgr.cols;
    float maxSide = (std::max)(h, w);
    int newH, newW;

    if (maxSide > m_detLimitSide) {
        float scale = m_detLimitSide / maxSide;
        newH = (int)(h * scale);
        newW = (int)(w * scale);
    } else {
        newH = h;
        newW = w;
    }

    // padding 到 32 的倍数
    int padH = ((newH + 31) / 32) * 32;
    int padW = ((newW + 31) / 32) * 32;

    cv::Mat resized, padded;
    cv::resize(bgr, resized, cv::Size(newW, newH));
    cv::copyMakeBorder(resized, padded, 0, padH - newH, 0, padW - newW,
                       cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0));

    // HWC → CHW + normalize (mean=0.485,0.456,0.406, std=0.229,0.224,0.225)
    // 实际 PaddleOCR DB 用的是: /255 然后 mean=[0.485,0.456,0.406], std=[0.229,0.224,0.225]
    cv::Mat rgb;
    cv::cvtColor(padded, rgb, cv::COLOR_BGR2RGB);
    cv::Mat f32;
    rgb.convertTo(f32, CV_32FC3, 1.0 / 255.0);

    // Split + normalize
    std::vector<cv::Mat> chns(3);
    cv::split(f32, chns);
    chns[0] = (chns[0] - 0.485f) / 0.229f;
    chns[1] = (chns[1] - 0.456f) / 0.224f;
    chns[2] = (chns[2] - 0.406f) / 0.225f;

    // CHW layout
    cv::Mat chw(padH, padW, CV_32FC3);
    std::vector<cv::Mat> merged = {chns[0], chns[1], chns[2]};
    cv::merge(merged, chw);

    return chw;
}

void PaddleOcr::detPostprocess(const std::vector<Ort::Value> &outputs,
                               float ratioW, float ratioH,
                               int originW, int originH,
                               std::vector<TextBox> &boxes)
{
    if (outputs.empty()) return;

    auto &tensor = outputs[0];
    auto info = tensor.GetTensorTypeAndShapeInfo();
    auto shape = info.GetShape();  // [1, 1, H, W]

    if (shape.size() < 4) return;
    int outH = (int)shape[2];
    int outW = (int)shape[3];

    const float *data = tensor.GetTensorData<float>();

    // 概率图 → 二值图
    cv::Mat prob(outH, outW, CV_32FC1, (void*)data);
    cv::Mat binary;
    cv::threshold(prob, binary, m_dbThresh, 255, cv::THRESH_BINARY);
    binary.convertTo(binary, CV_8UC1);

    // 找轮廓
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(binary, contours, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);

    float minSide = 3;
    for (auto &cnt : contours) {
        float area = (float)cv::contourArea(cnt);
        if (area < minSide * minSide) continue;

        // 最小外接矩形 + 膨胀（Vatti clipping 简化版）
        cv::RotatedRect rrect = cv::minAreaRect(cnt);
        float w = rrect.size.width;
        float h = rrect.size.height;
        if ((std::min)(w, h) < 3) continue;

        // unclip ratio 简化
        float perimeter = (float)cv::arcLength(cnt, true);
        float offset = perimeter * m_unclipRatio * 0.5f;

        // 用 approxPolyDP 近似
        std::vector<cv::Point> approx;
        cv::approxPolyDP(cnt, approx, 0.02 * perimeter, true);

        // 收缩回原图坐标
        std::vector<cv::Point> scaled;
        for (auto &p : approx) {
            int x = (int)(p.x / ratioW);
            int y = (int)(p.y / ratioH);
            x = (std::max)(0, (std::min)(x, originW - 1));
            y = (std::max)(0, (std::min)(y, originH - 1));
            scaled.push_back(cv::Point(x, y));
        }

        if (scaled.size() >= 4) {
            // 取前4个点
            if (scaled.size() > 4)
                scaled.resize(4);
            TextBox tb;
            tb.box = scaled;
            tb.score = 1.0f;
            boxes.push_back(tb);
        }
    }
}

// ═══════════════════════════════════════════════
// 排序：从上到下，从左到右
// ═══════════════════════════════════════════════

std::vector<TextBox> PaddleOcr::sortBoxes(const std::vector<TextBox> &boxes)
{
    if (boxes.size() <= 1) return boxes;

    std::vector<TextBox> sorted = boxes;

    // 按 Y 中心排序
    std::sort(sorted.begin(), sorted.end(), [](const TextBox &a, const TextBox &b) {
        float ay = 0, by = 0;
        for (auto &p : a.box) ay += p.y;
        for (auto &p : b.box) by += p.y;
        return (ay / a.box.size()) < (by / b.box.size());
    });

    // 同行合并：Y 中心差小于平均高度一半，按 X 排序
    float avgH = 0;
    for (auto &b : sorted) {
        float minY = 1e9f, maxY = 0;
        for (auto &p : b.box) {
            if (p.y < minY) minY = p.y;
            if (p.y > maxY) maxY = p.y;
        }
        avgH += (maxY - minY);
    }
    avgH /= sorted.size();

    for (size_t i = 0; i < sorted.size(); i++) {
        float cy = 0;
        for (auto &p : sorted[i].box) cy += p.y;
        cy /= sorted[i].box.size();

        for (size_t j = i + 1; j < sorted.size(); j++) {
            float cy2 = 0;
            for (auto &p : sorted[j].box) cy2 += p.y;
            cy2 /= sorted[j].box.size();

            if (std::abs(cy - cy2) < avgH * 0.5f) {
                // 同行，按 X 排序
                float cx1 = 0, cx2 = 0;
                for (auto &p : sorted[i].box) cx1 += p.x;
                for (auto &p : sorted[j].box) cx2 += p.x;
                if (cx1 / sorted[i].box.size() > cx2 / sorted[j].box.size())
                    std::swap(sorted[i], sorted[j]);
            }
        }
    }

    return sorted;
}

// ═══════════════════════════════════════════════
// 裁剪图像
// ═══════════════════════════════════════════════

cv::Mat PaddleOcr::getCropImage(const cv::Mat &bgr, const TextBox &box)
{
    if (box.box.size() < 4) return {};

    auto &pts = box.box;

    // 找最小包围矩
    int minX = bgr.cols, maxX = 0, minY = bgr.rows, maxY = 0;
    for (auto &p : pts) {
        if (p.x < minX) minX = p.x;
        if (p.x > maxX) maxX = p.x;
        if (p.y < minY) minY = p.y;
        if (p.y > maxY) maxY = p.y;
    }

    minX = (std::max)(0, minX - 2);
    maxX = (std::min)(bgr.cols - 1, maxX + 2);
    minY = (std::max)(0, minY - 2);
    maxY = (std::min)(bgr.rows - 1, maxY + 2);

    if (maxX <= minX || maxY <= minY) return {};

    return bgr(cv::Rect(minX, minY, maxX - minX, maxY - minY)).clone();
}

// ═══════════════════════════════════════════════
// 识别
// ═══════════════════════════════════════════════

QString PaddleOcr::recognize(const cv::Mat &crop)
{
    if (crop.empty()) return {};

    cv::Mat input = recPreprocess(crop);

    int64_t shape[] = {1, 3, m_recImgH, input.cols};
    Ort::MemoryInfo memInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    Ort::Value inputTensor = Ort::Value::CreateTensor<float>(memInfo,
        (float*)input.data, 1 * 3 * m_recImgH * input.cols,
        shape, 4);

    try {
        auto outNamesVec = m_recSession->GetOutputNames();
        std::vector<const char*> outNames;
        for (auto &n : outNamesVec) outNames.push_back(n.c_str());
        const char *inName = m_recInputName.c_str();
        auto outputs = m_recSession->Run(Ort::RunOptions{nullptr},
            &inName, &inputTensor, 1,
            outNames.data(), outNames.size());

        if (!outputs.empty()) {
            auto info = outputs[0].GetTensorTypeAndShapeInfo();
            auto shape = info.GetShape();  // [1, T, num_classes]

            if (shape.size() >= 3) {
                int timeSteps = (int)shape[1];
                int numClasses = (int)shape[2];
                const float *data = outputs[0].GetTensorData<float>();
                return ctcDecode(data, timeSteps, numClasses);
            }
        }
    }
    catch (const Ort::Exception &) {}

    return {};
}

cv::Mat PaddleOcr::recPreprocess(const cv::Mat &crop)
{
    // resize: 高度固定 m_recImgH，宽等比例，然后 pad 到 32 的倍数
    float ratio = (float)m_recImgH / crop.rows;
    int newW = (int)(crop.cols * ratio);

    cv::Mat resized;
    cv::resize(crop, resized, cv::Size(newW, m_recImgH));

    // pad width to multiple of 8
    int padW = ((newW + 7) / 8) * 8;
    cv::Mat padded;
    if (padW > newW) {
        cv::copyMakeBorder(resized, padded, 0, 0, 0, padW - newW,
                           cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0));
    } else {
        padded = resized;
    }

    cv::Mat rgb, f32;
    cv::cvtColor(padded, rgb, cv::COLOR_BGR2RGB);
    rgb.convertTo(f32, CV_32FC3, 1.0 / 255.0);

    // normalize: mean [0.5,0.5,0.5], std [0.5,0.5,0.5]
    f32 = (f32 - 0.5f) / 0.5f;

    return f32;
}

// ═══════════════════════════════════════════════
// CTC 贪心解码
// ═══════════════════════════════════════════════

QString PaddleOcr::ctcDecode(const float *probs, int timeSteps, int numClasses)
{
    if (m_dict.empty()) return {};

    int lastIdx = -1;
    QString result;

    // numClasses 理论上 = 字典大小 + 1 (blank)
    // 但实际可能不同，以 min(numClasses, dict.size()+1) 为准
    int effectiveClasses = (std::min)(numClasses, (int)m_dict.size() + 1);

    for (int t = 0; t < timeSteps; t++) {
        int maxIdx = 0;
        float maxVal = probs[t * numClasses];
        for (int c = 1; c < effectiveClasses; c++) {
            if (probs[t * numClasses + c] > maxVal) {
                maxVal = probs[t * numClasses + c];
                maxIdx = c;
            }
        }
        // maxIdx == 0 通常是 blank
        if (maxIdx > 0 && maxIdx != lastIdx && maxIdx - 1 < (int)m_dict.size()) {
            result += QString::fromStdString(m_dict[maxIdx - 1]);
        }
        lastIdx = maxIdx;
    }

    return result;
}
