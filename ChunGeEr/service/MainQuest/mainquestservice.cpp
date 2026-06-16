#include "mainquestservice.h"
#include "../../Ocr/ocrmnager.h"
#include "../../LeoControl/mousekeyboardmanager.h"
#include "../../signalslotconnector.h"
#include "XuLog.h"
#include <QThread>
#include <windows.h>

MainQuestService::MainQuestService(QObject *parent) : BaseService(parent) {}

void MainQuestService::startService()
{
    questLog("主线任务服务启动 [视觉驱动]");
    toRun = true;
    detectGameWindow();
    questLog(QString("窗口偏移: (%1,%2)").arg(gameOffsetX).arg(gameOffsetY));
    currentState = MainQuestState::OpenQuestPanel;
    retryCount = 0;
    start();
}

void MainQuestService::stopService()
{
    questLog("主线任务服务停止");
    toRun = false;
}

void MainQuestService::transitionTo(MainQuestState next) { currentState = next; }

// ── 视觉：暗像素比例 ──
double MainQuestService::darkRatio(const QRect &roi)
{
    cv::Mat screen = screenToMat();
    if (screen.empty()) return 0;

    int x = qBound(0, roi.x(), screen.cols - 1);
    int y = qBound(0, roi.y(), screen.rows - 1);
    int w = qMin(roi.width(),  screen.cols - x);
    int h = qMin(roi.height(), screen.rows - y);
    if (w <= 0 || h <= 0) return 0;

    cv::Rect r(x, y, w, h);
    cv::Mat crop = screen(r);

    int dark = 0, total = crop.rows * crop.cols;
    for (int row = 0; row < crop.rows; row++) {
        for (int col = 0; col < crop.cols; col++) {
            cv::Vec3b px = crop.at<cv::Vec3b>(row, col);
            if ((px[0] + px[1] + px[2]) / 3.0 < 80) dark++;
        }
    }
    return (double)dark / total;
}

// ── 视觉：面板是否打开 ──
bool MainQuestService::isPanelOpen()
{
    double ratio = darkRatio(panelCheckRoi);
    questLog(QString("面板暗比: %1%").arg(ratio * 100, 0, 'f', 1));
    return ratio > 0.30;
}

// ── 视觉：对话框是否打开 ──
bool MainQuestService::isDialogOpen()
{
    double ratio = darkRatio(dialogCheckRoi);
    questLog(QString("对话框暗比: %1%").arg(ratio * 100, 0, 'f', 1));
    return ratio > 0.25;
}

