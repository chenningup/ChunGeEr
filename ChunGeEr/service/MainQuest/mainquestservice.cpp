#include "mainquestservice.h"
#include "../../LeoControl/mousekeyboardmanager.h"
#include "../../signalslotconnector.h"
#include "XuLog.h"
#include <QThread>
#include <QRandomGenerator>
#include <QSettings>
#include <QCoreApplication>
#include <windows.h>
#include <algorithm>
#include <fstream>
#include <iterator>

MainQuestService::MainQuestService(QObject *parent) : BaseService(parent) {}

void MainQuestService::startService()
{
    questLog("主线任务服务启动");

    // 从 config.ini 加载 ROI
    QString iniPath = QCoreApplication::applicationDirPath() + "/config.ini";
    QSettings s(iniPath, QSettings::IniFormat);
    // if (s.contains("ROIs/MainQuest")) {
    //     QStringList parts = s.value("ROIs/MainQuest").toString().split(',');
    //     if (parts.size() == 4)
    //         questTrackRoi = QRect(parts[0].toInt(), parts[1].toInt(), parts[2].toInt(), parts[3].toInt());
    // }
    if (s.contains("ROIs/MainQuest")) {
        QStringList parts = s.value("ROIs/MainQuest").toString().split(',');
        if (parts.size() == 4)
            questTrackRoi = QRect(parts[0].toInt(), parts[1].toInt(), parts[2].toInt(), parts[3].toInt());
    }
    if (s.contains("ROIs/MapCoord")) {
        QStringList parts = s.value("ROIs/MapCoord").toString().split(',');
        if (parts.size() == 4)
            mapCoordRoi = QRect(parts[0].toInt(), parts[1].toInt(), parts[2].toInt(), parts[3].toInt());
    }
    if (s.contains("ROIs/TargetAvatar")) {
        QStringList parts = s.value("ROIs/TargetAvatar").toString().split(',');
        if (parts.size() == 4)
            targetAvatarRoi = QRect(parts[0].toInt(), parts[1].toInt(), parts[2].toInt(), parts[3].toInt());
    }
    if (s.contains("ROIs/DialogBtn")) {
        QStringList parts = s.value("ROIs/DialogBtn").toString().split(',');
        if (parts.size() == 4)
            dialogBtnRoi = QRect(parts[0].toInt(), parts[1].toInt(), parts[2].toInt(), parts[3].toInt());
    }
    questLog(QString("任务追踪ROI: (%1,%2,%3,%4) 地图坐标ROI: (%5,%6,%7,%8) 对手头像ROI: (%9,%10,%11,%12) 对话按钮ROI: (%13,%14,%15,%16)")
                 .arg(questTrackRoi.x()).arg(questTrackRoi.y()).arg(questTrackRoi.width()).arg(questTrackRoi.height())
                 .arg(mapCoordRoi.x()).arg(mapCoordRoi.y()).arg(mapCoordRoi.width()).arg(mapCoordRoi.height())
                 .arg(targetAvatarRoi.x()).arg(targetAvatarRoi.y()).arg(targetAvatarRoi.width()).arg(targetAvatarRoi.height())
                 .arg(dialogBtnRoi.x()).arg(dialogBtnRoi.y()).arg(dialogBtnRoi.width()).arg(dialogBtnRoi.height()));

    // 加载字库
    m_fontPath = QCoreApplication::applicationDirPath() + "/datang_font.bfl";
    if (BitmapFontLib::Instance().load(m_fontPath)) {
        m_fontLoaded = true;
        questLog(QString("字库已加载: %1 字").arg(BitmapFontLib::Instance().charCount()));
    } else {
        m_fontLoaded = false;
        questLog("⚠ 字库加载失败,将使用纯颜色检测");
    }

    toRun = true;
    detectGameWindow();
    currentState = MainQuestState::ReadQuestTrack;
    retryCount = 0;
    currentQuestType = QuestType::Unknown;
    m_hasPrevMap = false;
    m_elapsed.start();
    // 注意: 不在这里调 start(),由外部 mainwindow 调 svc->start() 启动线程
    // run() 也不调 startService(),避免重复调用
}

void MainQuestService::stopService()
{
    questLog("主线任务服务停止");
    toRun = false;
}

void MainQuestService::transitionTo(MainQuestState next)
{
    questLog(QString("[状态] %1 → %2")
                 .arg(static_cast<int>(currentState))
                 .arg(static_cast<int>(next)));
    currentState = next;
    retryCount = 0;
}

QRect MainQuestService::offsetROI(const QRect &r) const
{
    return QRect(r.x() + gameOffsetX, r.y() + gameOffsetY, r.width(), r.height());
}

// ════════════════════════════════════════
// 视觉检测
// ════════════════════════════════════════

double MainQuestService::darkRatio(const QRect &roi)
{
    cv::Mat screen = screenToMat();
    if (screen.empty()) return 0;

    QRect r = offsetROI(roi);
    int x = qBound(0, r.x(), screen.cols - 1);
    int y = qBound(0, r.y(), screen.rows - 1);
    int w = qMin(r.width(),  screen.cols - x);
    int h = qMin(r.height(), screen.rows - y);
    if (w <= 0 || h <= 0) return 0;

    cv::Mat crop = screen(cv::Rect(x, y, w, h));
    cv::Mat gray;
    cv::cvtColor(crop, gray, cv::COLOR_BGR2GRAY);
    int dark = cv::countNonZero(gray < 80);
    int total = gray.rows * gray.cols;
    return (double)dark / total;
}

