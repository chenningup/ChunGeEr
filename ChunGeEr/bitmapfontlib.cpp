#include "bitmapfontlib.h"
#include "XuLog.h"
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <intrin.h>

// ════════════════════════════════════════════════
// 单例
// ════════════════════════════════════════════════
BitmapFontLib &BitmapFontLib::Instance()
{
    static BitmapFontLib inst;
    return inst;
}

// ════════════════════════════════════════════════
// 颜色过滤器
// ════════════════════════════════════════════════
void BitmapFontLib::setColorFilter(const BflColorFilter &filter)
{
    m_colorFilter = filter;
}

BflColorFilter BitmapFontLib::colorFilter() const
{
    return m_colorFilter;
}

BflColorFilter &BitmapFontLib::colorFilterRef()
{
    return m_colorFilter;
}

// ════════════════════════════════════════════════
// 二值化 — 基于颜色过滤
// 遍历每个像素，任一取样颜色匹配→前景(255)
// 范围外→背景(0)
// ════════════════════════════════════════════════
cv::Mat BitmapFontLib::binarize(const cv::Mat &roi) const
{
    return binarizeWith(roi, m_colorFilter);
}

// ════════════════════════════════════════════════
// 二值化 — 指定颜色过滤器
// ════════════════════════════════════════════════
cv::Mat BitmapFontLib::binarizeWith(const cv::Mat &roi, const BflColorFilter &filter) const
{
    if (roi.empty() || filter.isEmpty()) return {};

    // 转到BGR
    cv::Mat bgr;
    if (roi.channels() == 4)
        cv::cvtColor(roi, bgr, cv::COLOR_BGRA2BGR);
    else if (roi.channels() == 3)
        bgr = roi.clone();
    else if (roi.channels() == 1) {
        cv::cvtColor(roi, bgr, cv::COLOR_GRAY2BGR);
    } else {
        return {};
    }

    cv::Mat binary(roi.rows, roi.cols, CV_8UC1, cv::Scalar(0));

    for (int y = 0; y < roi.rows; y++) {
        const cv::Vec3b *row = bgr.ptr<cv::Vec3b>(y);
        uint8_t *out = binary.ptr<uint8_t>(y);
        for (int x = 0; x < roi.cols; x++) {
            int b = row[x][0], g = row[x][1], r = row[x][2];
            for (const auto &pt : filter.points) {
                int tr = pt.color.red(), tg = pt.color.green(), tb = pt.color.blue();
                int br = pt.bias.red(), bg = pt.bias.green(), bb = pt.bias.blue();
                if (abs(r - tr) <= br && abs(g - tg) <= bg && abs(b - tb) <= bb) {
                    out[x] = 255;
                    break;
                }
            }
        }
    }

    return binary;
}

// ════════════════════════════════════════════════
// 二值图 → 十六进制点阵（大漠格式）
//
// 规则：
//   1. 按行从上到下、每行从左到右逐像素排列
//   2. 像素>0 → bit=1，=0 → bit=0
//   3. 每行宽度÷4的余数补0对齐
//   4. 每4位转1个十六进制字符
//
// 示例（5宽×11高的"1"字）：
//   00100
//   11100
//   00100 → 点阵二进制: 01000 00000 10100 00000 ...
//   ...
//   11111
// → 十六进制: 402807FF801002
// ════════════════════════════════════════════════
std::string BitmapFontLib::matToHex(const cv::Mat &binary)
{
    if (binary.empty()) return {};

    int w = binary.cols, h = binary.rows;

    // 每行宽度÷4余数 → 需要补几个0对齐
    int pad = (4 - (w % 4)) % 4;

    // 构建位字符串
    std::string bits;
    bits.reserve(h * (w + pad));

    for (int y = 0; y < h; y++) {
        const uint8_t *row = binary.ptr<uint8_t>(y);
        for (int x = 0; x < w; x++) {
            bits.push_back(row[x] > 0 ? '1' : '0');
        }
        // 补0对齐
        for (int p = 0; p < pad; p++) {
            bits.push_back('0');
        }
    }

    // 每4位转1个hex字符
    std::string hex;
    hex.reserve(bits.size() / 4);

    for (size_t i = 0; i + 3 < bits.size(); i += 4) {
        int val = 0;
        if (bits[i] == '1')     val |= 8;
        if (bits[i + 1] == '1') val |= 4;
        if (bits[i + 2] == '1') val |= 2;
        if (bits[i + 3] == '1') val |= 1;
        hex.push_back("0123456789ABCDEF"[val]);
    }

    return hex;
}