// ── 视觉：找金色按钮 ──
QList<QRect> MainQuestService::findGoldButtons(const QRect &roi)
{
    QList<QRect> results;
    cv::Mat screen = screenToMat();
    if (screen.empty()) return results;

    int x = qBound(0, roi.x(), screen.cols - 1);
    int y = qBound(0, roi.y(), screen.rows - 1);
    int w = qMin(roi.width(),  screen.cols - x);
    int h = qMin(roi.height(), screen.rows - y);
    if (w <= 0 || h <= 0) return results;

    cv::Mat crop = screen(cv::Rect(x, y, w, h)).clone();
    cv::Mat hsv;
    cv::cvtColor(crop, hsv, cv::COLOR_BGR2HSV);

    // 金色/黄色范围
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

// ── 视觉：找蓝色高亮条目（活跃任务） ──
QList<QRect> MainQuestService::findBlueHighlights(const QRect &roi)
{
    QList<QRect> results;
    cv::Mat screen = screenToMat();
    if (screen.empty()) return results;

    int x = qBound(0, roi.x(), screen.cols - 1);
    int y = qBound(0, roi.y(), screen.rows - 1);
    int w = qMin(roi.width(),  screen.cols - x);
    int h = qMin(roi.height(), screen.rows - y);
    if (w <= 0 || h <= 0) return results;

    cv::Mat crop = screen(cv::Rect(x, y, w, h)).clone();
    cv::Mat hsv;
    cv::cvtColor(crop, hsv, cv::COLOR_BGR2HSV);

    // 蓝色高亮范围 (活跃任务背景)
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

    // 按Y坐标排序（从上到下）
    std::sort(results.begin(), results.end(), [](const QRect &a, const QRect &b) {
        return a.y() < b.y();
    });

    return results;
}

// ── 视觉：找坐标链接 (数字,数字) ──
QList<QRect> MainQuestService::findCoordinateLinks(const QRect &roi)
{
    QList<QRect> results;
    cv::Mat screen = screenToMat();
    if (screen.empty()) return results;

    int x = qBound(0, roi.x(), screen.cols - 1);
    int y = qBound(0, roi.y(), screen.rows - 1);
    int w = qMin(roi.width(),  screen.cols - x);
    int h = qMin(roi.height(), screen.rows - y);
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

    // 按Y坐标排序（从上到下，找描述区最下面的坐标）
    std::sort(results.begin(), results.end(), [](const QRect &a, const QRect &b) {
        return a.y() > b.y();  // 从下到上
    });

    return results;
}

// ── 视觉：检测任务列表滚动条 ──
bool MainQuestService::detectScrollbar(QRect &scrollBar)
{
    cv::Mat screen = screenToMat();
    if (screen.empty()) return false;

    // 滚动条在任务列表右侧，是窄竖条（灰/暗色）
    int sx = questClickRoi.right() - 15;
    int sy = questClickRoi.y();
    int sw = 15;
    int sh = questClickRoi.height();

    if (sx < 0 || sy < 0 || sx + sw > screen.cols || sy + sh > screen.rows) return false;

    cv::Mat crop = screen(cv::Rect(sx, sy, sw, sh)).clone();
    cv::Mat gray;
    cv::cvtColor(crop, gray, cv::COLOR_BGR2GRAY);

    // 找最暗的竖列（滑块）
    int bestCol = -1;
    double bestDark = 999;
    for (int col = 2; col < sw - 2; col++) {
        double sum = 0;
        for (int row = 0; row < sh; row++) {
            sum += gray.at<uchar>(row, col);
        }
        double avg = sum / sh;
        if (avg < bestDark && avg < 180) {
            bestDark = avg;
            bestCol = col;
        }
    }

    if (bestCol < 0) return false;

    // 在最佳列上找连续暗段（滑块）
    int top = -1, bot = -1;
    for (int row = 0; row < sh; row++) {
        if (gray.at<uchar>(row, bestCol) < 120) {
            if (top < 0) top = row;
            bot = row;
        }
    }

    if (top < 0 || bot - top < 30) return false;

    scrollBar = QRect(sx, sy + top, sw, bot - top);
    questLog(QString("滚动条: y=%1~%2, h=%3").arg(sy + top).arg(sy + bot).arg(bot - top));
    return true;
}

// ── 按键 ──
void MainQuestService::pressL()
{
    MouseKeyboardManager::Instance().clickButton('L');
}

// ── 点击 ──
void MainQuestService::clickAt(int sx, int sy)
{
    MouseKeyboardManager::Instance().mouseMoveDirect(sx, sy);
    QThread::msleep(50);
    MouseKeyboardManager::Instance().mouseClick();
    questLog(QString("点击(%1,%2)").arg(sx).arg(sy));
}

// ── 截屏 ──
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

// ── 日志 ──
void MainQuestService::questLog(const QString &msg)
{
    QString log = QString("[主线] %1").arg(msg);
    infof(log.toStdString());
    emit SignalSlotConnector::Instance().log(log);
}

// ── 窗口检测 ──
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
        RECT wr, cr;
        if (GetWindowRect(hGameWnd, &wr) && GetClientRect(hGameWnd, &cr)) {
            POINT pt = {0, 0};
            ClientToScreen(hGameWnd, &pt);
            gameOffsetX = pt.x - wr.left;
            gameOffsetY = pt.y - wr.top;
            questLog(QString("游戏窗口: (%1,%2) 客户区: %3x%4")
                         .arg(wr.left).arg(wr.top)
                         .arg(cr.right - cr.left).arg(cr.bottom - cr.top));
            if (wr.left != 0 || wr.top != 0) {
                SetWindowPos(hGameWnd, HWND_TOP, 0, 0,
                             wr.right - wr.left, wr.bottom - wr.top, SWP_SHOWWINDOW);
                gameOffsetX = pt.x;
                gameOffsetY = pt.y;
            }
        }
    }

    // 应用偏移到所有 ROI
    auto offset = [this](QRect &r) { r.translate(gameOffsetX, gameOffsetY); };
    offset(panelCheckRoi);
    offset(goBtnSearchRoi);
    offset(questClickRoi);
    offset(questDescRoi);
    offset(dialogCheckRoi);
    offset(dialogBtnRoi);
}

