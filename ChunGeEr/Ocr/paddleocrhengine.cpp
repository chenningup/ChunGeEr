#include "paddleocrhengine.h"
#include <QFile>
#include <QDebug>
#include <QTextStream>
#include <cmath>
#include <algorithm>
#include <numeric>

// ============================================================
//  构造 / 析构
// ============================================================
PaddleOcrEngine::PaddleOcrEngine(QObject *parent)
    : QObject{parent}
{
    mSessionOpt.SetIntraOpNumThreads(2);
    mSessionOpt.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_DISABLE_ALL);
    mMemInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
}

PaddleOcrEngine::~PaddleOcrEngine() = default;

PaddleOcrEngine &PaddleOcrEngine::Instance()
{
    static PaddleOcrEngine engine;
    return engine;
}

// ============================================================
//  初始化
// ============================================================
bool PaddleOcrEngine::init(const QString &modelDir)
{
    // 多种模型名尝试顺序 (按优先级)
    QStringList detCandidates = {
        "ch_PP-OCRv4_det_infer.onnx",
        "ch_PP-OCRv3_det_infer.onnx",
        "det.onnx"
    };
    QStringList recCandidates = {
        "ch_PP-OCRv4_rec_infer.onnx",
        "ch_PP-OCRv2_rec_infer.onnx",
        "ch_PP-OCRv3_rec_infer.onnx",
        "rec.onnx"
    };

    QString detPath, recPath, dictPath;
    for (const auto &c : detCandidates) {
        QString p = modelDir + "/" + c;
        if (QFile::exists(p)) { detPath = p; break; }
    }
    for (const auto &c : recCandidates) {
        QString p = modelDir + "/" + c;
        if (QFile::exists(p)) { recPath = p; break; }
    }
    dictPath = modelDir + "/ppocr_keys_v1.txt";

    if (!loadDetModel(detPath)) {
        qWarning() << "[PaddleOcr] 检测模型加载失败:" << detPath;
        return false;
    }
    if (!loadRecModel(recPath)) {
        qWarning() << "[PaddleOcr] 识别模型加载失败:" << recPath;
        return false;
    }
    if (!loadDict(dictPath)) {
        qWarning() << "[PaddleOcr] 字典加载失败:" << dictPath;
        return false;
    }

    mReady = true;
    qDebug() << "[PaddleOcr] 初始化完成"
             << "dict=" << mDict.size() << "chars"
             << "det=" << mDetInputDims[2] << "x" << mDetInputDims[3]
             << "rec_seq=" << mRecOutSeqLen;
    return true;
}

// ============================================================
//  模型加载
// ============================================================
bool PaddleOcrEngine::loadDetModel(const QString &path)
{
    if (!QFile::exists(path)) {
        qWarning() << "[PaddleOcr] 检测模型不存在:" << path;
        return false;
    }
    try {
        mDetSession = Ort::Session(mEnv, path.toStdWString().c_str(), mSessionOpt);

        // 输入信息
        Ort::TypeInfo typeInfo = mDetSession.GetInputTypeInfo(0);
        auto tensorInfo = typeInfo.GetTensorTypeAndShapeInfo();
        mDetInputDims = tensorInfo.GetShape();
        mDetInputNames.push_back("x");

        // 输出信息
        size_t numOut = mDetSession.GetOutputCount();
        for (size_t i = 0; i < numOut; ++i)
            mDetOutputNames.push_back("sigmoid_0.tmp_0");

        // 动态 batch: N=1 固定
        if (mDetInputDims[0] < 0) mDetInputDims[0] = 1;
        if (mDetInputDims[1] < 0) mDetInputDims[1] = 3;

        qDebug() << "[PaddleOcr] 检测模型加载成功, 输出=" << numOut;
        return true;
    }
    catch (const Ort::Exception &e) {
        qWarning() << "[PaddleOcr] 检测模型加载异常:" << e.what();
        return false;
    }
}