bool MainQuestService::isDialogOpen()
{
    return darkRatio(dialogCheckRoi) > 0.25;
}

QList<QRect> MainQuestService::findBlueHighlights(const QRect &roi)
{
    QList<QRect> results;
    cv::Mat screen = screenToMat();
    if (screen.empty()) return results;

    QRect r = offsetROI(roi);
    int x = qBound(0, r.x(), screen.cols - 1);
    int y = qBound(0, r.y(), screen.rows - 1);
    int w = qMin(r.width(),  screen.cols - x);
    int h = qMin(r.height(), screen.rows - y);
    if (w <= 0 || h <= 0) return results;

    cv::Mat crop = screen(cv::Rect(x, y, w, h)).clone();

    // 调试:保存ROI截图
    static int dbgSave = 0;
    if (dbgSave < 3) {
        cv::imwrite("debug_quest_roi_" + std::to_string(dbgSave) + ".png", crop);
        questLog(QString("保存调试ROI截图: debug_quest_roi_%1.png").arg(dbgSave));
        dbgSave++;
    }
    cv::Mat hsv;
    cv::cvtColor(crop, hsv, cv::COLOR_BGR2HSV);

    cv::Mat mask;
    cv::inRange(hsv, cv::Scalar(100, 80, 80), cv::Scalar(130, 255, 255), mask);

    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(20, 6));
    cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, kernel);
    cv::morphologyEx(mask, mask, cv::MORPH_OPEN, kernel);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    for (auto &cnt : contours) {
        cv::Rect br = cv::boundingRect(cnt);
        if (br.width < 80 || br.height < 15) continue;
        if (br.width > 300 || br.height > 60) continue;
        double ratio = (double)br.width / br.height;
        if (ratio < 2.0 || ratio > 15.0) continue;
        results.append(QRect(x + br.x, y + br.y, br.width, br.height));
    }

    std::sort(results.begin(), results.end(), [](const QRect &a, const QRect &b) {
        return a.y() < b.y();
    });
    return results;
}

QList<QRect> MainQuestService::findCoordinateLinks(const QRect &roi)
{
    QList<QRect> results;
    cv::Mat screen = screenToMat();
    if (screen.empty()) return results;

    QRect r = offsetROI(roi);
    int x = qBound(0, r.x(), screen.cols - 1);
    int y = qBound(0, r.y(), screen.rows - 1);
    int w = qMin(r.width(),  screen.cols - x);
    int h = qMin(r.height(), screen.rows - y);
    if (w <= 0 || h <= 0) return results;

    cv::Mat crop = screen(cv::Rect(x, y, w, h)).clone();

    // 调试:保存ROI截图
    static int dbgSave = 0;
    if (dbgSave < 3) {
        cv::imwrite("debug_quest_roi_" + std::to_string(dbgSave) + ".png", crop);
        questLog(QString("保存调试ROI截图: debug_quest_roi_%1.png").arg(dbgSave));
        dbgSave++;
    }
    cv::Mat hsv;
    cv::cvtColor(crop, hsv, cv::COLOR_BGR2HSV);

    // 坐标链接通常是亮青色/浅蓝色文字
    cv::Mat mask;
    cv::inRange(hsv, cv::Scalar(85, 100, 180), cv::Scalar(105, 255, 255), mask);

    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(8, 3));
    cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, kernel);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    for (auto &cnt : contours) {
        cv::Rect br = cv::boundingRect(cnt);
        if (br.width < 40 || br.height < 10) continue;
        if (br.width > 200 || br.height > 40) continue;
        double ratio = (double)br.width / br.height;
        if (ratio < 1.5 || ratio > 10.0) continue;
        results.append(QRect(x + br.x, y + br.y, br.width, br.height));
    }

    // 从下到上(描述区最下面的坐标通常是目的地)
    std::sort(results.begin(), results.end(), [](const QRect &a, const QRect &b) {
        return a.y() > b.y();
    });
    return results;
}

QList<QRect> MainQuestService::findGoldButtons(const QRect &roi)
{
    QList<QRect> results;
    cv::Mat screen = screenToMat();
    if (screen.empty()) return results;

    QRect r = offsetROI(roi);
    int x = qBound(0, r.x(), screen.cols - 1);
    int y = qBound(0, r.y(), screen.rows - 1);
    int w = qMin(r.width(),  screen.cols - x);
    int h = qMin(r.height(), screen.rows - y);
    if (w <= 0 || h <= 0) return results;

    cv::Mat crop = screen(cv::Rect(x, y, w, h)).clone();

    // 调试:保存ROI截图
    static int dbgSave = 0;
    if (dbgSave < 3) {
        cv::imwrite("debug_quest_roi_" + std::to_string(dbgSave) + ".png", crop);
        questLog(QString("保存调试ROI截图: debug_quest_roi_%1.png").arg(dbgSave));
        dbgSave++;
    }
    cv::Mat hsv;
    cv::cvtColor(crop, hsv, cv::COLOR_BGR2HSV);

    cv::Mat mask1, mask2, mask;
    cv::inRange(hsv, cv::Scalar(10, 60, 100), cv::Scalar(40, 255, 255), mask1);
    cv::inRange(hsv, cv::Scalar(10, 30, 160), cv::Scalar(35, 255, 255), mask2);
    mask = mask1 | mask2;

    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(12, 4));
    cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, kernel);
    cv::morphologyEx(mask, mask, cv::MORPH_OPEN, kernel);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    for (auto &cnt : contours) {
        cv::Rect br = cv::boundingRect(cnt);
        if (br.width < 30 || br.height < 12) continue;
        if (br.width > 300 || br.height > 80) continue;
        double ratio = (double)br.width / br.height;
        if (ratio < 1.2 || ratio > 8.0) continue;
        results.append(QRect(x + br.x, y + br.y, br.width, br.height));
    }

    std::sort(results.begin(), results.end(), [](const QRect &a, const QRect &b) {
        return a.width() * a.height() > b.width() * b.height();
    });
    return results;
}