// ════════════════════════════════════════════════
// 十六进制点阵 → 二值图（matToHex的逆操作）
// ════════════════════════════════════════════════
cv::Mat BitmapFontLib::hexToMat(const std::string &hex, int width, int height)
{
    if (hex.empty() || width <= 0 || height <= 0) return {};

    cv::Mat binary(height, width, CV_8UC1, cv::Scalar(0));

    int pad = (4 - (width % 4)) % 4;
    int rowBits = width + pad;

    for (int y = 0; y < height; y++) {
        uint8_t *row = binary.ptr<uint8_t>(y);
        for (int x = 0; x < width; x++) {
            int bitIdx = y * rowBits + x;
            int hexIdx = bitIdx / 4;
            int bitOff = 3 - (bitIdx % 4); // hex字符高位在前

            if (hexIdx < (int)hex.size()) {
                char c = hex[hexIdx];
                int val = 0;
                if (c >= '0' && c <= '9') val = c - '0';
                else if (c >= 'A' && c <= 'F') val = c - 'A' + 10;
                else if (c >= 'a' && c <= 'f') val = c - 'a' + 10;

                if (val & (1 << bitOff)) {
                    row[x] = 255;
                }
            }
        }
    }

    return binary;
}

// ════════════════════════════════════════════════
// 比较两个点阵的相似度
// 把hex转成二进制 → XOR求不同位数 → 1.0 - 差异比例
// ════════════════════════════════════════════════
double BitmapFontLib::compareBits(const std::string &a, const std::string &b)
{
    if (a.empty() && b.empty()) return 1.0;
    if (a.empty() || b.empty()) return 0.0;

    // 先解hex为字节数组
    auto hexToBytes = [](const std::string &hex) -> std::vector<uint8_t> {
        std::vector<uint8_t> bytes;
        bytes.reserve(hex.size());
        for (char c : hex) {
            if (c >= '0' && c <= '9') bytes.push_back(c - '0');
            else if (c >= 'A' && c <= 'F') bytes.push_back(c - 'A' + 10);
            else if (c >= 'a' && c <= 'f') bytes.push_back(c - 'a' + 10);
            else bytes.push_back(0);
        }
        return bytes;
    };

    auto ab = hexToBytes(a);
    auto bb = hexToBytes(b);

    // 逐4位比较
    int totalBits = qMin((int)ab.size(), (int)bb.size()) * 4;
    if (totalBits == 0) return 0.0;

    int diffBits = 0;
    int n = qMin((int)ab.size(), (int)bb.size());
    for (int i = 0; i < n; i++) {
        uint8_t diff = ab[i] ^ bb[i];
        diffBits += __popcnt16(diff);  // 每个hex是4位
    }

    // 超出部分全算不同
    int extraA = 0, extraB = 0;
    for (int i = n; i < (int)ab.size(); i++) extraA += __popcnt16(ab[i]);
    for (int i = n; i < (int)bb.size(); i++) extraB += __popcnt16(bb[i]);
    diffBits += extraA + extraB;
    totalBits += (extraA > 0 ? (ab.size() - n) * 4 : 0)
               + (extraB > 0 ? (bb.size() - n) * 4 : 0);

    return 1.0 - (double)diffBits / totalBits;
}

