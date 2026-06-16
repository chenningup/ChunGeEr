// ═══════════════════════════════════════════════════════════
// preprocessForOCR: 将游戏文字裁剪图预处理为标准黑白文字
// 移除描边、投影、抗锯齿 → 白纸黑字风格
// ═══════════════════════════════════════════════════════════
static void preprocessForOCR(const cv::Mat &src, cv::Mat &dst)
{
    using namespace cv;

    // 1. 转灰度
    Mat gray;
    if (src.channels() == 3)
        cvtColor(src, gray, COLOR_BGR2GRAY);
    else
        gray = src.clone();

    // 2. 放大 3x（游戏文字通常 12-18px，放大到 36-54px 才够 OCR）
    Mat upscaled;
    resize(gray, upscaled, Size(), 3.0, 3.0, INTER_CUBIC);

    // 3. 白礼帽（Top-Hat）：提取比背景亮的文字
    //    游戏 UI 上大部分文字是亮色（白/黄）文字配深色描边
    int kernelSize = std::max(3, upscaled.cols / 20);  // 自适应核大小
    Mat kernel = getStructuringElement(MORPH_ELLIPSE, Size(kernelSize, kernelSize));
    Mat tophat;
    morphologyEx(upscaled, tophat, MORPH_TOPHAT, kernel);

    // 4. 黑礼帽（Black-Hat）：提取比背景暗的描边/阴影
    Mat blackhat;
    morphologyEx(upscaled, blackhat, MORPH_BLACKHAT, kernel);

    // 5. 合并两者（亮色文字 + 暗色轮廓 = 完整的文字形状）
    Mat merged = tophat + blackhat;

    // 6. OTSU 二值化
    Mat binary;
    threshold(merged, binary, 0, 255, THRESH_BINARY | THRESH_OTSU);

    // 7. 如果 OTSU 分离度不够（前景 < 2% 或 > 95%），用自适应
    double fgRatio = countNonZero(binary) / (double)(binary.rows * binary.cols);
    if (fgRatio < 0.02 || fgRatio > 0.90) {
        // 回退到自适应阈值
        adaptiveThreshold(upscaled, binary, 255,
                         ADAPTIVE_THRESH_GAUSSIAN_C,
                         THRESH_BINARY_INV, 13, 5);
        // 检测是否需要反转（亮底暗字 → 反转成黑底白字 → 再反转成白底黑字）
        Scalar m = mean(binary);
        if (m[0] > 128) bitwise_not(binary, binary);
    }

    // 8. 形态学降噪
    Mat denoiseK = getStructuringElement(MORPH_ELLIPSE, Size(2, 2));
    morphologyEx(binary, binary, MORPH_OPEN, denoiseK);
    morphologyEx(binary, binary, MORPH_CLOSE, getStructuringElement(MORPH_RECT, Size(3, 1)));

    // 9. 反转成白底黑字（Tesseract 标准输入）
    bitwise_not(binary, binary);

    // 10. 加白边
    copyMakeBorder(binary, dst, 12, 12, 12, 12, BORDER_CONSTANT, Scalar(255));
}
