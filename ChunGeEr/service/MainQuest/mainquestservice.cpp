#include "mainquestservice.h"
#include "../../LeoControl/mousekeyboardmanager.h"
#include "../../signalslotconnector.h"
#include "XuLog.h"
#include <QThread>
#include <QSettings>
#include <QCoreApplication>
#include <windows.h>
#include <algorithm>

MainQuestService::MainQuestService(QObject *parent) : BaseService(parent) {}

void MainQuestService::startService()
{
    questLog("主线任务服务启动");

    // 从 config.ini 加载 ROI
    QString iniPath = QCoreApplication::applicationDirPath() + "/config.ini";
    QSettings s(iniPath, QSettings::IniFormat);
    if (s.contains("ROIs/MainQuest")) {
        QStringList parts = s.value("ROIs/MainQuest").toString().split(',');
        if (parts.size() == 4)
            questTrackRoi = QRect(parts[0].toInt(), parts[1].toInt(), parts[2].toInt(), parts[3].toInt());
    }
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
    questLog(QString("任务追踪ROI: (%1,%2,%3,%4) 地图坐标ROI: (%5,%6,%7,%8)")
                 .arg(questTrackRoi.x()).arg(questTrackRoi.y()).arg(questTrackRoi.width()).arg(questTrackRoi.height())
                 .arg(mapCoordRoi.x()).arg(mapCoordRoi.y()).arg(mapCoordRoi.width()).arg(mapCoordRoi.height()));

    // 加载字库
    m_fontPath = QCoreApplication::applicationDirPath() + "/datang_font.bfl";
    if (BitmapFontLib::Instance().load(m_fontPath)) {
        m_fontLoaded = true;
        questLog(QString("字库已加载: %1 字").arg(BitmapFontLib::Instance().charCount()));
    } else {
        m_fontLoaded = false;
        questLog("⚠ 字库加载失败，将使用纯颜色检测");
    }

    toRun = true;
    detectGameWindow();
    currentState = MainQuestState::ReadQuestTrack;
    retryCount = 0;
    currentQuestType = QuestType::Unknown;
    m_hasPrevMap = false;
    m_elapsed.start();
    start();
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

    // 从下到上（描述区最下面的坐标通常是目的地）
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

    if (!m_fontLoaded || BitmapFontLib::Instance().isEmpty()) {
        questLog("字库为空，跳过文字识别");
        return {};
    }

    // 用字库二值化 + 识别
    cv::Mat binary = BitmapFontLib::Instance().binarize(crop);
    if (binary.empty()) return {};

    QString text = BitmapFontLib::Instance().recognizeText(binary);
    questLog(QString("任务追踪文字: %1").arg(text));
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
        questLog("▶ 读取任务追踪面板");

        // 1. 用字库识别任务文字（如果有字库的话）
        QString questText = recognizeQuestText();

        // 2. 在任务追踪区域找坐标链接
        QList<QRect> coordLinks = findCoordinateLinks(questTrackRoi);
        if (coordLinks.isEmpty()) {
            questLog("⚠ 任务追踪区无坐标链接");

            // 也找找蓝色高亮（可能有任务但没坐标=已完成待交）
            QList<QRect> blueItems = findBlueHighlights(questTrackRoi);
            if (!blueItems.isEmpty()) {
                questLog("找到蓝色高亮条目，可能任务已完成待交");
                currentQuestType = QuestType::Submit;
                // 点击蓝色条目
                clickCenter(blueItems.first());
                QThread::msleep(1000);
                transitionTo(MainQuestState::DialogSubmit);
            } else {
                retryCount++;
                if (retryCount > 3) {
                    questLog("无任务信息，等待5秒重试");
                    retryCount = 0;
                    QThread::msleep(5000);
                }
            }
            break;
        }

        // 3. 有坐标链接 → 点击寻路
        questLog(QString("找到%1个坐标链接，点击第一个").arg(coordLinks.size()));
        clickCenter(coordLinks.first());

        // 判断任务类型（暂时都当对话处理，后续字库识别后细化）
        if (currentQuestType == QuestType::Unknown) {
            currentQuestType = QuestType::Talk;
        }

        m_hasPrevMap = false;
        transitionTo(MainQuestState::WaitAutoPath);
        break;
    }

    // ── 地图坐标帧差等待寻路到达 ──
    case MainQuestState::WaitAutoPath:
    {
        questLog("▶ 等待自动寻路到达（地图坐标帧差）...");
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
                questLog("✅ 寻路到达（地图坐标稳定）");
                transitionTo(MainQuestState::DetectTaskType);
                break;
            }
        }

        if (currentState == MainQuestState::WaitAutoPath) {
            questLog("⚠ 寻路超时，尝试检测当前状态");
            transitionTo(MainQuestState::DetectTaskType);
        }
        break;
    }

    // ── 判断任务类型并执行 ──
    case MainQuestState::DetectTaskType:
    {
        questLog("▶ 判断任务类型");

        if (currentQuestType == QuestType::Submit) {
            questLog("交付任务，开始对话");
            transitionTo(MainQuestState::DialogSubmit);
        } else if (currentQuestType == QuestType::Kill) {
            questLog("打怪任务，检测战斗状态");
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
                questLog("⚠ 无对话框，可能已到达但NPC未交互");
                QRect npcRoi = offsetROI(QRect(400, 300, 240, 200));
                clickAt(npcRoi.center().x(), npcRoi.center().y());
                QThread::msleep(1500);
                if (!isDialogOpen()) {
                    questLog("仍无对话框，回到任务追踪重试");
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

        if (isInCombat()) {
            questLog("战斗中，等待击杀...");
            QThread::msleep(3000);
            break;
        }

        questLog("不在战斗，检查任务进度");
        transitionTo(MainQuestState::CheckProgress);
        break;
    }

    // ── 检查进度 ──
    case MainQuestState::CheckProgress:
    {
        questLog("▶ 检查任务进度");

        if (isDialogOpen()) {
            questLog("仍有对话框，继续对话");
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

        if (!isDialogOpen()) {
            for (int i = 0; i < 20; i++) {
                QThread::msleep(500);
                if (isDialogOpen()) break;
            }
        }

        if (isDialogOpen()) {
            QList<QRect> btns = findGoldButtons(dialogBtnRoi);
            if (!btns.isEmpty()) {
                clickCenter(btns.first());
                questLog("✅ 交付对话点击");
            } else {
                QRect br = offsetROI(dialogBtnRoi);
                clickAt(br.x() + br.width() * 2 / 3, br.y() + br.height() / 2);
            }
            QThread::msleep(2000);

            if (isDialogOpen()) {
                questLog("交付对话框仍在，继续点击");
                break;
            } else {
                questLog("✅ 交付完成");
                transitionTo(MainQuestState::Done);
            }
        } else {
            questLog("⚠ 无交付对话框，可能已完成");
            transitionTo(MainQuestState::Done);
        }
        break;
    }

    // ── 完成，循环 ──
    case MainQuestState::Done:
    {
        questLog("✅ 任务步骤完成，3秒后继续下一个");
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