// ════════════════════════════════════════════════
// 纵向投影切字
// ════════════════════════════════════════════════
std::vector<cv::Rect> BitmapFontLib::segmentChars(const cv::Mat &binaryLine, int minGapPixels)
{
    std::vector<cv::Rect> chars;
    if (binaryLine.empty() || binaryLine.cols < 1) return chars;

    int w = binaryLine.cols;
    int h = binaryLine.rows;

    // 纵向投影
    std::vector<int> proj(w, 0);
    for (int x = 0; x < w; x++) {
        int sum = 0;
        for (int y = 0; y < h; y++) {
            if (binaryLine.at<uint8_t>(y, x) > 0) sum++;
        }
        proj[x] = sum;
    }

    int start = -1;
    for (int x = 0; x < w; x++) {
        if (proj[x] > 0) {
            if (start < 0) start = x;
        } else {
            if (start >= 0) {
                bool realGap = true;
                for (int g = 0; g < minGapPixels && (x + g) < w; g++) {
                    if (proj[x + g] > 0) { realGap = false; break; }
                }
                if (realGap) {
                    chars.push_back(cv::Rect(start, 0, x - start, h));
                    start = -1;
                }
            }
        }
    }
    if (start >= 0) {
        chars.push_back(cv::Rect(start, 0, w - start, h));
    }

    return chars;
}

// ════════════════════════════════════════════════
// 内部：二值图 → BflGlyph
// ════════════════════════════════════════════════
BflGlyph BitmapFontLib::makeGlyph(const cv::Mat &binaryChar)
{
    BflGlyph g;
    if (binaryChar.empty()) return g;

    g.width = binaryChar.cols;
    g.height = binaryChar.rows;
    g.hexBits = matToHex(binaryChar);
    g.effectivePixels = cv::countNonZero(binaryChar);

    return g;
}

// ════════════════════════════════════════════════
// 文件格式（大漠兼容文本，扩展了width字段）
// 每行: hexBits$charName$effectivePixels$width$height
//
// 兼容旧4字段格式（缺width时由height反推）
// （注意补0对齐：实际 width = floor(hex.length()*4 / height)）
//
// 文件头（可选注释行，以#开头）：
//   # BFL font library, color: FEFEFE bias: 303030
// ════════════════════════════════════════════════

