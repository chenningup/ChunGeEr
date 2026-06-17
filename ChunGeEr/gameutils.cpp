#include "gameutils.h"
#include "Ocr/ocrmnager.h"
#include <QDir>
#include <QFileInfo>

GameUtils &GameUtils::Instance()
{
    static GameUtils inst;
    return inst;
}

void GameUtils::setTemplateRoot(const QString &root)
{
    m_templateRoot = root;
}

// ════════════════════════════════════════════════
// 裁剪固定区域
// ════════════════════════════════════════════════
cv::Mat GameUtils::cropROI(const cv::Mat &frame, const QRect &roi)
{
    if (frame.empty() || roi.isEmpty()) return {};
    QRect safe = roi.intersected(QRect(0, 0, frame.cols, frame.rows));
    if (safe.isEmpty()) return {};
    cv::Rect sr(safe.x(), safe.y(), safe.width(), safe.height());
    return frame(sr).clone();
}

// ════════════════════════════════════════════════
// 通用最佳匹配
// ════════════════════════════════════════════════
GameUtils::MatchResult GameUtils::bestMatch(const cv::Mat &frame, const QString &templateDir, const QString &nameFilter)
{
    MatchResult best;
    best.confidence = 0;
    QDir dir(templateDir);
    if (!dir.exists() || frame.empty()) return best;

    QStringList filters = {"*.png", "*.jpg", "*.bmp"};
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files);

    for (const QFileInfo &fi : files) {
        if (!nameFilter.isEmpty() && !fi.baseName().startsWith(nameFilter)) continue;
        cv::Mat templ = cv::imread(fi.absoluteFilePath().toLocal8Bit().toStdString());
        if (templ.empty()) continue;
        if (templ.cols > frame.cols || templ.rows > frame.rows) continue;

        cv::Mat result;
        cv::matchTemplate(frame, templ, result, cv::TM_CCOEFF_NORMED);

        double maxVal;
        cv::Point maxLoc;
        cv::minMaxLoc(result, nullptr, &maxVal, nullptr, &maxLoc);

        if (maxVal > best.confidence && maxVal >= m_matchThreshold) {
            best.name = fi.baseName();
            best.confidence = maxVal;
            best.centerX = maxLoc.x + templ.cols / 2;
            best.centerY = maxLoc.y + templ.rows / 2;
        }
    }
    return best;
}

// ════════════════════════════════════════════════
// 当前地图
// ════════════════════════════════════════════════
GameUtils::MatchResult GameUtils::detectLocation(const cv::Mat &frame)
{
    cv::Mat roi = cropROI(frame, m_roiLocation);
    return bestMatch(roi, m_templateRoot + "/locations");
}

// ════════════════════════════════════════════════
// 角色等级（模板匹配）
// ════════════════════════════════════════════════
GameUtils::MatchResult GameUtils::detectLevel(const cv::Mat &frame)
{
    cv::Mat roi = cropROI(frame, m_roiLevel);
    return bestMatch(roi, m_templateRoot + "/levels");
}

// ════════════════════════════════════════════════
// 当前主线任务
// ════════════════════════════════════════════════
GameUtils::MatchResult GameUtils::detectMainQuest(const cv::Mat &frame)
{
    cv::Mat roi = cropROI(frame, m_roiMainQuest);
    return bestMatch(roi, m_templateRoot + "/mainquests");
}

// ════════════════════════════════════════════════
// 技能栏
// ════════════════════════════════════════════════
QStringList GameUtils::detectSkills(const cv::Mat &frame)
{
    QStringList skills;
    cv::Mat roi = cropROI(frame, m_roiSkills);
    if (roi.empty()) return skills;

    int slotCount = 8;
    int slotWidth = roi.cols / slotCount;

    for (int i = 0; i < slotCount; i++) {
        cv::Rect slot(i * slotWidth, 0, slotWidth, roi.rows);
        cv::Mat slotImg = roi(slot).clone();

        // 对每个格子单独做模板匹配
        QDir dir(m_templateRoot + "/skills");
        QStringList filters = {"*.png", "*.jpg", "*.bmp"};
        QFileInfoList files = dir.entryInfoList(filters, QDir::Files);

        double bestVal = 0;
        QString bestName;

        for (const QFileInfo &fi : files) {
            cv::Mat templ = cv::imread(fi.absoluteFilePath().toLocal8Bit().toStdString());
            if (templ.empty()) continue;
            // resize template to slot size for fair comparison
            if (templ.cols != slotWidth || templ.rows != roi.rows) {
                cv::resize(templ, templ, cv::Size(slotWidth, roi.rows));
            }

            cv::Mat result;
            cv::matchTemplate(slotImg, templ, result, cv::TM_CCOEFF_NORMED);
            double v;
            cv::minMaxLoc(result, nullptr, &v, nullptr, nullptr);
            if (v > bestVal && v >= m_matchThreshold) {
                bestVal = v;
                bestName = fi.baseName();
            }
        }
        skills.append(bestName.isEmpty() ? "?" : bestName);
    }
    return skills;
}