bool MainQuestService::isMapCoordChanging()
{
    cv::Mat screen = screenToMat();
    if (screen.empty()) return false;

    QRect r = offsetROI(mapCoordRoi);
    int x = qBound(0, r.x(), screen.cols - 1);
    int y = qBound(0, r.y(), screen.rows - 1);
    int w = qMin(r.width(),  screen.cols - x);
    int h = qMin(r.height(), screen.rows - y);
    if (w <= 0 || h <= 0) return false;

    cv::Mat cur = screen(cv::Rect(x, y, w, h)).clone();

    if (!m_hasPrevMap) {
        m_prevMapCoord = cur.clone();
        m_hasPrevMap = true;
        return true;
    }

    cv::Mat diff;
    cv::absdiff(cur, m_prevMapCoord, diff);
    double change = cv::mean(diff)[0];

    m_prevMapCoord = cur.clone();
    questLog(QString("地图坐标帧差: %1").arg(change, 0, 'f', 2));
    return change > 3.0;
}

bool MainQuestService::isInCombat()
{
    cv::Mat screen = screenToMat();
    if (screen.empty()) return false;

    QRect r = offsetROI(combatCheckRoi);
    int x = qBound(0, r.x(), screen.cols - 1);
    int y = qBound(0, r.y(), screen.rows - 1);
    int w = qMin(r.width(),  screen.cols - x);
    int h = qMin(r.height(), screen.rows - y);
    if (w <= 0 || h <= 0) return false;

    cv::Mat crop = screen(cv::Rect(x, y, w, h)).clone();

    // 调试:保存ROI截图
    static int dbgSave = 0;
    if (dbgSave < 3) {
        cv::imwrite("debug_quest_roi_" + std::to_string(dbgSave) + ".png", crop);
        questLog(QString("保存调试ROI截图: debug_quest_roi_%1.png").arg(dbgSave));
        dbgSave++;
    }
    cv::Mat hsv;
    cv::cvtColor(crop, hsv, cv::COLOR_BGR2HSV);

    cv::Mat redMask1, redMask2, redMask;
    cv::inRange(hsv, cv::Scalar(0, 100, 100), cv::Scalar(10, 255, 255), redMask1);
    cv::inRange(hsv, cv::Scalar(160, 100, 100), cv::Scalar(179, 255, 255), redMask2);
    redMask = redMask1 | redMask2;

    int redPixels = cv::countNonZero(redMask);
    questLog(QString("战斗检测红色像素: %1").arg(redPixels));
    return redPixels > 500;
}

QString MainQuestService::recognizeQuestText()
{
    cv::Mat screen = screenToMat();
    if (screen.empty()) return {};

    QRect r = offsetROI(questTrackRoi);
    int x = qBound(0, r.x(), screen.cols - 1);
    int y = qBound(0, r.y(), screen.rows - 1);
    int w = qMin(r.width(),  screen.cols - x);
    int h = qMin(r.height(), screen.rows - y);
    if (w <= 0 || h <= 0) return {};

    cv::Mat crop = screen(cv::Rect(x, y, w, h)).clone();

    // 调试:保存ROI截图
    static int dbgSave = 0;
    if (dbgSave < 3) {
        cv::imwrite("debug_quest_roi_" + std::to_string(dbgSave) + ".png", crop);
        questLog(QString("保存调试ROI截图: debug_quest_roi_%1.png").arg(dbgSave));
        dbgSave++;
    }

    if (!m_fontLoaded || BitmapFontLib::Instance().isEmpty()) {
        questLog("字库为空,跳过文字识别");
        return {};
    }

    // 用findString直接传彩色图,每个字形用自己的colorFilter做binarize
    double sim = 0.85;
    auto results = BitmapFontLib::Instance().findString(crop, sim);

    // NMS: 同一字形的重叠匹配取最高分
    std::sort(results.begin(), results.end(),
        [](const auto &a, const auto &b) { return a.similarity > b.similarity; });

    std::vector<BflFindResult> filtered;
    std::vector<bool> suppressed(results.size(), false);
    for (int i = 0; i < (int)results.size(); i++) {
        if (suppressed[i]) continue;
        filtered.push_back(results[i]);
        for (int j = i + 1; j < (int)results.size(); j++) {
            if (suppressed[j]) continue;
            if (results[j].charName == results[i].charName) {
                int x1 = std::max(results[i].x, results[j].x);
                int x2 = std::min(results[i].x + results[i].width,
                                  results[j].x + results[j].width);
                int overlap = x2 - x1;
                int minWidth = std::min(results[i].width, results[j].width);
                if (overlap > minWidth * 0.5) {
                    suppressed[j] = true;
                }
            }
        }
    }

    // 按所在位置x排序,左到右拼接
    std::sort(filtered.begin(), filtered.end(),
        [](const auto &a, const auto &b) { return a.x < b.x; });

    QString text;
    for (const auto &r : filtered) {
        text += QString::fromStdString(r.charName);
    }
    questLog(QString("任务追踪文字: %1 (匹配%2个 压后%3个)")
        .arg(text).arg(results.size()).arg(filtered.size()));
    return text;
}