// ── 主循环 ──
void MainQuestService::run()
{
    while (toRun) {
        processState();
        QThread::msleep(loopIntervalMs);
    }
}

void MainQuestService::processState()
{
    switch (currentState) {
    // ── 状态0：空闲 ──
    case MainQuestState::Idle:
        QThread::msleep(3000);
        break;

    // ── 状态1：L键开面板，视觉确认 ──
    case MainQuestState::OpenQuestPanel:
    {
        questLog("L键打开任务面板");
        pressL();
        QThread::msleep(1500);

        if (isPanelOpen()) {
            questLog("✅ 面板已打开");
            transitionTo(MainQuestState::ClickQuestAndGo);
        } else {
            retryCount++;
            if (retryCount > MAX_RETRIES) {
                questLog("❌ 多次L键无效，跳过");
                retryCount = 0;
                transitionTo(MainQuestState::Idle);
            } else {
                questLog(QString("面板未开，重试 %1/%2").arg(retryCount).arg(MAX_RETRIES));
                QThread::msleep(1000);
            }
        }
        break;
    }

    // ── 状态2：点击任务 + 前往 ──
    case MainQuestState::ClickQuestAndGo:
    {
        questLog("点击任务列表 + 前往");

        // 第一步：滚动到底，确保看到最新任务
        QRect scrollBar;
        if (detectScrollbar(scrollBar)) {
            questLog(QString("检测到滚动条 (%1,%2,%3,%4)，拖到底")
                         .arg(scrollBar.x()).arg(scrollBar.y())
                         .arg(scrollBar.width()).arg(scrollBar.height()));
            // 点击滚动条底部
            clickAt(scrollBar.center().x(), scrollBar.bottom() - 5);
            QThread::msleep(500);
            clickAt(scrollBar.center().x(), scrollBar.bottom() - 5);
            QThread::msleep(500);
        }

        // 第二步：从下往上找金色任务条目
        QList<QRect> goldItems = findGoldButtons(questClickRoi);
        // 按 Y 坐标从大到小排序（底部优先）
        std::sort(goldItems.begin(), goldItems.end(), [](const QRect &a, const QRect &b) {
            return a.bottom() > b.bottom();
        });

        if (!goldItems.isEmpty()) {
            QRect &btn = goldItems.first();
            int qx = btn.center().x();
            int qy = btn.center().y();
            questLog(QString("视觉找到%1个金色条目，点最底部: (%2,%3)")
                         .arg(goldItems.size()).arg(qx).arg(qy));
            clickAt(qx, qy);
            QThread::msleep(800);
        } else {
            // 无金色条目，从下往上扫描点击
            questLog("无金色条目，从下往上扫描");
            int cx = questClickRoi.x() + questClickRoi.width() / 2;
            int bottom = questClickRoi.y() + questClickRoi.height();
            for (int cy = bottom - 10; cy >= questClickRoi.y(); cy -= 35) {
                clickAt(cx, cy);
                QThread::msleep(150);
            }
        }

        QThread::msleep(1800);

        // 第三步：在任务描述区找坐标链接并点击
        QList<QRect> coordLinks = findCoordinateLinks(questDescRoi);
        if (!coordLinks.isEmpty()) {
            QRect &link = coordLinks.first();
            int lx = link.center().x();
            int ly = link.center().y();
            questLog(QString("视觉找到坐标链接 at (%1,%2)，点击寻路").arg(lx).arg(ly));
            clickAt(lx, ly);
        } else {
            // 回退：尝试点击"追踪"按钮
            QList<QRect> trackBtns = findGoldButtons(goBtnSearchRoi);
            if (!trackBtns.isEmpty()) {
                QRect &btn = trackBtns.first();
                questLog(QString("未找到坐标，回退点追踪按钮 at (%1,%2)")
                             .arg(btn.center().x()).arg(btn.center().y()));
                clickAt(btn.center().x(), btn.center().y());
            } else {
                int bx = goBtnSearchRoi.x() + goBtnSearchRoi.width() * 2 / 3;
                int by = goBtnSearchRoi.y() + goBtnSearchRoi.height() / 2;
                questLog(QString("固定位追踪 (%1,%2)").arg(bx).arg(by));
                clickAt(bx, by);
            }
        }

        // ★ 关键：等3秒让任务面板关闭，再开始寻路检测
        questLog("等待面板关闭...");
        QThread::msleep(3000);

        transitionTo(MainQuestState::WaitAutoPath);
        break;
    }

    // ── 状态3：帧差等寻路到达 ──
    case MainQuestState::WaitAutoPath:
    {
        questLog("等待自动寻路...");
        int stableCount = 0;
        cv::Mat prevFrame;
        int dialogIgnoreFrames = 6;  // 前6个帧周期(3秒)不检测对话框

        for (int i = 0; i < autoPathTimeoutMs / frameCheckMs; i++) {
            QThread::msleep(frameCheckMs);
            if (!toRun) return;

            cv::Mat frame = screenToMat();
            if (frame.empty()) continue;

            // 对话框检测（延迟6帧避免误判面板关闭中）
            if (i >= dialogIgnoreFrames && isDialogOpen()) {
                questLog("寻路中检测到对话框");
                transitionTo(MainQuestState::DetectDialog);
                return;
            }

            if (!prevFrame.empty()) {
                // 帧差：比较中心区域
                cv::Rect centerRoi(panelCheckRoi.x(), panelCheckRoi.y(),
                                    panelCheckRoi.width(), panelCheckRoi.height());
                if (centerRoi.x + centerRoi.width <= frame.cols &&
                    centerRoi.y + centerRoi.height <= frame.rows) {
                    cv::Mat f1 = frame(centerRoi);
                    cv::Mat f2 = prevFrame(centerRoi);
                    cv::Mat diff;
                    cv::absdiff(f1, f2, diff);
                    double change = cv::mean(diff)[0];

                    if (change < 5.0) {
                        stableCount++;
                    } else {
                        stableCount = 0;
                    }

                    if (i % 5 == 0)
                        questLog(QString("帧%1 变化:%2 稳定:%3")
                                     .arg(i).arg(change, 0, 'f', 1).arg(stableCount));
                }
            }
            prevFrame = frame.clone();

            if (stableCount >= frameStableFrames) {
                questLog("✅ 寻路到达");
                break;
            }
        }

        transitionTo(MainQuestState::DetectDialog);
        break;
    }

    // ── 状态4：检测对话框 ──
    case MainQuestState::DetectDialog:
    {
        questLog("检测对话框...");

        // 等待对话框出现（最多10秒）
        for (int i = 0; i < 20; i++) {
            QThread::msleep(500);
            if (isDialogOpen()) {
                questLog("✅ 对话框已打开");
                transitionTo(MainQuestState::ClickDialogButton);
                return;
            }
        }

        questLog("⚠ 无对话框，可能任务已完成或卡住");
        // 尝试再开L重新开始
        transitionTo(MainQuestState::OpenQuestPanel);
        break;
    }

    // ── 状态5：点对话按钮 ──
    case MainQuestState::ClickDialogButton:
    {
        questLog("点击对话按钮");

        QList<QRect> btns = findGoldButtons(dialogBtnRoi);
        if (!btns.isEmpty()) {
            QRect &b = btns.first();
            clickAt(b.center().x(), b.center().y());
            questLog(QString("视觉点对话按钮 at (%1,%2)").arg(b.center().x()).arg(b.center().y()));
        } else {
            // 固定位：对话框底部中点偏右
            int bx = dialogBtnRoi.x() + dialogBtnRoi.width() * 2 / 3;
            int by = dialogBtnRoi.y() + dialogBtnRoi.height() / 2;
            clickAt(bx, by);
            questLog(QString("固定位点对话 (%1,%2)").arg(bx).arg(by));
        }

        QThread::msleep(2000);
        transitionTo(MainQuestState::CheckQuestProgress);
        break;
    }

    // ── 状态6：检查进度 ──
    case MainQuestState::CheckQuestProgress:
    {
        questLog("检查任务进度...");

        if (isDialogOpen()) {
            questLog("仍有对话框，继续点击");
            transitionTo(MainQuestState::ClickDialogButton);
        } else {
            questLog("对话框已关闭，任务可能完成");
            // 回到起点，重新打开面板获取下一个任务
            QThread::msleep(3000);
            transitionTo(MainQuestState::OpenQuestPanel);
        }
        break;
    }

    // ── 默认 ──
    default:
        transitionTo(MainQuestState::Idle);
        break;
    }
}

void MainQuestService::clientHandleRecMsg(const json &) {}
