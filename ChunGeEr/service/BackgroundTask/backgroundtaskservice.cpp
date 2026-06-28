#include "backgroundtaskservice.h"
#include "XuLog.h"
#include "gameslot.h"
#include "LeoControl/mousekeyboardmanager.h"
#include <QSettings>
#include <QCoreApplication>
#include <QThread>
#include <QDateTime>
#include <QRandomGenerator>

BackgroundTaskService::BackgroundTaskService(QObject *parent)
    : BaseService(parent)
{
}

void BackgroundTaskService::startService()
{
    bgLog("后台任务服务启动");

    // 从 config.ini 加载 ROI
    QString iniPath = QCoreApplication::applicationDirPath() + "/config.ini";
    QSettings s(iniPath, QSettings::IniFormat);

    if (s.contains("ROIs/LoginReward")) {
        QStringList parts = s.value("ROIs/LoginReward").toString().split(',');
        if (parts.size() == 4)
            loginRewardRoi = QRect(parts[0].toInt(), parts[1].toInt(),
                                   parts[2].toInt(), parts[3].toInt());
    }
    bgLog(QString("登录奖励ROI: (%1,%2,%3,%4)")
              .arg(loginRewardRoi.x()).arg(loginRewardRoi.y())
              .arg(loginRewardRoi.width()).arg(loginRewardRoi.height()));

    if (s.contains("ROIs/Disconnect")) {
        QStringList dp = s.value("ROIs/Disconnect").toString().split(',');
        if (dp.size() == 4)
            disconnectRoi = QRect(dp[0].toInt(), dp[1].toInt(),
                                   dp[2].toInt(), dp[3].toInt());
    }
    bgLog(QString("掉线检测ROI: (%1,%2,%3,%4)")
              .arg(disconnectRoi.x()).arg(disconnectRoi.y())
              .arg(disconnectRoi.width()).arg(disconnectRoi.height()));

    toRun = true;
    detectGameWindow();
}

void BackgroundTaskService::stopService()
{
    bgLog("后台任务服务停止");
    toRun = false;
}

void BackgroundTaskService::run()
{
    startService();

    int heartbeatCounter = 0;
    while (toRun) {
        QThread::msleep(2000);  // 每2秒检测一次

        if (!toRun) break;

        // 心跳日志：每30秒（15轮）打一条，在暂停/无帧检查之前输出
        if (++heartbeatCounter >= 15) {
            heartbeatCounter = 0;
            bool hasFrame = (curPic.data && !curPic.data->empty());
            bgLog(QString("心跳 cnt=15 paused=%1 frame=%2")
                  .arg(m_paused.loadRelaxed()).arg(hasFrame ? 1 : 0));
        }
        infof("heartbeatCounter :{}",heartbeatCounter);
        // 上层任务暂停时，后台也跳过（避免冲突）
        if (m_paused.loadRelaxed() == 1) continue;

        // 截图有效性检查
        picMutex.lock();
        bool hasFrame = (curPic.data && !curPic.data->empty());
        picMutex.unlock();
        if (!hasFrame) continue;

        // 登录奖励领取
        if (checkLoginReward()) {
            // 领取后等待一段时间避免重复触发
            QThread::msleep(3000);
        }

        // 掉线检测与重连
        if (checkDisconnect()) {
            // 重连后等待较长时间避免重复触发
            QThread::msleep(5000);
        }
    }
}

// ════════════════════════════════════════
// 登录奖励领取
// ════════════════════════════════════════