bool BitmapFontLib::load(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        infof("[BFL] load FAILED: cannot open {}", path.toStdString());
        return false;
    }

    m_glyphs.clear();
    m_colorFilter.clear();

    QTextStream ts(&file);
    // Qt6默认UTF-8，无需setCodec

    int lineNo = 0;
    while (!ts.atEnd()) {
        QString line = ts.readLine().trimmed();
        lineNo++;

        // 跳过空行
        if (line.isEmpty()) continue;

        // 解析颜色注释行: # BFL color: e52222 bias: 303030
        if (line.startsWith("# BFL color:")) {
            // 格式: # BFL color: RRGGBB bias: RRGGBB
            QString rest = line.mid(QString("# BFL color:").length()).trimmed();
            QStringList parts2 = rest.split("bias:");
            if (parts2.size() >= 2) {
                QString colorHex = parts2[0].trimmed();
                QString biasHex = parts2[1].trimmed();
                auto hexToColor = [](const QString &hex) -> QColor {
                    if (hex.length() < 6) return QColor();
                    int r = hex.mid(0,2).toInt(nullptr,16);
                    int g = hex.mid(2,2).toInt(nullptr,16);
                    int b = hex.mid(4,2).toInt(nullptr,16);
                    return QColor(r,g,b);
                };
                BflColorPoint pt;
                pt.color = hexToColor(colorHex);
                pt.bias = hexToColor(biasHex);
                m_colorFilter.points.append(pt);
                infof("[BFL] load: restored color R{} G{} B{} bias R{} G{} B{}",
                      pt.color.red(), pt.color.green(), pt.color.blue(),
                      pt.bias.red(), pt.bias.green(), pt.bias.blue());
            }
            continue;
        }

        // 跳过其他注释
        if (line.startsWith('#')) continue;

        // 旧格式兼容：跳过BFL1 magic标记
        if (line.startsWith("BFL1")) continue;

        QStringList parts = line.split('$');
        if (parts.size() < 4) {
            infof("[BFL] load: skip line {} (only {} parts)", lineNo, parts.size());
            continue;
        }

        BflGlyph g;
        g.hexBits = parts[0].toStdString();
        g.effectivePixels = parts[2].toInt();

        // 5字段格式: hexBits$charName$effectivePixels$width$height
        // 6字段格式: hexBits$charName$effectivePixels$width$height$colors
        if (parts.size() >= 5) {
            g.width = parts[3].toInt();
            g.height = parts[4].toInt();
            // 6字段：解析字形独立颜色
            if (parts.size() >= 6) {
                // 格式: R,G,B|r,b,g|R,G,B|r,b,g|...
                // 鏍煎紡: R,G,B,biasR,biasG,biasB,R,G,B,biasR,biasG,biasB,...
                // 鍏煎 silde format: R,G,B|biasR,biasG,biasB|R,G,B|biasR,biasG,biasB
                QStringList nums;
                if (parts[5].contains('|')) {
                    // 鏃ф牸寮? 鐢?| 鍒嗛殧锛屾娊鍑烘墍鏈夋暟瀛?
                    QStringList entries = parts[5].split('|');
                    for (const QString &e : entries) {
                        nums << e.split(',');
                    }
                } else {
                    nums = parts[5].split(',');
                }
                for (int ci = 0; ci + 5 < nums.size(); ci += 6) {
                    BflColorPoint pt;
                    pt.color = QColor(nums[ci].toInt(), nums[ci+1].toInt(), nums[ci+2].toInt());
                    pt.bias = QColor(nums[ci+3].toInt(), nums[ci+4].toInt(), nums[ci+5].toInt());
                    g.colorFilter.points.append(pt);
                }
            }
        } else {
            g.height = parts[3].toInt();
            // 从hex长度和height反推宽度（包含补0对齐，略大于真实宽度）
            int totalBits = (int)g.hexBits.size() * 4;
            g.width = (g.height > 0) ? totalBits / g.height : 0;
        }

        QString charName = parts[1];
        m_glyphs[charName].append(g);
    }

    infof("[BFL] loaded: {} chars from {}", m_glyphs.size(), path.toStdString());
    return true;
}

bool BitmapFontLib::save(const QString &path) const
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        infof("[BFL] save FAILED: cannot write {}", path.toStdString());
        return false;
    }

    QTextStream ts(&file);
    // Qt6默认UTF-8，无需setCodec

    // 文件头注释（全局颜色，向后兼容）
    QStringList colorLines;
    for (const auto &pt : m_colorFilter.points) {
        colorLines.append(QString("# BFL color: %1%2%3 bias: %4%5%6")
            .arg(pt.color.red(), 2, 16, QChar('0'))
            .arg(pt.color.green(), 2, 16, QChar('0'))
            .arg(pt.color.blue(), 2, 16, QChar('0'))
            .arg(pt.bias.red(), 2, 16, QChar('0'))
            .arg(pt.bias.green(), 2, 16, QChar('0'))
            .arg(pt.bias.blue(), 2, 16, QChar('0')));
    }
    ts << colorLines.join("\n") << "\n";

    for (auto it = m_glyphs.begin(); it != m_glyphs.end(); ++it) {
        const QString &charName = it.key();
        for (const BflGlyph &g : it.value()) {
            ts << QString::fromStdString(g.hexBits)
               << "$" << charName
               << "$" << g.effectivePixels
               << "$" << g.width
               << "$" << g.height;
            // 写入字形独立颜色（6字段格式）
            if (!g.colorFilter.points.isEmpty()) {
                QStringList colorStrs;
                for (const auto &pt : g.colorFilter.points) {
                    colorStrs.append(QString("%1,%2,%3,%4,%5,%6")
                        .arg(pt.color.red()).arg(pt.color.green()).arg(pt.color.blue())
                        .arg(pt.bias.red()).arg(pt.bias.green()).arg(pt.bias.blue()));
                }
                ts << "$" << colorStrs.join("|");
            }
            ts << "\n";
        }
    }

    int glyphCount = 0;
    for (const auto &v : m_glyphs) glyphCount += v.size();
    infof("[BFL] saved: {} chars ({} glyphs) to {}", m_glyphs.size(), glyphCount, path.toStdString());
    return true;
}

