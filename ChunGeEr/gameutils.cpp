#include "gameutils.h"
#include "Ocr/ocrmnager.h"
#include "XuLog.h"
#include <QDir>
#include <QFileInfo>
#include <QCoreApplication>
#include <QSettings>
#include <windows.h>

GameUtils &GameUtils::Instance()
{
    static GameUtils inst;
    return inst;
}

// ════════════════════════════════════════════════
// 从config.ini加载所有ROI（窗口相对坐标）
// ════════════════════════════════════════════════
void GameUtils::loadROIs()
{
    QString configPath = QCoreApplication::applicationDirPath() + "/config.ini";
    QSettings settings(configPath, QSettings::IniFormat);
    settings.beginGroup("ROIs");

    auto parse = [&](const QString &key) -> QRect {
        QString val = settings.value(key).toString();
        auto parts = val.split(',');
        if (parts.size() == 4)
            return QRect(parts[0].toInt(), parts[1].toInt(), parts[2].toInt(), parts[3].toInt());
        return QRect();
    };

    m_roiLocation   = parse("Location");
    m_roiLevel      = parse("Level");
    m_roiSkills     = parse("Skills");
    m_roiMainQuest  = parse("MainQuest");
    m_roiDisconnect = parse("Disconnect");
    m_roiStopped    = parse("Stopped");
    m_roiSettingsPanel = parse("SettingsPanel");

    settings.endGroup();

    infof("[GU] ROIs loaded: location={}, level={}, skills={}",
        m_roiLocation.isEmpty() ? "N/A" : QString("%1x%2").arg(m_roiLocation.width()).arg(m_roiLocation.height()).toStdString(),
        m_roiLevel.isEmpty() ? "N/A" : QString("%1x%2").arg(m_roiLevel.width()).arg(m_roiLevel.height()).toStdString(),
        m_roiSkills.isEmpty() ? "N/A" : QString("%1x%2").arg(m_roiSkills.width()).arg(m_roiSkills.height()).toStdString());
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

    // ROI存的是窗口相对坐标，需叠加游戏窗口屏幕位置
    int winX = 0, winY = 0;
    HWND hwnd = FindWindowW(nullptr, L"\u5927\u5510\u65e0\u53cc\u516c\u6d4b - \u4e03\u4fa0\u4e94\u4e49 (4.0.58:1041281  1.0.5:1039767)");
    if (!hwnd) {
        // 模糊匹配
        HWND h = FindWindowW(nullptr, nullptr);
        while (h) {
            wchar_t title[256];
            GetWindowTextW(h, title, 256);
            if (wcsstr(title, L"\u5927\u5510\u65e0\u53cc")) { hwnd = h; break; }
            h = GetWindow(h, GW_HWNDNEXT);
        }
    }
    if (hwnd) {
        RECT wr;
        if (GetWindowRect(hwnd, &wr)) {
            winX = wr.left;
            winY = wr.top;
        }
    }

    // 窗口相对 → 屏幕绝对
    QRect screenROI(roi.x() + winX, roi.y() + winY, roi.width(), roi.height());
    QRect safe = screenROI.intersected(QRect(0, 0, frame.cols, frame.rows));
    if (safe.isEmpty()) return {};
    cv::Rect sr(safe.x(), safe.y(), safe.width(), safe.height());
    return frame(sr).clone();
}

cv::Mat GameUtils::loadCachedTemplate(const QString &path)
{
    auto it = m_templateCache.find(path);
    if (it != m_templateCache.end())
        return it.value();
    cv::Mat templ = cv::imread(path.toLocal8Bit().toStdString());
    if (!templ.empty())
        m_templateCache.insert(path, templ);
    return templ;
}

// ════════════════════════════════════════════════
// 通用最佳匹配
// ════════════════════════════════════════════════
GameUtils::MatchResult GameUtils::bestMatch(const cv::Mat &frame, const QString &templateDir, const QString &nameFilter)
{
    MatchResult best;
    best.confidence = 0;
    QDir dir(templateDir);
    if (!dir.exists()) {
        infof("[GU] bestMatch: dir NOT exist {}", templateDir.toStdString());
        return best;
    }
    if (frame.empty()) {
        infof("[GU] bestMatch: frame EMPTY");
        return best;
    }

    QStringList filters = {"*.png", "*.jpg", "*.bmp"};
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files);
    infof("[GU] bestMatch: dir={} filter={} files={} frame={}x{}",
          templateDir.toStdString(), nameFilter.toStdString(), files.size(), frame.cols, frame.rows);

    for (const QFileInfo &fi : files) {
        if (!nameFilter.isEmpty() && !fi.baseName().startsWith(nameFilter)) continue;
        cv::Mat templ = loadCachedTemplate(fi.absoluteFilePath());
        if (templ.empty()) {
            infof("[GU]   load FAILED {}", fi.baseName().toStdString());
            continue;
        }
        if (templ.cols > frame.cols || templ.rows > frame.rows) {
            infof("[GU]   too big {} ({}x{}) > frame ({}x{})",
                  fi.baseName().toStdString(), templ.cols, templ.rows, frame.cols, frame.rows);
            continue;
        }

        cv::Mat result;
        cv::matchTemplate(frame, templ, result, cv::TM_CCOEFF_NORMED);

        double maxVal;
        cv::Point maxLoc;
        cv::minMaxLoc(result, nullptr, &maxVal, nullptr, &maxLoc);

        infof("[GU]   {} conf={:.3f} at=({},{}) templ={}x{}",
              fi.baseName().toStdString(), maxVal, maxLoc.x, maxLoc.y, templ.cols, templ.rows);

        if (maxVal > best.confidence && maxVal >= m_matchThreshold) {
            best.name = fi.baseName();
            best.confidence = maxVal;
            best.centerX = maxLoc.x + templ.cols / 2;
            best.centerY = maxLoc.y + templ.rows / 2;
        }
    }
    infof("[GU] bestMatch result: name={} conf={:.3f} center=({},{})",
          best.name.toStdString(), best.confidence, best.centerX, best.centerY);
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

    QDir dir(m_templateRoot + "/skills");
    QStringList filters = {"*.png", "*.jpg", "*.bmp"};
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files);

    // 预加载所有技能模板
    QList<QPair<QString, cv::Mat>> templates;
    for (const QFileInfo &fi : files) {
        cv::Mat templ = loadCachedTemplate(fi.absoluteFilePath());
        if (templ.empty()) continue;
        templates.append({fi.baseName(), templ});
    }

    for (int i = 0; i < slotCount; i++) {
        cv::Rect slot(i * slotWidth, 0, slotWidth, roi.rows);
        cv::Mat slotImg = roi(slot).clone();

        double bestVal = 0;
        QString bestName;

        for (const auto &tp : templates) {
            cv::Mat templ = tp.second;
            if (templ.cols != slotWidth || templ.rows != roi.rows) {
                cv::resize(templ, templ, cv::Size(slotWidth, roi.rows));
            }

            cv::Mat result;
            cv::matchTemplate(slotImg, templ, result, cv::TM_CCOEFF_NORMED);
            double v;
            cv::minMaxLoc(result, nullptr, &v, nullptr, nullptr);
            if (v > bestVal && v >= m_matchThreshold) {
                bestVal = v;
                bestName = tp.first;
            }
        }
        skills.append(bestName.isEmpty() ? "?" : bestName);
    }
    return skills;
}