bool BackgroundTaskService::checkLoginReward()
{
    if (loginRewardRoi.isNull() || loginRewardRoi.isEmpty()) return false;

    cv::Mat screen = screenToMat();
    if (screen.empty()) return false;

    // 在登录奖励ROI区域内找"领取"文字
    QRect r = offsetROI(loginRewardRoi);
    int x = qBound(0, r.x(), screen.cols - 1);
    int y = qBound(0, r.y(), screen.rows - 1);
    int w = qMin(r.width(),  screen.cols - x);
    int h = qMin(r.height(), screen.rows - y);
    if (w <= 0 || h <= 0) return false;

    cv::Mat crop = screen(cv::Rect(x, y, w, h)).clone();

    QRect matchRect;
    bool found = false;

    // 方案1: 字库查找
    if (!BitmapFontLib::Instance().isEmpty()) {
        auto matches = BitmapFontLib::Instance().findString(crop, 0.85);
        if (!matches.empty()) {
            for (const auto &m : matches) {
                if (m.charName.find("领取") != std::string::npos ||
                    m.charName.find("领") != std::string::npos) {
                    matchRect = QRect(x + m.x, y + m.y, m.width, m.height);
                    found = true;
                    bgLog(QString("字库匹配到'领取' at (%1,%2) sim=%3")
                              .arg(matchRect.center().x())
                              .arg(matchRect.center().y())
                              .arg(m.similarity, 0, 'f', 3));
                    break;
                }
            }
        }
    }

    // 方案2: 模板图片查找
    if (!found) {
        QRect tmpl = findTemplate("popups/领取", 0.80);
        if (!tmpl.isNull()) {
            matchRect = tmpl;
            found = true;
            bgLog(QString("模板匹配到'领取' at (%1,%2)")
                      .arg(matchRect.center().x())
                      .arg(matchRect.center().y()));
        }
    }

    if (!found) return false;

    // 通知上层任务暂停
    emit pauseRequested();

    // 等待上层暂停
    QThread::msleep(200);

    // 点击"领取"上方30像素
    int clickX = matchRect.center().x();
    int clickY = matchRect.center().y() - 30;
    bgLog(QString("点击领取: (%1,%2) [文字中心上方30px]").arg(clickX).arg(clickY));
    clickAt(clickX, clickY);

    // 等待领取动画/弹窗
    QThread::msleep(2000);

    // 通知上层任务恢复
    emit resumeRequested();

    return true;
}

// ════════════════════════════════════════
// 掉线检测与重连
// ════════════════════════════════════════