void BitmapFontLib::clear()
{
    m_glyphs.clear();
    emit fontLibChanged();
}

// ════════════════════════════════════════════════
// 添加字符样本
// ════════════════════════════════════════════════
void BitmapFontLib::addChar(const std::string &charName, const cv::Mat &binaryChar)
{
    if (binaryChar.empty() || charName.empty()) return;

    BflGlyph g = makeGlyph(binaryChar);
    g.colorFilter = m_colorFilter;  // 训练时存当前全局颜色过滤器到字形
    QString key = QString::fromStdString(charName);
    m_glyphs[key].append(g);

    infof("[BFL] addChar: '{}' {}x{} ep={} hex_len={} total_samples={}",
          charName, g.width, g.height, g.effectivePixels,
          g.hexBits.size(), m_glyphs[key].size());

    emit fontLibChanged();
}

void BitmapFontLib::removeChar(const std::string &charName)
{
    m_glyphs.remove(QString::fromStdString(charName));
    emit fontLibChanged();
}

// ════════════════════════════════════════════════
// 兼容旧API：从标注文字行训练
// ════════════════════════════════════════════════
int BitmapFontLib::trainFromLine(const cv::Mat &binaryLine, const QString &text)
{
    auto chars = segmentChars(binaryLine);
    if (chars.size() != text.size()) {
        infof("[BFL] trainFromLine: segment count ({}) != text length ({})",
              chars.size(), text.size());
        return 0;
    }

    int added = 0;
    for (int i = 0; i < (int)chars.size(); i++) {
        cv::Mat charRoi = binaryLine(chars[i]).clone();
        addChar(QString(text[i]).toUtf8().toStdString(), charRoi);
        added++;
    }
    return added;
}

