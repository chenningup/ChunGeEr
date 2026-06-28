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
    m_delivering = false;
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

// ════════════════════════════════════════
// 视觉检测
// ════════════════════════════════════════

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

// ════════════════════════════════════════
// 工具
// ════════════════════════════════════════

void MainQuestService::questLog(const QString &msg)
{
    QString log = QString("[主线] %1").arg(msg);
    infof(log.toStdString());
    emit SignalSlotConnector::Instance().log(log);
}

void MainQuestService::moveRandomWASD(int durationMs)
{
    // 随机选W/A/S/D之一，按住移动
    static const int keys[] = { 'w', 'a', 's', 'd' };
    int k1 = keys[QRandomGenerator::global()->bounded(4)];
    int k2 = keys[QRandomGenerator::global()->bounded(4)];

    auto &km = MouseKeyboardManager::Instance();
    km.keyPress(k1);
    km.keyPress(k2);  // 同时按两个方向（如W+D斜向）

    int elapsed = 0;
    int step = 200;
    while (elapsed < durationMs) {
        QThread::msleep(step);
        elapsed += step;
        // 中途随机换方向
        if (QRandomGenerator::global()->bounded(100) < 20) {
            km.keyRelease(k1);
            km.keyRelease(k2);
            k1 = keys[QRandomGenerator::global()->bounded(4)];
            k2 = keys[QRandomGenerator::global()->bounded(4)];
            km.keyPress(k1);
            km.keyPress(k2);
        }
    }

    km.keyRelease(k1);
    km.keyRelease(k2);
    questLog(QString("WASD移动完成 (%1秒)").arg(durationMs / 1000));
}

QuestType MainQuestService::questTypeFromName(const QString &name) const
{
    // 对话任务
    static const QStringList talkQuests = {
        "主线拜见宁婉儿",
    };
    // 杀怪任务
    static const QStringList killQuests = {
        "主线熟悉武器",
        "主线打倒豪猪王",
    };
    // 熟悉药品
    static const QStringList useItemQuests = {
        "主线熟悉药品",
    };

    for (const auto &q : talkQuests) {
        if (name.contains(q)) return QuestType::Talk;
    }
    for (const auto &q : killQuests) {
        if (name.contains(q)) return QuestType::Kill;
    }
    for (const auto &q : useItemQuests) {
        if (name.contains(q)) return QuestType::UseItem;
    }
    return QuestType::Unknown;
}