bool BackgroundTaskService::checkDisconnect()
{

    // 冷却：距上次重连不足30秒，跳过
    qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    //infof("checkDisconnect,nowMs:{},m_lastReconnectMs:{}",nowMs,m_lastReconnectMs);
    if (nowMs - m_lastReconnectMs < 30000) return false;

    cv::Mat screen = screenToMat();
    if (screen.empty())
    {
        infof("screen empty");
        return false;
    }

    // ── 步骤1: 检测登录界面（= 确定掉线，不会误判）──
    QRect loginRect = findTemplate("login/登录界面", 0.75);
    if (loginRect.isNull())
    {
        return false;  // 没掉线
    }

    bgLog("检测到登录界面 → 掉线确认");
    emit pauseRequested();
    QThread::msleep(300);

    // ── 步骤2: 检查是否有"服务器断开连接"弹窗 ──
    // 在登录界面ROI或中心区域内搜索
    QRect dcROI = disconnectRoi;
    if (dcROI.isNull() || dcROI.isEmpty()) {
        // 没有配置掉线ROI → 用屏幕中心800x600
        int sx = screen.cols, sy = screen.rows;
        int cw = qMin(800, sx), ch = qMin(600, sy);
        dcROI = QRect(sx/2 - cw/2, sy/2 - ch/2, cw, ch);
    }
    cv::Rect dcCv(dcROI.x(), dcROI.y(), dcROI.width(), dcROI.height());
    cv::Mat dcCrop = screen(dcCv).clone();

    bool hasPopup = false;
    // 字库查找
    if (!BitmapFontLib::Instance().isEmpty()) {
        auto matches = BitmapFontLib::Instance().findString(dcCrop, 0.85);
        for (const auto &m : matches) {
            if (m.charName.find("服务器") != std::string::npos ||
                m.charName.find("断开") != std::string::npos ||
                m.charName.find("连接") != std::string::npos) {
                hasPopup = true;
                bgLog(QString("字库检测到掉线弹窗: '%1' at (%2,%3) sim=%4")
                          .arg(QString::fromStdString(m.charName))
                          .arg(dcROI.x() + m.x).arg(dcROI.y() + m.y)
                          .arg(m.similarity, 0, 'f', 3));
                break;
            }
        }
    }
    if (!hasPopup) {
        QRect r = findTemplate("popups/服务器断开连接", 0.75);
        if (!r.isNull()) {
            hasPopup = true;
            bgLog(QString("模板匹配到掉线弹窗 at (%1,%2)")
                      .arg(r.center().x()).arg(r.center().y()));
        }
    }

    // ── 步骤3: 有弹窗 → 点击"确定"关掉 ──
    if (hasPopup) {
        bool clickedOk = false;
        if (!BitmapFontLib::Instance().isEmpty()) {
            auto matches = BitmapFontLib::Instance().findString(dcCrop, 0.85);
            for (const auto &m : matches) {
                if (m.charName.find("确定") != std::string::npos) {
                    int cx = dcROI.x() + m.x + m.width / 2;
                    int cy = dcROI.y() + m.y + m.height / 2;
                    bgLog(QString("点击'确定' (%1,%2)").arg(cx).arg(cy));
                    clickAt(cx, cy);
                    clickedOk = true;
                    break;
                }
            }
        }
        if (!clickedOk) {
            QRect ok = findTemplate("popups/确定", 0.75);
            if (!ok.isNull()) {
                clickAt(ok.center().x(), ok.center().y());
                clickedOk = true;
            }
        }
        if (!clickedOk) {
            bgLog("未找到确定按钮，按回车");
            MouseKeyboardManager::Instance().clickButton(KEY_RETURN);
        }
        QThread::msleep(2000);
    } else {
        bgLog("登录界面无掉线弹窗，直接登录");
    }

    // ── 步骤4: 输入账号密码登录 ──
    if (m_account.isEmpty() || m_password.isEmpty()) {
        bgLog("❌ 重连失败：未配置账号密码");
        m_lastReconnectMs = nowMs;
        emit resumeRequested();
        return false;
    }

    // 等待账号输入框出现
    {
        bool gotAccount = false;
        for (int i = 0; i < 10; i++) {
            QThread::msleep(1000);
            cv::Mat frame = screenToMat();
            if (frame.empty()) continue;
            QRect acc = findTemplate("login/账号", 0.75);
            if (!acc.isNull()) {
                int cx = acc.center().x() + 80;
                int cy = acc.center().y();
                bgLog(QString("点击账号输入框 (%1,%2)").arg(cx).arg(cy));
                clickAt(cx, cy);
                QThread::msleep(500);
                gotAccount = true;
                break;
            }
        }
        if (!gotAccount) {
            bgLog("Tab切换到账号框");
            MouseKeyboardManager::Instance().clickButton(KEY_TAB);
            QThread::msleep(100);
            MouseKeyboardManager::Instance().clickButton(KEY_TAB);
            QThread::msleep(500);
        }
    }

    // 输入账号
    bgLog(QString("输入账号 %1").arg(m_account));
    {
        auto &km = MouseKeyboardManager::Instance();
        for (QChar ch : m_account) {
            char c = ch.toLatin1();
            if (c == '@') {
                km.keyPress(KEY_LEFT_SHIFT);  QThread::msleep(30);
                km.clickButton('2');
                km.keyRelease(KEY_LEFT_SHIFT);
            } else {
                km.clickButton(c);
            }
            QThread::msleep(50 + QRandomGenerator::global()->bounded(70));
        }
        QThread::msleep(200);
        km.clickButton(KEY_RETURN);  QThread::msleep(150);
        km.clickButton(KEY_RETURN);  QThread::msleep(300);
    }

    // ── 步骤5: 输入密码 ──
    {
        cv::Mat frame = screenToMat();
        if (!frame.empty()) {
            QRect pwd = findTemplate("login/密码", 0.75);
            if (!pwd.isNull()) {
                int cx = pwd.center().x() + 80;
                int cy = pwd.center().y();
                clickAt(cx, cy);
                QThread::msleep(500);
            } else {
                MouseKeyboardManager::Instance().clickButton(KEY_TAB);
                QThread::msleep(500);
            }
        }
    }

    bgLog("输入密码 ***");
    {
        auto &km = MouseKeyboardManager::Instance();
        for (QChar ch : m_password) {
            char c = ch.toLatin1();
            if (c == '@') {
                km.keyPress(KEY_LEFT_SHIFT);  QThread::msleep(30);
                km.clickButton('2');
                km.keyRelease(KEY_LEFT_SHIFT);
            } else {
                km.clickButton(c);
            }
            QThread::msleep(50 + QRandomGenerator::global()->bounded(70));
        }
        QThread::msleep(200);
    }

    // ── 步骤6: 点击登录 ──
    {
        QRect btn = findTemplate("login/登录", 0.75);
        if (!btn.isNull()) {
            clickAt(btn.center().x(), btn.center().y());
        } else {
            MouseKeyboardManager::Instance().clickButton(KEY_RETURN);
        }
    }
    QThread::msleep(2000);

    // ── 步骤7: 服务器确定 ──
    for (int i = 0; i < 12; i++) {
        QThread::msleep(1000);
        QRect srv = findTemplate("login/服务器确定", 0.75);
        if (!srv.isNull()) {
            clickAt(srv.center().x(), srv.center().y());
            break;
        }
    }
    QThread::msleep(3000);

    // ── 步骤8: 等待进入游戏 ──
    bool reconnected = false;
    for (int i = 0; i < 30; i++) {
        QThread::msleep(2000);
        cv::Mat frame = screenToMat();
        if (frame.empty()) continue;

        // 又掉线了？
        QRect dcAgain = findTemplate("login/登录界面", 0.75);
        if (!dcAgain.isNull()) {
            bgLog("❌ 重连后再次出现登录界面，放弃");
            break;
        }

        // 检测角色选择 → 点进入游戏
        QRect enter = findTemplate("login/进入游戏", 0.75);
        if (!enter.isNull()) {
            clickAt(enter.center().x(), enter.center().y());
            QThread::msleep(3000);
            continue;
        }

        // 无登录界面 = 已进入游戏
        reconnected = true;
        break;
    }

    m_lastReconnectMs = QDateTime::currentMSecsSinceEpoch();

    if (reconnected) {
        bgLog("✅ 重连成功！");

        // ── 步骤9: 关闭活动弹窗 ──
        QThread::msleep(3000);  // 等游戏画面稳定
        for (int i = 0; i < 3; i++) {
            QRect actRect = findTemplate("popups/展开的活动", 0.75);
            if (!actRect.isNull()) {
                bgLog(QString("关闭活动弹窗 (%1,%2)")
                          .arg(actRect.center().x()).arg(actRect.center().y()));
                clickAt(actRect.center().x(), actRect.center().y());
                QThread::msleep(1000);
            } else {
                break;
            }
        }

        // ── 步骤10: 按F11屏蔽其他玩家 ──
        bgLog("按F11屏蔽其他玩家");
        MouseKeyboardManager::Instance().clickButton(KEY_F11);
        QThread::msleep(500);
    } else {
        bgLog("⚠ 重连结果不确定，恢复任务运行");
    }

    emit resumeRequested();
    return true;
}

void BackgroundTaskService::bgLog(const QString &msg)
{
    QString logstr = QString("[后台] %1").arg(msg);
    infof(logstr.toStdString());
    emit log(logstr);
}