// ════════════════════════════════════════════════
// 寻找：每个字形用自己的颜色过滤器做binarize
//
// 对每个字库中的字形：
//   1. 若字形有独立colorFilter，用它binarize原图；否则用全局m_colorFilter
//   2. 以字形尺寸为窗口滑动
//   3. 每个窗口提取 → 转hex → compareBits比对
//   4. 相似度 >= sim 的记录下来
// ════════════════════════════════════════════════
std::vector<BflFindResult> BitmapFontLib::findString(const cv::Mat &image, double sim) const
{
    std::vector<BflFindResult> results;
    if (image.empty() || m_glyphs.isEmpty()) return results;

    int imgW = image.cols, imgH = image.rows;

    infof("[BFL] findString: image {}x{}, glyphs={}", imgW, imgH, m_glyphs.size());
    for (auto it = m_glyphs.begin(); it != m_glyphs.end(); ++it) {
        const std::string &charName = it.key().toStdString();
        for (const BflGlyph &g : it.value()) {
            infof("[BFL]  glyph: name={} w={} h={} hexLen={} colorPoints={}",
                  charName, g.width, g.height, g.hexBits.size(), g.colorFilter.points.size());
            if (g.width > imgW || g.height > imgH) {
                infof("[BFL]   skip: glyph {}x{} > image {}x{}", g.width, g.height, imgW, imgH);
                continue;
            }

            // 用字形独立颜色binarize；没有则用全局
            const BflColorFilter &cf = g.colorFilter.isEmpty() ? m_colorFilter : g.colorFilter;
            infof("[BFL]   cf points={} (global={})", cf.points.size(), g.colorFilter.isEmpty());
            cv::Mat binary = binarizeWith(image, cf);
            if (binary.empty()) {
                infof("[BFL]   binarize returned empty!");
                continue;
            }
            int nonZero = cv::countNonZero(binary);
            infof("[BFL]   binarize ok: nonZero={}/{}", nonZero, binary.total());
            // 调试：保存原图和二值化图
            static int dbgIdx = 0;
            if (dbgIdx < 5) {
                std::string prefix = "debug_bfl_" + std::to_string(dbgIdx);
                cv::imwrite(prefix + "_orig.png", image);
                cv::imwrite(prefix + "_binary.png", binary);
                infof("[BFL]   saved debug images: {}_orig.png, {}_binary.png", prefix, prefix);
                dbgIdx++;
            }

            int maxX = imgW - g.width;
            int maxY = imgH - g.height;

            for (int y = 0; y <= maxY; y++) {
                for (int x = 0; x <= maxX; x++) {
                    cv::Rect roi(x, y, g.width, g.height);
                    std::string windowHex = matToHex(binary(roi));
                    double score = compareBits(windowHex, g.hexBits);

                    if (score >= sim) {
                        BflFindResult r;
                        r.charName = charName;
                        r.x = x;
                        r.y = y;
                        r.width = g.width;
                        r.height = g.height;
                        r.similarity = score;
                        results.push_back(r);
                    }
                }
            }
        }
    }

    // NMS: 同一字形的重叠匹配（IoU > 0.5）只保留最高分
    std::sort(results.begin(), results.end(),
        [](const BflFindResult &a, const BflFindResult &b) { return a.similarity > b.similarity; });
    std::vector<BflFindResult> nms;
    std::vector<bool> suppressed(results.size(), false);
    for (size_t i = 0; i < results.size(); i++) {
        if (suppressed[i]) continue;
        nms.push_back(results[i]);
        for (size_t j = i + 1; j < results.size(); j++) {
            if (suppressed[j]) continue;
            if (results[i].charName != results[j].charName) continue;
            // 计算IoU
            int x1 = std::max(results[i].x, results[j].x);
            int y1 = std::max(results[i].y, results[j].y);
            int x2 = std::min(results[i].x + results[i].width, results[j].x + results[j].width);
            int y2 = std::min(results[i].y + results[i].height, results[j].y + results[j].height);
            if (x2 <= x1 || y2 <= y1) continue;
            int intersect = (x2 - x1) * (y2 - y1);
            int areaI = results[i].width * results[i].height;
            int areaJ = results[j].width * results[j].height;
            double iou = (double)intersect / (areaI + areaJ - intersect);
            if (iou > 0.5) suppressed[j] = true;
        }
    }
    return nms;
}

std::vector<BflFindResult> BitmapFontLib::findChar(const std::string &charName, const cv::Mat &image, double sim) const
{
    std::vector<BflFindResult> results;
    if (image.empty()) return results;

    QString key = QString::fromStdString(charName);
    auto it = m_glyphs.find(key);
    if (it == m_glyphs.end()) return results;

    int imgW = image.cols, imgH = image.rows;

    for (const BflGlyph &g : it.value()) {
        if (g.width > imgW || g.height > imgH) continue;

        const BflColorFilter &cf = g.colorFilter.isEmpty() ? m_colorFilter : g.colorFilter;
        cv::Mat binary = binarizeWith(image, cf);
        if (binary.empty()) continue;

        int maxX = imgW - g.width;
        int maxY = imgH - g.height;

        for (int y = 0; y <= maxY; y++) {
            for (int x = 0; x <= maxX; x++) {
                cv::Rect roi(x, y, g.width, g.height);
                std::string windowHex = matToHex(binary(roi));
                double score = compareBits(windowHex, g.hexBits);

                if (score >= sim) {
                    BflFindResult r;
                    r.charName = charName;
                    r.x = x;
                    r.y = y;
                    r.width = g.width;
                    r.height = g.height;
                    r.similarity = score;
                    results.push_back(r);
                }
            }
        }
    }

    return results;
}