bool PaddleOcrEngine::loadRecModel(const QString &path)
{
    if (!QFile::exists(path)) {
        qWarning() << "[PaddleOcr] 识别模型不存在:" << path;
        return false;
    }
    try {
        mRecSession = Ort::Session(mEnv, path.toStdWString().c_str(), mSessionOpt);

        Ort::TypeInfo typeInfo = mRecSession.GetInputTypeInfo(0);
        auto tensorInfo = typeInfo.GetTensorTypeAndShapeInfo();
        mRecInputDims = tensorInfo.GetShape();
        mRecInputNames.push_back("x");

        // 获取输出形状确定序列长度
        auto outInfo = mRecSession.GetOutputTypeInfo(0);
        auto outShape = outInfo.GetTensorTypeAndShapeInfo().GetShape();
        mRecOutSeqLen = outShape[1];       // [1, seq_len, num_classes]
        mRecNumClasses = outShape[2];

        size_t numOut = mRecSession.GetOutputCount();
        mRecOutputNames.push_back("softmax_0.tmp_0");

        if (mRecInputDims[0] < 0) mRecInputDims[0] = 1;
        if (mRecInputDims[1] < 0) mRecInputDims[1] = 3;
        if (mRecInputDims[2] < 0) mRecInputDims[2] = 48;

        qDebug() << "[PaddleOcr] 识别模型加载成功, seq=" << mRecOutSeqLen
                 << "classes=" << mRecNumClasses;
        return true;
    }
    catch (const Ort::Exception &e) {
        qWarning() << "[PaddleOcr] 识别模型加载异常:" << e.what();
        return false;
    }
}

bool PaddleOcrEngine::loadDict(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "[PaddleOcr] 字典文件无法打开:" << path;
        // 给个默认字典 (至少加载空格)
        mDict << "blank";   // index 0 = CTC blank
        for (int i = 0; i < 95; ++i)
            mDict << QChar(0x21 + i); // ASCII 可打印
        mRecNumClasses = mDict.size();
        return false;
    }
    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    mDict.clear();
    while (!in.atEnd()) {
        QString line = in.readLine();
        if (!line.isEmpty())
            mDict << line;
    }
    mRecNumClasses = mDict.size();
    qDebug() << "[PaddleOcr] 字典加载:" << mDict.size() << "chars";
    return true;
}

// ============================================================
//  对外接口
// ============================================================
QString PaddleOcrEngine::identify(cv::Mat &pic)
{
    if (!mReady || pic.empty())
        return {};

    auto blocks = identifyBlocks(pic);
    QStringList lines;
    for (const auto &b : blocks) {
        if (!b.text.trimmed().isEmpty())
            lines << b.text.trimmed();
    }
    return lines.join('\n');
}