QString MainQuestService::monsterNameFromQuest(const QString &questName) const
{
    // 任务名 → 怪物名 映射表
    static const QHash<QString, QString> monsterMap = {
        {"主线熟悉武器", "豪猪"},
        {"主线打倒豪猪王", "豪猪王"},
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

// ════════════════════════════════════════
// 主循环
// ════════════════════════════════════════

void MainQuestService::run()
{
    // 在线程启动时初始化(加载ROI、字库等)
    startService();
    while (toRun) {
        // 后台任务请求暂停时，等待恢复
        if (m_bgPaused) {
            QThread::msleep(200);
            continue;
        }
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

        // 拼接识别到的所有文字，选出置信度最高的主线任务
        QString allText;
        QString mainTask;
        double mainTaskSim = 0.0;
        for (const auto &r : results)
        {
            allText += QString::fromStdString(r.charName);
            if (QString::fromStdString(r.charName).contains("主线")) {
                if (r.similarity > mainTaskSim) {
                    mainTaskSim = r.similarity;
                    mainTask = QString::fromStdString(r.charName);
                }
            }
        }
        questLog(QString("识别结果: %1 (匹配%2个) | 主线: %3(%.2f)")
                     .arg(allText).arg(results.size()).arg(mainTask).arg(mainTaskSim));



        // 记录当前任务名
        m_currentQuestName = mainTask;

        // 查映射表确定任务类型
        currentQuestType = questTypeFromName(m_currentQuestName);
        if (currentQuestType == QuestType::Talk) {
            questLog("任务类型:对话");
        } else if (currentQuestType == QuestType::Kill) {
            questLog("任务类型:杀怪");
        } else if (currentQuestType == QuestType::UseItem) {
            questLog("任务类型:熟悉药品");
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
            m_delivering = true;

            bool foundDeliver = false;
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
                        foundDeliver = true;
                        break;
                    }
                }
            }

            if (!foundDeliver) {
                // 没找到任务名/交付人 → WASD移动5秒改变视角后重试
                if (m_wasdRetry < 3) {
                    m_wasdRetry++;
                    questLog(QString("未找到交付人/任务名, WASD移动5秒 (第%1次)").arg(m_wasdRetry));
                    moveRandomWASD(5000);
                    transitionTo(MainQuestState::ReadQuestTrack);
                } else {
                    m_wasdRetry = 0;
                    questLog("WASD重试3次仍失败,跳过");
                    QThread::msleep(2000);
                    transitionTo(MainQuestState::ReadQuestTrack);
                }
            } else {
                m_wasdRetry = 0;
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
                if (m_delivering)
                    transitionTo(MainQuestState::DialogSubmit);
                else
                    transitionTo(MainQuestState::DetectTaskType);
                break;
            }
        }

        if (currentState == MainQuestState::WaitAutoPath) {
            questLog("⚠ 寻路超时,尝试检测当前状态");
            if (m_delivering)
                transitionTo(MainQuestState::DialogSubmit);
            else
                transitionTo(MainQuestState::DetectTaskType);
        }
        break;
    }

    // ── 判断任务类型并执行 ──
    case MainQuestState::DetectTaskType:
    {
        if (currentQuestType == QuestType::UseItem) {
            // ═══ 使用物品（熟悉药品等）═══
            questLog("▶ 使用物品");

            cv::Mat frame = screenToMat();
            if (frame.empty()) {
                questLog("截图失败,重试");
                randSleep(1500, 2500);
                break;
            }

            // 搜索"确定"按钮（BFL文字识别，只搜"确定"不遍历全部字库）
            bool clicked = false;
            if (!BitmapFontLib::Instance().isEmpty()) {
                auto matches = BitmapFontLib::Instance().findText(frame, "确定", 0.85);
                if (!matches.empty()) {
                    const auto &m = matches[0];
                    int cx = m.x + m.width / 2;
                    int cy = m.y + m.height / 2;
                    questLog(QString("点击确定 (%1,%2) sim=%3")
                                 .arg(cx).arg(cy)
                                 .arg(m.similarity, 0, 'f', 3));
                    clickAt(cx, cy);
                    clicked = true;
                }
            }

            if (!clicked) {
                questLog("未找到确定按钮");
            }

            // 在背包中找"精钢剑"图片，右键装备
            QThread::msleep(1000);
            questLog("寻找精钢剑");
            {
                QRect itemRect = findTemplate("items/精钢剑", 0.75);
                if (!itemRect.isNull()) {
                    int cx = itemRect.center().x();
                    int cy = itemRect.center().y();
                    questLog(QString("精钢剑 at (%1,%2) → 右键装备")
                                 .arg(cx).arg(cy));
                    MouseKeyboardManager::Instance().mouseMoveDirect(cx, cy);
                    QThread::msleep(200);
                    MouseKeyboardManager::Instance().mouseRightClick();
                    QThread::msleep(500);
                } else {
                    questLog("未找到精钢剑图片");
                }
            }

            // 按B关闭背包
            QThread::msleep(500);
            questLog("按B关闭背包");
            MouseKeyboardManager::Instance().clickButton('b');
            QThread::msleep(500);

            transitionTo(MainQuestState::ReadQuestTrack);
        }
        else if (currentQuestType == QuestType::Kill) {
            // ═══ 打怪任务 ═══
            questLog("▶ 打怪任务");

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

            QString targetName = monsterNameFromQuest(m_currentQuestName);
            if (targetName.isEmpty()) {
                questLog(QString("任务[%1]未配置怪物名,返回重读").arg(m_currentQuestName));
                transitionTo(MainQuestState::ReadQuestTrack);
                break;
            }

            bool foundTarget = false;
            if (m_fontLoaded && !BitmapFontLib::Instance().isEmpty()) {
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
                questLog("未找到目标,按Tab切换");
                MouseKeyboardManager::Instance().keyPress(KEY_TAB);
                randSleep(50, 100);
                MouseKeyboardManager::Instance().keyRelease(KEY_TAB);
                randSleep(800, 1000);
                break;
            }

            // 找到目标 → 释放技能
            questLog("锁定目标,释放技能");
            MouseKeyboardManager::Instance().clickButton('1');
            randSleep(50, 80);
            MouseKeyboardManager::Instance().clickButton('1');
            randSleep(50, 80);
            MouseKeyboardManager::Instance().clickButton('1');
            randSleep(500, 1000);
            MouseKeyboardManager::Instance().clickButton('2');
            randSleep(1000, 1500);

            // 目标消失 → 重读面板确认
            cv::Mat frame2 = screenToMat();
            if (!frame2.empty()) {
                cv::Mat avatarCrop2 = frame2(cv::Rect(ax, ay, aw, ah)).clone();
                bool stillThere = false;
                if (m_fontLoaded && !BitmapFontLib::Instance().isEmpty()) {
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
                    randSleep(1200, 2000);
                    transitionTo(MainQuestState::ReadQuestTrack);
                    break;
                }
            }
            questLog("目标仍在,继续打怪");
        }
        else {
            // ═══ 对话任务 ═══
            questLog("▶ 对话任务");

            // 在对话框ROI区域内找"取消"按钮（config.ini可配 DialogBtn ROI）
            QRect dialogROI = offsetROI(dialogBtnRoi);

            // 等待对话框出现（检测取消按钮）
            bool dialogFound = false;
            for (int i = 0; i < 20; i++) {
                QThread::msleep(500);
                if (!toRun) return;
                // 在dialogROI区域内搜索
                QRect cropRoi = dialogROI.isValid() ? dialogROI : QRect(0, 0, 0, 0);
                QRect cancelRect;
                if (!cropRoi.isEmpty()) {
                    cancelRect = findTemplateInROI("popups/取消", 0.80, cropRoi);
                } else {
                    cancelRect = findTemplate("popups/取消", 0.80);
                }
                if (!cancelRect.isNull()) {
                    dialogFound = true;
                    break;
                }
            }

            if (!dialogFound) {
                questLog("⚠ 无对话框,尝试点击NPC");
                int cx = dialogROI.isValid() ? dialogROI.center().x() : 520;
                int cy = dialogROI.isValid() ? dialogROI.center().y() : 400;
                clickAt(cx, cy);
                QThread::msleep(1500);
                QRect cancelRect;
                if (!dialogROI.isEmpty()) {
                    cancelRect = findTemplateInROI("popups/取消", 0.80, dialogROI);
                } else {
                    cancelRect = findTemplate("popups/取消", 0.80);
                }
                if (cancelRect.isNull()) {
                    questLog("仍无对话框,回到任务追踪重试");
                    transitionTo(MainQuestState::ReadQuestTrack);
                    break;
                }
            }

            // 循环对话：第一轮点拜见小师姑，后续点对话
            int dialogRounds = 0;
            while (toRun) {
                // 在dialogROI区域内找取消按钮
                QRect cancelRect;
                if (!dialogROI.isEmpty()) {
                    cancelRect = findTemplateInROI("popups/取消", 0.80, dialogROI);
                } else {
                    cancelRect = findTemplate("popups/取消", 0.80);
                }
                if (cancelRect.isNull()) {
                    questLog("✅ 对话框消失,对话结束");
                    break;
                }

                dialogRounds++;
                questLog(QString("对话第 %1 轮").arg(dialogRounds));

                questLog("find duihua");
                // 按任务名+轮次选按钮图
                QString btnImage;
                if (m_currentQuestName.contains("拜见宁婉儿")) {
                    if (dialogRounds == 1) {
                        btnImage = "popups/拜见小师姑";
                    } else {
                        btnImage = "popups/对话";
                    }
                } else {
                    btnImage = "popups/对话";
                }

                bool clicked = false;
                if (!btnImage.isEmpty()) {
                    QRect btnRect;
                    if (!dialogROI.isEmpty()) {
                        btnRect = findTemplateInROI(btnImage, 0.80, dialogROI);
                    } else {
                        btnRect = findTemplate(btnImage, 0.80);
                    }
                    if (!btnRect.isNull()) {
                        questLog(QString("找到对话按钮: %1 at (%2,%3)")
                                     .arg(btnImage)
                                     .arg(btnRect.center().x())
                                     .arg(btnRect.center().y()));
                        clickCenter(btnRect);
                        randSleep(500, 1000);
                        clicked = true;
                    }
                    else
                    {
                        questLog("btnRect is null");
                    }
                }
                else
                {
                    questLog("btnImage is empty");
                }
                if (!clicked) {
                    questLog(QString("⚠ 未找到对话按钮(%1),等待重试").arg(btnImage));
                }

                randSleep(1200, 2000);
            }

            currentQuestType = QuestType::Unknown;
            transitionTo(MainQuestState::ReadQuestTrack);
        }
        break;
    }

    // ── 对话交付 ──
    case MainQuestState::DialogSubmit:
    {
        questLog("▶ 对话交付");

        QRect dialogROI = offsetROI(dialogBtnRoi);

        // 1. 等待弹窗出现（最多等15秒，在dialogROI内搜索）
        bool popupFound = false;
        for (int i = 0; i < 30; i++) {
            QThread::msleep(500);
            if (!toRun) return;
            QRect cancelRect;
            if (!dialogROI.isEmpty()) {
                cancelRect = findTemplateInROI("popups/取消", 0.80, dialogROI);
            } else {
                cancelRect = findTemplate("popups/取消", 0.80);
            }
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

        // 2. 循环：在dialogROI内找对话按钮→点击→等→再检测，直到弹窗消失
        int dialogRounds = 0;
        while (toRun) {
            // 再确认弹窗还在（在dialogROI内搜索取消按钮）
            QRect cancelRect;
            if (!dialogROI.isEmpty()) {
                cancelRect = findTemplateInROI("popups/取消", 0.80, dialogROI);
            } else {
                cancelRect = findTemplate("popups/取消", 0.80);
            }
            if (cancelRect.isNull()) {
                questLog("✅ 弹窗已消失，交付完成");
                break;
            }

            dialogRounds++;
            questLog(QString("对话第 %1 轮").arg(dialogRounds));

            // 按任务名找对应按钮图
            QString btnImage;
            if (m_currentQuestName.contains("拜见宁婉儿"))
            {
                if(dialogRounds == 1)
                {
                    btnImage = "popups/拜见小师姑";
                }
                else
                {
                    btnImage = "popups/对话";
                }
            }
            else
            {
                btnImage = "popups/对话";
            }
            // 后续任务按钮图在这里加 elif

            bool clicked = false;

            if (!btnImage.isEmpty()) {
                QRect btnRect;
                if (!dialogROI.isEmpty()) {
                    btnRect = findTemplateInROI(btnImage, 0.80, dialogROI);
                } else {
                    btnRect = findTemplate(btnImage, 0.80);
                }
                if (!btnRect.isNull()) {
                    questLog(QString("找到对话按钮: %1 at (%2,%3)")
                                 .arg(btnImage)
                                 .arg(btnRect.center().x())
                                 .arg(btnRect.center().y()));
                    clickCenter(btnRect);
                    randSleep(200, 500);
                    clicked = true;
                }
            }

            randSleep(200, 500);
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
        m_delivering = false;
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