// ════════════════════════════════════════════════
// 旧接口：在已二值化图像中查找（外部binarize）
// ════════════════════════════════════════════════
std::vector<BflFindResult> BitmapFontLib::findStringBinary(const cv::Mat &binary, double sim) const
{
    std::vector<BflFindResult> results;
    if (binary.empty() || m_glyphs.isEmpty()) return results;

    int imgW = binary.cols, imgH = binary.rows;

    for (auto it = m_glyphs.begin(); it != m_glyphs.end(); ++it) {
        const std::string &charName = it.key().toStdString();
        for (const BflGlyph &g : it.value()) {
            if (g.width > imgW || g.height > imgH) continue;

            int maxX = imgW - g.width;
            int maxY = imgH - g.height;

            for (int y = 0; y <= maxY; y++) {
                for (int x = 0; x <= maxX; x++) {
                    cv::Rect roi(x, y, g.width, g.height);
                    std::string windowHex = matToHex(binary(roi));
                    double score = compareBits(windowHex, g.hexBits);

                    if (score >= sim) {
                        BflFindResult r;
                        r.charName = charName;
                        r.x = x;
                        r.y = y;
                        r.width = g.width;
                        r.height = g.height;
                        r.similarity = score;
                        results.push_back(r);
                    }
                }
            }
        }
    }

    return results;
}

std::vector<BflFindResult> BitmapFontLib::findCharBinary(const std::string &charName, const cv::Mat &binary, double sim) const
{
    std::vector<BflFindResult> results;
    if (binary.empty()) return results;

    QString key = QString::fromStdString(charName);
    auto it = m_glyphs.find(key);
    if (it == m_glyphs.end()) return results;

    int imgW = binary.cols, imgH = binary.rows;

    for (const BflGlyph &g : it.value()) {
        if (g.width > imgW || g.height > imgH) continue;

        int maxX = imgW - g.width;
        int maxY = imgH - g.height;

        for (int y = 0; y <= maxY; y++) {
            for (int x = 0; x <= maxX; x++) {
                cv::Rect roi(x, y, g.width, g.height);
                std::string windowHex = matToHex(binary(roi));
                double score = compareBits(windowHex, g.hexBits);

                if (score >= sim) {
                    BflFindResult r;
                    r.charName = charName;
                    r.x = x;
                    r.y = y;
                    r.width = g.width;
                    r.height = g.height;
                    r.similarity = score;
                    results.push_back(r);
                }
            }
        }
    }

    return results;
}

// ════════════════════════════════════════════════
// 兼容旧API：识别一行文字（切字+逐个比对）
// 对每个切出的字符，在字库中找最相似的
// ════════════════════════════════════════════════
QString BitmapFontLib::recognizeText(const cv::Mat &binaryLine) const
{
    if (binaryLine.empty() || m_glyphs.isEmpty()) return {};

    auto chars = segmentChars(binaryLine);
    QString result;

    for (const cv::Rect &r : chars) {
        cv::Mat charRoi = binaryLine(r).clone();

        std::string inputHex = matToHex(charRoi);

        double bestScore = 0.0;
        QString bestChar;

        for (auto it = m_glyphs.begin(); it != m_glyphs.end(); ++it) {
            for (const BflGlyph &g : it.value()) {
                double score = compareBits(inputHex, g.hexBits);
                if (score > bestScore) {
                    bestScore = score;
                    bestChar = it.key();
                }
            }
        }

        if (bestScore > 0.5) {
            result.append(bestChar);
        } else {
            result.append('?');
        }
    }

    return result;
}