QList<PaddleOcrEngine::TextBlock> PaddleOcrEngine::identifyBlocks(cv::Mat &pic)
{
    QList<TextBlock> result;
    if (!mReady || pic.empty())
        return result;

    // 1) 检测 —— 找出所有文本框
    int resizeH, resizeW;
    double scale;
    cv::Mat detInput = detPreprocess(pic, resizeH, resizeW, scale);

    // ONNX 推理: detection
    Ort::AllocatorWithDefaultOptions allocator;
    std::vector<float> detBlob(detInput.total());
    std::memcpy(detBlob.data(), detInput.data, detBlob.size() * sizeof(float));

    std::vector<int64_t> detShape{1, 3, resizeH, resizeW};
    Ort::Value detTensor = Ort::Value::CreateTensor<float>(
        mMemInfo, detBlob.data(), detBlob.size(), detShape.data(), detShape.size());

    std::vector<Ort::Value> detOutput = mDetSession.Run(
        Ort::RunOptions{},
        mDetInputNames.data(), &detTensor, 1,
        mDetOutputNames.data(), mDetOutputNames.size());

    if (detOutput.empty() || !detOutput[0].IsTensor())
        return result;

    // 解析检测输出
    float *probData = detOutput[0].GetTensorMutableData<float>();
    auto probShape = detOutput[0].GetTensorTypeAndShapeInfo().GetShape();
    int probH = probShape[2];  // H
    int probW = probShape[3];  // W

    // DB 后处理
    auto detBoxes = detPostprocess(probData, probH, probW,
                                   pic.rows, pic.cols, scale);

    // 2) 识别 —— 对每个文本框做 OCR
    for (auto &db : detBoxes) {
        // 裁剪文本区域
        cv::Mat textImg = cropTextImg(pic, db);
        if (textImg.empty() || textImg.cols < 2 || textImg.rows < 2)
            continue;

        // 识别预处理
        int recW;
        cv::Mat recInput = recPreprocess(textImg, recW);
        if (recInput.empty())
            continue;

        // ONNX 推理: recognition
        std::vector<float> recBlob(recInput.total());
        std::memcpy(recBlob.data(), recInput.data, recBlob.size() * sizeof(float));

        std::vector<int64_t> recShape{1, 3, mRecImgH, recW};
        Ort::Value recTensor = Ort::Value::CreateTensor<float>(
            mMemInfo, recBlob.data(), recBlob.size(),
            recShape.data(), recShape.size());

        auto recOutput = mRecSession.Run(
            Ort::RunOptions{},
            mRecInputNames.data(), &recTensor, 1,
            mRecOutputNames.data(), mRecOutputNames.size());

        if (recOutput.empty() || !recOutput[0].IsTensor())
            continue;

        // CTC 解码
        auto recOutShape = recOutput[0].GetTensorTypeAndShapeInfo().GetShape();
        int actualSeqLen = recOutShape[1];
        float *recData = recOutput[0].GetTensorMutableData<float>();
        QString text = ctcDecode(recData, actualSeqLen, mRecNumClasses);
        if (text.trimmed().isEmpty())
            continue;

        // 计算文本块的 bounding rect
        cv::Rect bbox = cv::boundingRect(cv::Mat(db.pts));

        // 计算平均置信度
        float conf = 0.0f;
        for (int i = 0; i < actualSeqLen; ++i) {
            int cls = 0;
            float maxV = recData[i * mRecNumClasses];
            float sum = 0;
            for (int c = 0; c < mRecNumClasses; ++c) {
                float v = recData[i * mRecNumClasses + c];
                if (v > maxV) { maxV = v; cls = c; }
                sum += v;
            }
            if (cls != 0) conf += maxV;
        }
        // 用非 blank 位置数计算平均
        int nonBlank = 0;
        int prevCls = -1;
        for (int i = 0; i < actualSeqLen; ++i) {
            int cls = 0;
            float maxV = recData[i * mRecNumClasses];
            for (int c = 0; c < mRecNumClasses; ++c) {
                if (recData[i * mRecNumClasses + c] > maxV) {
                    maxV = recData[i * mRecNumClasses + c];
                    cls = c;
                }
            }
            if (cls > 0 && cls != prevCls) {
                nonBlank++;
                prevCls = cls;
            } else if (cls == 0) {
                prevCls = -1;
            }
        }
        double avgConf = nonBlank > 0 ? conf / nonBlank : 0.0;

        result.append({text.trimmed(), bbox, avgConf});
    }

    return result;
}

// ============================================================
//  检测前处理
// ============================================================
cv::Mat PaddleOcrEngine::detPreprocess(const cv::Mat &src,
                                        int &outH, int &outW,
                                        double &scale)
{
    // 限制最长边
    int maxSide = std::max(src.cols, src.rows);
    if (maxSide > mDetMaxSideLen)
        scale = double(mDetMaxSideLen) / maxSide;
    else
        scale = 1.0;

    int newW = int(src.cols * scale);
    int newH = int(src.rows * scale);
    newW = (newW / 32) * 32;
    newH = (newH / 32) * 32;
    if (newW < 32) newW = 32;
    if (newH < 32) newH = 32;

    outW = newW;
    outH = newH;

    cv::Mat resized;
    cv::resize(src, resized, cv::Size(newW, newH), 0, 0, cv::INTER_LINEAR);

    // BGR → RGB → float32 → normalize to [0,1]
    cv::Mat rgb;
    cv::cvtColor(resized, rgb, cv::COLOR_BGR2RGB);
    rgb.convertTo(rgb, CV_32FC3, 1.0 / 255.0);

    // 标准化: mean=[0.485,0.456,0.406], std=[0.229,0.224,0.225]
    static const float mean[3] = {0.485f, 0.456f, 0.406f};
    static const float stdv[3] = {0.229f, 0.224f, 0.225f};
    cv::Mat normed(newH, newW, CV_32FC3);
    for (int y = 0; y < newH; ++y) {
        for (int x = 0; x < newW; ++x) {
            cv::Vec3f p = rgb.at<cv::Vec3f>(y, x);
            normed.at<cv::Vec3f>(y, x) = cv::Vec3f(
                (p[0] - mean[0]) / stdv[0],
                (p[1] - mean[1]) / stdv[1],
                (p[2] - mean[2]) / stdv[2]);
        }
    }

    // HWC → CHW
    cv::Mat chw(newH * 3, newW, CV_32FC1);
    for (int y = 0; y < newH; ++y) {
        for (int x = 0; x < newW; ++x) {
            cv::Vec3f p = normed.at<cv::Vec3f>(y, x);
            chw.at<float>(0 * newH + y, x) = p[0];
            chw.at<float>(1 * newH + y, x) = p[1];
            chw.at<float>(2 * newH + y, x) = p[2];
        }
    }

    return chw;
}