// ════════════════════════════════════════
// 动作
// ════════════════════════════════════════

void MainQuestService::clickAt(int sx, int sy)
{
    MouseKeyboardManager::Instance().mouseMoveDirect(sx, sy);
    QThread::msleep(50);
    MouseKeyboardManager::Instance().mouseClick();
    questLog(QString("点击(%1,%2)").arg(sx).arg(sy));
}

void MainQuestService::clickCenter(const QRect &r)
{
    clickAt(r.center().x(), r.center().y());
}

// ════════════════════════════════════════
// 工具
// ════════════════════════════════════════

cv::Mat MainQuestService::screenToMat()
{
    picMutex.lock();
    if (!curPic.data || curPic.data->empty()) {
        picMutex.unlock();
        return {};
    }
    cv::Mat img(curPic.des.Height, curPic.des.Width, CV_8UC4,
                curPic.data->data(), curPic.RowPitch);
    cv::Mat bgr;
    cv::cvtColor(img, bgr, cv::COLOR_BGRA2BGR);
    picMutex.unlock();
    return bgr;
}

void MainQuestService::questLog(const QString &msg)
{
    QString log = QString("[主线] %1").arg(msg);
    infof(log.toStdString());
    emit SignalSlotConnector::Instance().log(log);
}

void MainQuestService::randSleep(int minMs, int maxMs)
{
    int ms = minMs + QRandomGenerator::global()->bounded(maxMs - minMs + 1);
    QThread::msleep(ms);
}

QRect MainQuestService::findTemplate(const QString &name, double threshold)
{
    cv::Mat screen = screenToMat();
    if (screen.empty()) return QRect();

    // 加载模板(带缓存)
    if (!m_templateCache.contains(name)) {
        if (m_templateRoot.isEmpty())
            m_templateRoot = QCoreApplication::applicationDirPath() + "/images";
        QString path = m_templateRoot + "/" + name;
        // Windows 中文路径:用 _wfopen 读取字节,cv::imdecode 解码
        FILE *fp = _wfopen(reinterpret_cast<const wchar_t *>(path.utf16()), L"rb");
        if (!fp) {
            questLog(QString("模板图片不存在: %1").arg(path));
            return QRect();
        }
        fseek(fp, 0, SEEK_END);
        long sz = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        std::vector<char> buf(sz);
        fread(buf.data(), 1, sz, fp);
        fclose(fp);
        cv::Mat tmpl = cv::imdecode(buf, cv::IMREAD_COLOR);
        if (tmpl.empty()) {
            questLog(QString("模板图片解码失败: %1").arg(path));
            return QRect();
        }
        m_templateCache[name] = tmpl;
    }
    const cv::Mat &tmpl = m_templateCache[name];

    cv::Mat result;
    cv::matchTemplate(screen, tmpl, result, cv::TM_CCOEFF_NORMED);

    double maxVal;
    cv::Point maxLoc;
    cv::minMaxLoc(result, nullptr, &maxVal, nullptr, &maxLoc);

    if (maxVal >= threshold) {
        return QRect(maxLoc.x, maxLoc.y, tmpl.cols, tmpl.rows);
    }
    return QRect();
}

QRect MainQuestService::findTemplateInROI(const QString &name, double threshold, const QRect &roi)
{
    cv::Mat screen = screenToMat();
    if (screen.empty()) return QRect();

    // 加载模板（带缓存）
    if (!m_templateCache.contains(name)) {
        if (m_templateRoot.isEmpty())
            m_templateRoot = QCoreApplication::applicationDirPath() + "/images";
        QString path = m_templateRoot + "/" + name;
        FILE *fp = _wfopen(reinterpret_cast<const wchar_t *>(path.utf16()), L"rb");
        if (!fp) {
            questLog(QString("模板图片不存在: %1").arg(path));
            return QRect();
        }
        fseek(fp, 0, SEEK_END);
        long sz = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        std::vector<char> buf(sz);
        fread(buf.data(), 1, sz, fp);
        fclose(fp);
        cv::Mat tmpl = cv::imdecode(buf, cv::IMREAD_COLOR);
        if (tmpl.empty()) {
            questLog(QString("模板图片解码失败: %1").arg(path));
            return QRect();
        }
        m_templateCache[name] = tmpl;
    }
    const cv::Mat &tmpl = m_templateCache[name];

    // 裁剪 ROI 区域
    QRect r = offsetROI(roi);
    int rx = qBound(0, r.x(), screen.cols - 1);
    int ry = qBound(0, r.y(), screen.rows - 1);
    int rw = qMin(r.width(), screen.cols - rx);
    int rh = qMin(r.height(), screen.rows - ry);
    if (rw <= 0 || rh <= 0 || rw < tmpl.cols || rh < tmpl.rows) {
        return QRect();
    }
    cv::Mat crop = screen(cv::Rect(rx, ry, rw, rh)).clone();

    cv::Mat result;
    cv::matchTemplate(crop, tmpl, result, cv::TM_CCOEFF_NORMED);

    double maxVal;
    cv::Point maxLoc;
    cv::minMaxLoc(result, nullptr, &maxVal, nullptr, &maxLoc);

    if (maxVal >= threshold) {
        // 返回屏幕绝对坐标
        return QRect(rx + maxLoc.x, ry + maxLoc.y, tmpl.cols, tmpl.rows);
    }
    return QRect();
}

