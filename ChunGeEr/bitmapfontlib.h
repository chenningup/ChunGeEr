#ifndef BITMAPFONTLIB_H
#define BITMAPFONTLIB_H

#include <QObject>
#include <QHash>
#include <QVector>
#include <QString>
#include <QColor>
#include <opencv2/opencv.hpp>

// ════════════════════════════════════════════════
// 点阵字库识别引擎 — 大漠风格
//
// 原理：
//   1. 颜色过滤二值化（目标颜色 + 偏色容差）
//   2. 纵向投影切字
//   3. 点阵位压缩为十六进制字符串
//   4. 逐位比对（XOR + popcount 算相似度）
//   5. 滑动窗口 FindStr
//
// 文件格式（文本，大漠兼容）：
//   每行: hexBits$charName$effectivePixels$width$height
// ════════════════════════════════════════════════

// ── 颜色过滤器 ──
struct BflColorPoint {
    QColor color;                            // 取样颜色
    QColor bias{0x30, 0x30, 0x30};           // 该颜色的偏色容差
};

struct BflColorFilter {
    QVector<BflColorPoint> points;            // 多种文字颜色

    bool isEmpty() const { return points.isEmpty(); }
    void add(const QColor &c, const QColor &bias = {0x30, 0x30, 0x30}) {
        points.append({c, bias});
    }
    void clear() { points.clear(); }
};

// ── 字形（点阵存储） ──
struct BflGlyph {
    std::string hexBits;    // 十六进制点阵（大漠格式）
    int width = 0;
    int height = 0;
    int effectivePixels = 0; // 有效像素数（1的个数）
    BflColorFilter colorFilter; // 该字形训练时的颜色过滤器（独立存储）
};

// ── 查找结果 ──
struct BflFindResult {
    std::string charName;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    double similarity = 0;  // 0.0 ~ 1.0
};

class BitmapFontLib : public QObject
{
    Q_OBJECT
public:
    static BitmapFontLib &Instance();

    // ════════════════════════════════════════════
    // 颜色过滤器
    // ════════════════════════════════════════════
    void setColorFilter(const BflColorFilter &filter);
    BflColorFilter colorFilter() const;
    BflColorFilter &colorFilterRef();

    // ════════════════════════════════════════════
    // 二值化 — 基于颜色过滤（替代固定阈值）
    // ════════════════════════════════════════════

    /// 根据颜色过滤器将ROI二值化（用全局 m_colorFilter）
    /// roi: 输入彩色图片（BGR/BGRA均可）
    /// 返回: 前景=255，背景=0
    cv::Mat binarize(const cv::Mat &roi) const;

    /// 根据指定颜色过滤器将ROI二值化（用于字形独立颜色）
    cv::Mat binarizeWith(const cv::Mat &roi, const BflColorFilter &filter) const;

    // ════════════════════════════════════════════
    // 点阵转换（大漠格式）
    // ════════════════════════════════════════════

    /// 二值图 → 十六进制点阵（大漠格式）
    /// 按行从上到下、列从左到右逐位排列
    /// 每行宽度÷4余数补0对齐（大漠规则）
    static std::string matToHex(const cv::Mat &binary);

    /// 十六进制点阵 → 二值图
    static cv::Mat hexToMat(const std::string &hex, int width, int height);

    /// 比较两个点阵的相似度
    /// 用XOR算不同位数，1.0 - (不同位数 / 总位数)
    static double compareBits(const std::string &a, const std::string &b);

    // ════════════════════════════════════════════
    // 字符分割
    // ════════════════════════════════════════════

    /// 纵向投影切字：扫描每列像素和，空列作为分隔符
    static std::vector<cv::Rect> segmentChars(const cv::Mat &binaryLine, int minGapPixels = 2);

    // ════════════════════════════════════════════
    // 字库管理
    // ════════════════════════════════════════════

    /// 从 .bfl 文件加载字库（大漠文本格式）
    bool load(const QString &path);

    /// 保存到 .bfl 文件（大漠文本格式）
    bool save(const QString &path) const;

    int charCount() const { return m_glyphs.size(); }
    void clear();
    bool isEmpty() const { return m_glyphs.isEmpty(); }

    // ════════════════════════════════════════════
    // 训练
    // ════════════════════════════════════════════

    /// 添加单个字符样本
    /// charName: 字符名（UTF-8）
    /// binaryChar: 二值化后的单字符区域（前景=255）
    void addChar(const std::string &charName, const cv::Mat &binaryChar);

    /// 删除字符的所有样本
    void removeChar(const std::string &charName);

    // ════════════════════════════════════════════
    // 查找/识别（大漠风格 FindStr）
    // ════════════════════════════════════════════

    /// 在彩色图像中查找所有字库字符（每个字形用自己的颜色过滤器做binarize）
    /// image: 彩色搜索区域（BGR/BGRA）
    /// sim: 相似度阈值（0.0~1.0）
    std::vector<BflFindResult> findString(const cv::Mat &image, double sim = 0.9) const;

    /// 在彩色图像中查找指定名称的字符
    std::vector<BflFindResult> findChar(const std::string &charName, const cv::Mat &image, double sim = 0.9) const;

    /// 在已二值化图像中查找所有字库字符（旧接口，用外部binarize的结果）
    std::vector<BflFindResult> findStringBinary(const cv::Mat &binary, double sim = 0.9) const;

    /// 在已二值化图像中查找指定名称的字符（旧接口）
    std::vector<BflFindResult> findCharBinary(const std::string &charName, const cv::Mat &binary, double sim = 0.9) const;

    // ════════════════════════════════════════════
    // 兼容旧API
    // ════════════════════════════════════════════

    /// 从标注文字行训练（自动切字）
    int trainFromLine(const cv::Mat &binaryLine, const QString &text);

    /// 识别一行文字（自动切字+逐个比对）
    QString recognizeText(const cv::Mat &binaryLine) const;

signals:
    void fontLibChanged();

private:
    BitmapFontLib() = default;

    /// 内部：从二值图构造BflGlyph
    static BflGlyph makeGlyph(const cv::Mat &binaryChar);

    BflColorFilter m_colorFilter;

    // 每个字符名 → 多个样本（支持同一字符的变体）
    QHash<QString, QVector<BflGlyph>> m_glyphs;
};

#endif // BITMAPFONTLIB_H