// ============================================================
//  DB 后处理
// ============================================================
std::vector<PaddleOcrEngine::DetBox>
PaddleOcrEngine::detPostprocess(const float *probMap,
                                 int oh, int ow,
                                 int origH, int origW,
                                 double scale)
{
    // 1) 将概率图转成 cv::Mat + sigmoid + threshold
    cv::Mat probMat(oh, ow, CV_32FC1);
    for (int y = 0; y < oh; ++y) {
        for (int x = 0; x < ow; ++x) {
            float val = probMap[y * ow + x];
            probMat.at<float>(y, x) = 1.0f / (1.0f + std::exp(-val));
        }
    }

    // 2) 二值化
    cv::Mat binary;
    cv::threshold(probMat, binary, mDetThresh, 255, cv::THRESH_BINARY);
    binary.convertTo(binary, CV_8UC1);

    // 3) 形态学开运算去噪
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(2, 2));
    cv::morphologyEx(binary, binary, cv::MORPH_OPEN, kernel);

    // 4) 找轮廓
    std::vector<std::vector<cv::Point>> contours;
    std::vector<cv::Vec4i> hierarchy;
    cv::findContours(binary, contours, hierarchy,
                     cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);

    double scaleInv = 1.0 / scale;

    std::vector<DetBox> boxes;
    for (const auto &contour : contours) {
        // 计算外包矩形
        cv::Rect r = cv::boundingRect(contour);
        // 跳过太小区域 (过滤噪声)
        if (r.area() < 10)
            continue;

        // 拟合最小外接矩形 (旋转矩形)
        cv::RotatedRect rotatedRect = cv::minAreaRect(contour);
        cv::Point2f pts[4];
        rotatedRect.points(pts);

        // 缩放回原图
        DetBox box;
        box.score = rotatedRect.size.area()
                        / double(origW * origH); // 简单评分
        if (box.score < mDetBoxThresh)
            continue;

        for (int i = 0; i < 4; ++i) {
            box.pts.emplace_back(
                pts[i].x * scaleInv,
                pts[i].y * scaleInv);
        }
        boxes.push_back(box);
    }

    // 5) 按 y 坐标排序（从上到下）
    std::sort(boxes.begin(), boxes.end(),
              [](const DetBox &a, const DetBox &b) {
                  // 用中心点 y 排序
                  float cyA = 0, cyB = 0;
                  for (auto &p : a.pts) cyA += p.y;
                  for (auto &p : b.pts) cyB += p.y;
                  return (cyA / 4) < (cyB / 4);
              });

    return boxes;
}