// ════════════════════════════════════════
// 任务名 → 类型 映射表(写死)
// 识别到任务名后直接查表,不靠关键字猜
// ════════════════════════════════════════
QuestType MainQuestService::questTypeFromName(const QString &name) const
{
    // 对话任务
    static const QStringList talkQuests = {
        "主线拜见宁婉儿",
        // 后续对话任务在这里加
    };
    // 杀怪任务
    static const QStringList killQuests = {
        // 杀怪任务名在这里加
        "主线熟悉武器",
    };

    for (const auto &q : talkQuests) {
        if (name.contains(q)) return QuestType::Talk;
    }
    for (const auto &q : killQuests) {
        if (name.contains(q)) return QuestType::Kill;
    }
    return QuestType::Unknown;
}

QString MainQuestService::monsterNameFromQuest(const QString &questName) const
{
    // 任务名 → 怪物名 映射表
    static const QHash<QString, QString> monsterMap = {
        {"主线熟悉武器", "豪猪"},
        // 后续杀怪任务在这里加：
        // {"任务名", "怪物名"},
    };

    for (auto it = monsterMap.begin(); it != monsterMap.end(); ++it) {
        if (questName.contains(it.key())) {
            return it.value();
        }
    }
    return {};
}

void MainQuestService::detectGameWindow()
{
    HWND hGameWnd = nullptr;
    HWND hWnd = GetTopWindow(nullptr);
    while (hWnd) {
        WCHAR buf[256] = {0};
        if (GetWindowTextW(hWnd, buf, 255) > 0 && IsWindowVisible(hWnd)) {
            QString t = QString::fromWCharArray(buf);
            if (t.contains("大唐无双")) { hGameWnd = hWnd; break; }
        }
        hWnd = GetNextWindow(hWnd, GW_HWNDNEXT);
    }
    if (hGameWnd) {
        RECT cr;
        if (GetClientRect(hGameWnd, &cr)) {
            POINT pt = {0, 0};
            ClientToScreen(hGameWnd, &pt);
            gameOffsetX = pt.x;
            gameOffsetY = pt.y;
            questLog(QString("游戏窗口: offset(%1,%2) 客户区:%3x%4")
                         .arg(gameOffsetX).arg(gameOffsetY)
                         .arg(cr.right - cr.left).arg(cr.bottom - cr.top));
        }
    } else {
        questLog("⚠ 未找到游戏窗口");
    }
}

// ════════════════════════════════════════
// 主循环
// ════════════════════════════════════════

void MainQuestService::run()
{
    // 在线程启动时初始化(加载ROI、字库等)
    startService();
    while (toRun) {
        processState();
        QThread::msleep(loopIntervalMs);
    }
}

void MainQuestService::processState()
{
    switch (currentState) {

    // ── 空闲 ──
    case MainQuestState::Idle:
        QThread::msleep(3000);
        transitionTo(MainQuestState::ReadQuestTrack);
        break;

    // ── 读取画面右侧任务追踪面板 ──
    case MainQuestState::ReadQuestTrack:
    {
        questLog("读取任务追踪面板");

        cv::Mat screen = screenToMat();
        if (screen.empty()) {
            questLog("截图为空,等待重试");
            QThread::msleep(1000);
            break;
        }

        QRect r = offsetROI(questTrackRoi);
        int rx = qBound(0, r.x(), screen.cols - 1);
        int ry = qBound(0, r.y(), screen.rows - 1);
        int rw = qMin(r.width(),  screen.cols - rx);
        int rh = qMin(r.height(), screen.rows - ry);
        if (rw <= 0 || rh <= 0) {
            questLog("任务追踪ROI无效");
            QThread::msleep(1000);
            break;
        }

        cv::Mat crop = screen(cv::Rect(rx, ry, rw, rh)).clone();

        if (!m_fontLoaded || BitmapFontLib::Instance().isEmpty()) {
            questLog("字库为空,跳过文字识别");
            QThread::msleep(1000);
            break;
        }

        auto results = BitmapFontLib::Instance().findString(crop, 0.85);

        // 拼接识别到的所有文字
        QString allText;
        QString mainTask;
        for (const auto &r : results)
        {
            if(QString::fromStdString(r.charName).contains("主线"))
            {
                mainTask = QString::fromStdString(r.charName);
            }
            allText += QString::fromStdString(r.charName);
        }
        questLog(QString("识别结果: %1 (匹配%2个)").arg(allText).arg(results.size()));



        // 记录当前任务名
        m_currentQuestName = mainTask;

        // 查映射表确定任务类型
        currentQuestType = questTypeFromName(m_currentQuestName);
        if (currentQuestType == QuestType::Talk) {
            questLog("任务类型:对话");
        } else if (currentQuestType == QuestType::Kill) {
            questLog("任务类型:杀怪");
        } else {
            questLog("任务类型:未知");
        }

        // 找"未完成"的匹配(表示任务进行中)
        BflFindResult incompleteMatch;
        bool foundIncomplete = false;
        for (const auto &r : results) {
            if (QString::fromStdString(r.charName) == QStringLiteral("未完成")) {  // 未完成
                if (!foundIncomplete || r.similarity > incompleteMatch.similarity) {
                    incompleteMatch = r;
                    foundIncomplete = true;
                }
            }
        }

        // 判断任务是否完成：先找"未完成"字样，找不到再检测红色进度数字
        bool questIncomplete = foundIncomplete;
        int incompleteClickX = 0, incompleteClickY = 0;

        if (foundIncomplete) {
            // "未完成"字样位置 → 点击左侧20px寻路
            incompleteClickX = rx + incompleteMatch.x - 20;
            incompleteClickY = ry + incompleteMatch.y + incompleteMatch.height / 2;
        } else {
            // 没找到"未完成"，查"打倒"右边150×30区域检测红色进度数字
            bool foundDadao = false;
            BflFindResult dadaoMatch;
            for (const auto &r : results) {
                if (QString::fromStdString(r.charName) == QStringLiteral("打倒")) {
                    dadaoMatch = r;
                    foundDadao = true;
                    break;
                }
            }

            if (foundDadao) {
                // "打倒"右侧紧邻150×30区域
                int redX = rx + dadaoMatch.x + dadaoMatch.width;
                int redY = ry + dadaoMatch.y;
                int redW = qMin(150, crop.cols - (dadaoMatch.x + dadaoMatch.width));
                int redH = dadaoMatch.height;

                if (redW > 0 && redH > 0) {
                    cv::Rect redRoi(dadaoMatch.x + dadaoMatch.width, dadaoMatch.y, redW, redH);
                    cv::Mat redCrop = crop(redRoi);

                    cv::Mat hsv;
                    cv::cvtColor(redCrop, hsv, cv::COLOR_BGR2HSV);
                    cv::Mat redMask1, redMask2, redMask;
                    cv::inRange(hsv, cv::Scalar(0, 100, 100), cv::Scalar(10, 255, 255), redMask1);
                    cv::inRange(hsv, cv::Scalar(170, 100, 100), cv::Scalar(180, 255, 255), redMask2);
                    cv::bitwise_or(redMask1, redMask2, redMask);

                    int redPixels = cv::countNonZero(redMask);
                    questLog(QString("红色进度检测(打倒右侧%1×%2): %3 个")
                                 .arg(redW).arg(redH).arg(redPixels));

                    // DEBUG
                    static int redDbg = 0;
                    if (redDbg < 5) {
                        cv::imwrite("debug_redpixel_" + std::to_string(redDbg) + ".png", redCrop);
                        questLog(QString("保存红色检测截图: debug_redpixel_%1.png (%2×%3)")
                                     .arg(redDbg).arg(redW).arg(redH));
                        redDbg++;
                    }

                    if (redPixels > 20) {
                        questIncomplete = true;
                        cv::Moments m = cv::moments(redMask, true);
                        if (m.m00 > 0) {
                            incompleteClickX = redX + (int)(m.m10 / m.m00) - 20;
                            incompleteClickY = redY + (int)(m.m01 / m.m00);
                        } else {
                            incompleteClickX = redX + redW / 2 - 20;
                            incompleteClickY = redY + redH / 2;
                        }
                    }
                }
            }
        }

        if (questIncomplete) {
            questLog(QString("任务进行中,点击寻路: (%1,%2)").arg(incompleteClickX).arg(incompleteClickY));
            clickAt(incompleteClickX, incompleteClickY);
            m_hasPrevMap = false;
            m_elapsed.restart();
            transitionTo(MainQuestState::WaitAutoPath);
        } else {
            // 任务已完成:找到任务名位置,点击其右侧30px寻路去找交付人
            questLog("任务已完成,寻找交付人");
            currentQuestType = QuestType::Submit;

            if (!results.empty()) {
                for (const auto &r : results) {
                    if (QString::fromStdString(r.charName) == QStringLiteral("交付人")) {
                        int clickX = rx + r.x + r.width + 30;
                        int clickY = ry + r.y + r.height / 2;
                        questLog(QString("点击交付人右侧30px寻路交付: (%1,%2)").arg(clickX).arg(clickY));
                        clickAt(clickX, clickY);
                        m_hasPrevMap = false;
                        m_elapsed.restart();
                        transitionTo(MainQuestState::WaitAutoPath);
                        break;
                    }
                }
            } else {
                questLog("未识别到任务名,无法寻路");
                QThread::msleep(2000);
                transitionTo(MainQuestState::ReadQuestTrack);
            }
        }
        break;
    }

    case MainQuestState::WaitAutoPath:
    {
        questLog("▶ 等待自动寻路到达(地图坐标帧差)...");
        int stableCount = 0;

        for (int i = 0; i < autoPathTimeoutMs / frameCheckMs; i++) {
            QThread::msleep(frameCheckMs);
            if (!toRun) return;

            bool changing = isMapCoordChanging();

            if (!changing) {
                stableCount++;
            } else {
                stableCount = 0;
            }

            if (i % 4 == 0) {
                questLog(QString("帧%1 稳定:%2/%3")
                             .arg(i).arg(stableCount).arg(frameStableFrames));
            }

            if (stableCount >= frameStableFrames) {
                questLog("✅ 寻路到达(地图坐标稳定)");
                transitionTo(MainQuestState::DetectTaskType);
                break;
            }
        }

        if (currentState == MainQuestState::WaitAutoPath) {
            questLog("⚠ 寻路超时,尝试检测当前状态");
            transitionTo(MainQuestState::DetectTaskType);
        }
        break;
    }

    // ── 判断任务类型并执行 ──
    case MainQuestState::DetectTaskType:
    {
        questLog("▶ 判断任务类型");

        if (currentQuestType == QuestType::Submit) {
            questLog("交付任务,开始对话");
            transitionTo(MainQuestState::DialogSubmit);
        } else if (currentQuestType == QuestType::Kill) {
            questLog("打怪任务,检测战斗状态");
            transitionTo(MainQuestState::DoCombat);
        } else {
            questLog("对话任务");
            transitionTo(MainQuestState::DoDialog);
        }
        break;
    }

    // ── 对话任务 ──
    case MainQuestState::DoDialog:
    {
        questLog("▶ 执行对话任务");

        if (!isDialogOpen()) {
            bool found = false;
            for (int i = 0; i < 20; i++) {
                QThread::msleep(500);
                if (isDialogOpen()) { found = true; break; }
            }
            if (!found) {
                questLog("⚠ 无对话框,可能已到达但NPC未交互");
                QRect npcRoi = offsetROI(QRect(400, 300, 240, 200));
                clickAt(npcRoi.center().x(), npcRoi.center().y());
                QThread::msleep(1500);
                if (!isDialogOpen()) {
                    questLog("仍无对话框,回到任务追踪重试");
                    transitionTo(MainQuestState::ReadQuestTrack);
                    break;
                }
            }
        }

        QList<QRect> btns = findGoldButtons(dialogBtnRoi);
        if (!btns.isEmpty()) {
            clickCenter(btns.first());
            questLog(QString("✅ 点击对话按钮 at (%1,%2)")
                         .arg(btns.first().center().x())
                         .arg(btns.first().center().y()));
        } else {
            QRect br = offsetROI(dialogBtnRoi);
            int bx = br.x() + br.width() * 2 / 3;
            int by = br.y() + br.height() / 2;
            clickAt(bx, by);
            questLog(QString("固定位对话 (%1,%2)").arg(bx).arg(by));
        }

        QThread::msleep(2000);
        transitionTo(MainQuestState::CheckProgress);
        break;
    }

    // ── 打怪任务 ──
    case MainQuestState::DoCombat:
    {
        questLog("▶ 执行打怪任务");

        // 1. 在对手头像ROI区域找怪物名字
        QRect avatarRoi = offsetROI(targetAvatarRoi);
        cv::Mat frame = screenToMat();
        if (frame.empty()) {
            questLog("截图失败,重试");
            randSleep(1500, 2500);
            break;
        }

        int ax = qBound(0, avatarRoi.x(), frame.cols - 1);
        int ay = qBound(0, avatarRoi.y(), frame.rows - 1);
        int aw = qMin(avatarRoi.width(),  frame.cols - ax);
        int ah = qMin(avatarRoi.height(), frame.rows - ay);
        if (aw <= 0 || ah <= 0) {
            questLog("对手头像ROI无效,重试");
            randSleep(1500, 2500);
            break;
        }
        cv::Mat avatarCrop = frame(cv::Rect(ax, ay, aw, ah)).clone();

        // 用字库在头像区域找怪物名字
        QString targetName = monsterNameFromQuest(m_currentQuestName);
        if (targetName.isEmpty()) {
            questLog(QString("任务[%1]未配置怪物名,跳过").arg(m_currentQuestName));
            transitionTo(MainQuestState::CheckProgress);
            break;
        }
        bool foundTarget = false;

        if (m_fontLoaded && !targetName.isEmpty()) {
            if (BitmapFontLib::Instance().isEmpty()) {
                questLog("字库为空,无法识别怪物名,请先训练怪物名字");
                transitionTo(MainQuestState::CheckProgress);
                break;
            }
            auto results = BitmapFontLib::Instance().findString(avatarCrop, 0.85);
            for (const auto &r : results) {
                QString name = QString::fromStdString(r.charName);
                if (name == targetName || targetName.contains(name)) {
                    foundTarget = true;
                    questLog(QString("找到目标: %1 sim=%2").arg(name).arg(r.similarity, 0, 'f', 2));
                    break;
                }
            }
        }

        if (!foundTarget) {
            // 2. 没找到怪物名字 → 按Tab切换目标
            questLog("未找到目标,按Tab切换");
            MouseKeyboardManager::Instance().keyPress(KEY_TAB);
            randSleep(50, 100);
            MouseKeyboardManager::Instance().keyRelease(KEY_TAB);
            randSleep(800, 1000);
            break;
        }

        // 3. 找到目标 → 按1、2技能打怪
        questLog("锁定目标,释放技能");
        MouseKeyboardManager::Instance().clickButton('1');
        randSleep(80, 150);
        MouseKeyboardManager::Instance().clickButton('1');
        randSleep(80, 150);
        MouseKeyboardManager::Instance().clickButton('1');
        randSleep(80, 150);

        MouseKeyboardManager::Instance().clickButton('2');
        randSleep(1800, 2500);

        // 4. 名字消失 → 检测任务是否完成
        // 重新截图检查目标是否还在
        cv::Mat frame2 = screenToMat();
        if (!frame2.empty()) {
            cv::Mat avatarCrop2 = frame2(cv::Rect(ax, ay, aw, ah)).clone();
            bool stillThere = false;
            if (m_fontLoaded) {
                auto results2 = BitmapFontLib::Instance().findString(avatarCrop2, 0.85);
                for (const auto &r : results2) {
                    QString name = QString::fromStdString(r.charName);
                    if (name == targetName || targetName.contains(name)) {
                        stillThere = true;
                        break;
                    }
                }
            }
            if (!stillThere) {
                questLog("目标消失,重读任务面板确认进度");
                randSleep(1200, 2000);  // 等UI刷新
                transitionTo(MainQuestState::ReadQuestTrack);
                break;
            }
        }

        // 目标还在，继续循环打怪
        questLog("目标仍在,继续打怪");
        break;
    }

    // ── 检查进度 ──
    case MainQuestState::CheckProgress:
    {
        questLog("▶ 检查任务进度");

        if (isDialogOpen()) {
            questLog("仍有对话框,继续对话");
            transitionTo(MainQuestState::DoDialog);
            break;
        }

        QThread::msleep(2000);
        currentQuestType = QuestType::Unknown;
        transitionTo(MainQuestState::ReadQuestTrack);
        break;
    }

    // ── 对话交付 ──
    case MainQuestState::DialogSubmit:
    {
        questLog("▶ 对话交付");

        // 1. 等待弹窗出现（最多等15秒）
        bool popupFound = false;
        for (int i = 0; i < 30; i++) {
            QThread::msleep(500);
            if (!toRun) return;
            QRect cancelRect = findTemplate("popups/取消.png", 0.80);
            if (!cancelRect.isNull()) {
                questLog("检测到交付弹窗（取消按钮）");
                popupFound = true;
                break;
            }
        }

        if (!popupFound) {
            questLog("⚠ 未检测到交付弹窗，重试");
            retryCount++;
            if (retryCount >= MAX_RETRIES) {
                questLog("交付弹窗重试超限，回到读取任务");
                retryCount = 0;
                transitionTo(MainQuestState::ReadQuestTrack);
            }
            break;
        }
        retryCount = 0;

        // 2. 循环：在取消按钮上方找对话按钮→点击→等→再检测，直到弹窗消失
        int dialogRounds = 0;
        while (toRun) {
            // 再确认弹窗还在
            QRect cancelRect = findTemplate("popups/取消.png", 0.80);
            if (cancelRect.isNull()) {
                questLog("✅ 弹窗已消失，交付完成");
                break;
            }

            dialogRounds++;
            questLog(QString("对话第 %1 轮").arg(dialogRounds));

            // 在取消按钮上方区域找对话按钮
            // 取消按钮中心向上偏移区域作为搜索范围
            int searchTop = qMax(0, cancelRect.y() - 800);
            int searchHeight = cancelRect.y() - searchTop;
            QRect searchArea(cancelRect.x() - 50, searchTop,
                             cancelRect.width() + 100, searchHeight);

            // 按任务名找对应按钮图
            QString btnImage;
            if (m_currentQuestName.contains("拜见宁婉儿"))
            {
                if(dialogRounds == 1)
                {
                    btnImage = "popups/拜见小师姑.png";
                }
                else
                {
                    btnImage = "popups/对话.png";
                }
            }
            else
            {
                btnImage = "popups/对话.png";
            }
            // 后续任务按钮图在这里加 elif

            bool clicked = false;

            if (!btnImage.isEmpty()) {
                QRect btnRect = findTemplateInROI(btnImage, 0.80, searchArea);
                if (!btnRect.isNull()) {
                    questLog(QString("找到对话按钮: %1 at (%2,%3)")
                                 .arg(btnImage)
                                 .arg(btnRect.center().x())
                                 .arg(btnRect.center().y()));
                    clickCenter(btnRect);
                    randSleep(500, 1000);
                    MouseKeyboardManager::Instance().mouseMoveDirect(btnRect.center().x() + 100, btnRect.center().y() +  100);
                    clicked = true;
                }
            }

            // fallback: 金色按钮
            // if (!clicked) {
            //     QList<QRect> btns = findGoldButtons(dialogBtnRoi);
            //     if (!btns.isEmpty()) {
            //         clickCenter(btns.first());
            //         questLog("✅ fallback金色按钮点击");
            //         clicked = true;
            //     }
            // }

            // if (!clicked) {
            //     questLog("⚠ 未找到对话按钮，点取消按钮上方区域");
            //     // 点搜索区域中心
            //     clickAt(searchArea.center().x(), searchArea.center().y());
            // }
            randSleep(1200, 2000);
        }

        transitionTo(MainQuestState::Done);
        break;
    }

    // ── 完成,循环 ──
    case MainQuestState::Done:
    {
        questLog("✅ 任务步骤完成,3秒后继续下一个");
        QThread::msleep(3000);
        currentQuestType = QuestType::Unknown;
        m_hasPrevMap = false;
        transitionTo(MainQuestState::ReadQuestTrack);
        break;
    }

    default:
        transitionTo(MainQuestState::Idle);
        break;
    }
}

void MainQuestService::clientHandleRecMsg(const json &) {}