// ============================================================
//  文本裁剪
// ============================================================
cv::Mat PaddleOcrEngine::cropTextImg(const cv::Mat &src, const DetBox &box)
{
    if (box.pts.size() < 4) return {};

    // 计算外包矩形的宽高比
    cv::RotatedRect rr = cv::minAreaRect(cv::Mat(box.pts));
    cv::Size2f sz = rr.size;
    if (sz.width < sz.height)
        std::swap(sz.width, sz.height);

    // 透视变换矫正
    cv::Point2f dstPts[4];
    dstPts[0] = cv::Point2f(0, 0);
    dstPts[1] = cv::Point2f(sz.width, 0);
    dstPts[2] = cv::Point2f(sz.width, sz.height);
    dstPts[3] = cv::Point2f(0, sz.height);

    cv::Point2f srcPts[4];
    rr.points(srcPts);

    // 确保顺序一致 (顺时针/逆时针)
    // 调整 srcPts 顺序匹配 dstPts: 左上→右上→右下→左下
    // 寻找最接近 (0,0) 的点作为左上角
    std::vector<int> order = {0, 1, 2, 3};
    std::sort(order.begin(), order.end(),
              [&srcPts](int a, int b) {
                  return srcPts[a].x + srcPts[a].y < srcPts[b].x + srcPts[b].y;
              });
    // reorder
    cv::Point2f srcOrdered[4];
    for (int i = 0; i < 4; ++i)
        srcOrdered[i] = srcPts[order[i]];

    cv::Mat M = cv::getPerspectiveTransform(srcOrdered, dstPts);
    cv::Mat cropped;
    cv::warpPerspective(src, cropped, M, cv::Size(int(sz.width), int(sz.height)));

    return cropped;
}

// ============================================================
//  识别前处理
// ============================================================
cv::Mat PaddleOcrEngine::recPreprocess(const cv::Mat &textImg, int &outW)
{
    // 保持宽高比 resize 到 height=48
    int h = textImg.rows;
    int w = textImg.cols;
    if (h <= 0 || w <= 0) return {};

    int newH = mRecImgH;
    int newW = int(float(w) / h * newH);
    // 限制最少 4px，然后对齐到 4 的倍数
    if (newW < 4) newW = 4;
    newW = (newW / 4) * 4;
    if (newW < 4) newW = 4;

    outW = newW;

    cv::Mat resized;
    cv::resize(textImg, resized, cv::Size(newW, newH), 0, 0, cv::INTER_LINEAR);

#if 1   // 可以给识别结果截图调试
//    static int idx = 0;
//    cv::imwrite(QString("rec_crop_%1.png").arg(idx++).toStdString(), resized);
#endif

    // BGR → GRAY → 3-channel
    cv::Mat gray;
    if (resized.channels() == 3) {
        cv::cvtColor(resized, gray, cv::COLOR_BGR2GRAY);
    } else if (resized.channels() == 4) {
        cv::cvtColor(resized, gray, cv::COLOR_BGRA2GRAY);
    } else {
        gray = resized.clone();
    }

    cv::Mat threeCh(newH, newW, CV_8UC3);
    for (int y = 0; y < newH; ++y) {
        for (int x = 0; x < newW; ++x) {
            uchar g = gray.at<uchar>(y, x);
            threeCh.at<cv::Vec3b>(y, x) = cv::Vec3b(g, g, g);
        }
    }

    // normalize → float32 [0,1]
    cv::Mat normed;
    threeCh.convertTo(normed, CV_32FC3, 1.0 / 255.0);

    // HWC → CHW
    cv::Mat chw(newH * 3, newW, CV_32FC1);
    for (int y = 0; y < newH; ++y) {
        for (int x = 0; x < newW; ++x) {
            cv::Vec3f p = normed.at<cv::Vec3f>(y, x);
            chw.at<float>(0 * newH + y, x) = p[0];
            chw.at<float>(1 * newH + y, x) = p[1];
            chw.at<float>(2 * newH + y, x) = p[2];
        }
    }

    return chw;
}

// ============================================================
//  CTC 解码
// ============================================================
QString PaddleOcrEngine::ctcDecode(const float *probs,
                                    int seqLen, int numClasses)
{
    QString result;
    int prevCls = -1;    // CTC 合并重复非 blank

    for (int t = 0; t < seqLen; ++t) {
        int maxIdx = 0;
        float maxVal = probs[t * numClasses];
        for (int c = 1; c < numClasses; ++c) {
            float v = probs[t * numClasses + c];
            if (v > maxVal) {
                maxVal = v;
                maxIdx = c;
            }
        }

        if (maxIdx == 0) {
            // blank
            prevCls = -1;
        } else if (maxIdx != prevCls) {
            if (maxIdx - 1 < mDict.size())
                result += mDict[maxIdx];  // index 0 = blank, 1-indexed in dict
            else
                result += QChar(0xFFFD);  // replacement
            prevCls = maxIdx;
        }
    }

    return result;
}
