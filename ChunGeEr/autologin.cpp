#include "autologin.h"
#include "gameutils.h"
#include "gameslot.h"
#include "LeoControl/mousekeyboardmanager.h"
#include "XuLog.h"
#include <QThread>
#include <QFileInfo>
#include <QCoreApplication>
#include <QSettings>
#include <windows.h>
#include <opencv2/imgproc.hpp>

AutoLogin::AutoLogin(GameSlot *slot, QObject *parent)
    : QObject(parent), m_slot(slot)
{
    m_timer = new QTimer(this);
    m_timer->setInterval(m_loopIntervalMs);
    connect(m_timer, &QTimer::timeout, this, &AutoLogin::processState);

    // 订阅 D3D11 桌面截图（用 DirectConnection 让回调在截图线程直接执行，
    // 不受工作线程 processState 中 msleep 阻塞事件循环的影响）
    connect(&ScreenCaptureManager::Instance(), &ScreenCaptureManager::capturedScreen,
            this, &AutoLogin::onCaptureScreen, Qt::DirectConnection);
}

AutoLogin::~AutoLogin()
{
    if (m_timer) {
        m_timer->stop();
    }
}

QString AutoLogin::phaseText() const
{
    static const char *texts[] = {
        "Idle","LaunchGame","WaitLauncher","ClickStartGame","WaitGameWindow",
        "WaitLoading",
        "DetectLogin","TypeAccount","TypePassword","ClickLoginBtn",
        "WaitServerSelect","ConfirmServer","WaitCharSelect","CharSelect","CharCreate","EnterGame",
        "VerifyInGame","ClosePopups","OpenSettings","GameSettings","FeatureSettings","PressF11","Done","Failed"
    };
    int idx = (int)m_phase;
    if (idx < 0 || idx >= (int)(sizeof(texts) / sizeof(texts[0])))
        return "Unknown";
    return texts[idx];
}

void AutoLogin::start(const QString &gamePath)
{
    m_gamePath = gamePath;
    m_retryCount = 0;
    m_phaseTicks = 0;
    m_launcherHwnd = nullptr;
    m_gameHwnd = nullptr;

    // 确保截图管理器在跑
    ScreenCaptureManager::Instance().startCapture();

    transitionTo(LoginPhase::LaunchGame);
    m_timer->start();
}

void AutoLogin::startPostInit()
{
    m_isPostInit = true;
    m_charCreateStep = 0;
    m_phaseTicks = 0;
    m_phase = LoginPhase::ClosePopups;
    loginLog("========== 开始初始化 ==========");
    m_timer->start();
}

void AutoLogin::cancel()
{
    m_timer->stop();
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(500);
    }
    m_phase = LoginPhase::Failed;
    emit finished(false);
}

// ════════════════════════════════════════════════
// D3D11 截图回调
// ════════════════════════════════════════════════
void AutoLogin::onCaptureScreen(ScreenCaptureManager::ScreenData data)
{
    m_picMutex.lock();
    m_curPic = data;
    bool valid = m_curPic.data && !m_curPic.data->empty();
    m_picMutex.unlock();
    int fc = ++m_frameCount;
    if (fc % 30 == 0) {
        loginLog(QString("[截图] #%1 %2x%3 valid=%4").arg(fc)
            .arg(valid ? data.des.Width : 0).arg(valid ? data.des.Height : 0).arg(valid));
    }
}

// ════════════════════════════════════════════════
// 截图 → cv::Mat
// ════════════════════════════════════════════════
cv::Mat AutoLogin::screenToMat()
{
    m_picMutex.lock();
    if (!m_curPic.data || m_curPic.data->empty()) {
        m_picMutex.unlock();
        return {};
    }
    cv::Mat img(m_curPic.des.Height, m_curPic.des.Width, CV_8UC4,
                m_curPic.data->data(), m_curPic.RowPitch);
    cv::Mat bgr;
    cv::cvtColor(img, bgr, cv::COLOR_BGRA2BGR);
    m_picMutex.unlock();
    return bgr;
}

// ════════════════════════════════════════════════
// 主状态机
// ════════════════════════════════════════════════
void AutoLogin::processState()
{
    m_phaseTicks++;

    switch (m_phase) {

    // ── 0. 启动游戏进程 ──
    case LoginPhase::LaunchGame:
    {
        loginLog("启动游戏进程...");
        if (m_process) {
            m_process->deleteLater();
            m_process = nullptr;
        }
        m_process = new QProcess;

        if (m_gamePath.endsWith(".lnk", Qt::CaseInsensitive)) {
            m_process->start("cmd", {"/c", "start", "", m_gamePath});
        } else {
            SHELLEXECUTEINFOW sei = {sizeof(sei)};
            sei.lpVerb = L"runas";
            sei.lpFile = reinterpret_cast<LPCWSTR>(m_gamePath.utf16());
            sei.lpDirectory = reinterpret_cast<LPCWSTR>(
                QFileInfo(m_gamePath).absolutePath().utf16());
            sei.nShow = SW_SHOWNORMAL;
            if (!::ShellExecuteExW(&sei)) {
                loginLog("❌ ShellExecuteEx 启动失败");
                emit statusMessage(QString("[窗口%1] 启动失败，请检查路径")
                    .arg(m_slot->index() + 1));
                cancel();
                return;
            }
        }
        transitionTo(LoginPhase::WaitLauncher);
        emit statusMessage(QString("[窗口%1] 进程已启动，等待启动器...")
            .arg(m_slot->index() + 1));
        break;
    }

    // ── 1. 等待启动器窗口 ──
    case LoginPhase::WaitLauncher:
    {
        HWND hwnd = FindWindowW(L"Qt5152QWindowIcon", nullptr);
        if (!hwnd) hwnd = FindWindowW(nullptr, L"大唐无双");

        if (hwnd) {
            m_launcherHwnd = hwnd;
            m_slot->setHwnd(hwnd);
            QThread::msleep(300);
            loginLog(QString("✅ 检测到启动器 hwnd=%1").arg((uintptr_t)hwnd, 0, 16));
            transitionTo(LoginPhase::ClickStartGame);
            emit statusMessage(QString("[窗口%1] 检测到启动器")
                .arg(m_slot->index() + 1));
            return;
        }

        if (m_phaseTicks > LAUNCHER_TIMEOUT) {
            loginLog("❌ 等待启动器超时");
            cancel();
        }
        break;
    }

    // ── 2. 检测并点击"开始游戏" ──
    case LoginPhase::ClickStartGame:
    {
        cv::Mat frame = screenToMat();
        if (frame.empty()) {
            if (++m_retryCount > MAX_RETRIES + 10) { cancel(); }
            return;
        }

        if (m_launcherHwnd) {
            QThread::msleep(200);
        }

        QPoint center;
        if (matchTemplate(frame, "\u5f00\u59cb\u6e38\u620f", &center, 0.80)) {
            loginLog(QString("✅ 检测到开始游戏按钮 (%1,%2)")
                .arg(center.x()).arg(center.y()));
            humanClick(center.x(), center.y());
            QThread::msleep(1500);
            transitionTo(LoginPhase::WaitGameWindow);
            emit statusMessage(QString("[窗口%1] 点击开始游戏，等待游戏窗口...")
                .arg(m_slot->index() + 1));
            m_retryCount = 0;
            return;
        }

        if (++m_retryCount > MAX_RETRIES) {
            loginLog("❌ 开始游戏按钮检测失败");
            cancel();
        }
        break;
    }

    // ── 3. 等待游戏客户端窗口 ──
    case LoginPhase::WaitGameWindow:
    {
        // 模糊匹配标题含"大唐"的窗口
        HWND hwnd = nullptr;
        HWND top = GetTopWindow(nullptr);
        while (top) {
            if (IsWindowVisible(top)) {
                WCHAR buf[256] = {0};
                GetWindowTextW(top, buf, 255);
                if (wcsstr(buf, L"\u5927\u5510")) {
                    hwnd = top;
                    break;
                }
            }
            top = GetNextWindow(top, GW_HWNDNEXT);
        }

        if (hwnd && hwnd != m_launcherHwnd) {
            m_gameHwnd = hwnd;
            m_slot->setHwnd(hwnd);
            QThread::msleep(500);
            QThread::msleep(500);

            loginLog(QString("✅ 游戏窗口已找到 hwnd=%1 pos=(-4,3) size=1048x837")
                .arg((uintptr_t)hwnd, 0, 16));
            transitionTo(LoginPhase::WaitLoading);
            emit statusMessage(QString("[窗口%1] 游戏窗口已找到，等待加载...")
                .arg(m_slot->index() + 1));
            m_retryCount = 0;
            return;
        }

        if (m_phaseTicks > LAUNCHER_TIMEOUT) {
            loginLog("❌ 等待游戏窗口超时");
            cancel();
        }
        break;
    }

    // ── 4. 等游戏加载，直到出现登录界面 ──
    case LoginPhase::WaitLoading:
    {
        cv::Mat frame = screenToMat();
        if (frame.empty()) return;

        auto &gu = GameUtils::Instance();
        auto locMatch = gu.detectLocation(frame);

        // 如果已在游戏中（可能是上次没退），跳过登录
        if (!locMatch.name.isEmpty() && locMatch.confidence > 0.6) {
            m_slot->setLoggedIn(true);
            loginLog(QString("✅ 已在游戏中 (地点=%1)")
                .arg(locMatch.name.toStdString()));
            transitionTo(LoginPhase::Done);
            emit statusMessage(QString("[窗口%1] 已在游戏中！")
                .arg(m_slot->index() + 1));
            return;
        }

        // 等登录界面出现（用整张登录界面模板匹配）
        bool hasLoginScreen = matchTemplate(frame, "\u767b\u5f55\u754c\u9762", nullptr, 0.80);
        if (hasLoginScreen) {
            loginLog("✅ 加载完成，检测到登录界面");

            // 窗口句柄可能已过期，重新确认
            if (m_gameHwnd && !::IsWindow(m_gameHwnd)) {
                HWND top = GetTopWindow(nullptr);
                while (top) {
                    if (IsWindowVisible(top)) {
                        WCHAR buf[256] = {0};
                        GetWindowTextW(top, buf, 255);
                        if (wcsstr(buf, L"\u5927\u5510")) {
                            m_gameHwnd = top;
                            m_slot->setHwnd(top);
                            break;
                        }
                    }
                    top = GetNextWindow(top, GW_HWNDNEXT);
                }
            }

            QThread::msleep(1000);
            transitionTo(LoginPhase::DetectLogin);
            m_retryCount = 0;
            return;
        }

        if (m_phaseTicks % 8 == 1) {
            loginLog(QString("等待加载... (%1s)").arg(m_phaseTicks * m_loopIntervalMs / 1000));
        }

        if (m_phaseTicks > 60) { // 48秒超时
            loginLog("加载超时，尝试直接输入");
            transitionTo(LoginPhase::DetectLogin);
        }
        break;
    }

    // ── 5. 检测登录界面 ──
    case LoginPhase::DetectLogin:
    {
        cv::Mat frame = screenToMat();
        if (frame.empty()) {
            if (++m_retryCount > MAX_RETRIES) { cancel(); }
            return;
        }

        // 用整张登录界面模板确认
        if (matchTemplate(frame, "\u767b\u5f55\u754c\u9762", nullptr, 0.80)) {
            loginLog("✅ 登录界面确认");

            // 登录界面出来后设窗口位置和大小（游戏已完全加载，SetWindowPos生效）
            if (m_gameHwnd) {
                QSettings settings(QCoreApplication::applicationDirPath() + "/config.ini", QSettings::IniFormat);
                int targetX = settings.value("GameWindow/PosX", 0).toInt();
                int targetY = settings.value("GameWindow/PosY", 0).toInt();
                int targetW = settings.value("GameWindow/Width", 1024).toInt();
                int targetH = settings.value("GameWindow/Height", 768).toInt();
                loginLog(QString("窗口目标: (%1,%2) %3x%4")
                    .arg(targetX).arg(targetY).arg(targetW).arg(targetH));

                ::SetWindowPos(m_gameHwnd, HWND_TOP, targetX, targetY,
                               targetW, targetH,
                               SWP_SHOWWINDOW);
                QThread::msleep(300);
                RECT wr;
                ::GetWindowRect(m_gameHwnd, &wr);
                loginLog(QString("定位后: (%1,%2) %3x%4")
                    .arg(wr.left).arg(wr.top)
                    .arg(wr.right-wr.left).arg(wr.bottom-wr.top));
            }

            transitionTo(LoginPhase::TypeAccount);
            m_retryCount = 0;
            return;
        }

        if (++m_retryCount > MAX_RETRIES) {
            loginLog("未检测到登录界面，尝试输入");
            transitionTo(LoginPhase::TypeAccount);
            m_retryCount = 0;
        }
        break;
    }

    // ── 6. 点击账号输入框 → 输入账号 ──
    case LoginPhase::TypeAccount:
    {
        cv::Mat frame = screenToMat();
        if (frame.empty()) {
            if (++m_retryCount > MAX_RETRIES) { cancel(); }
            return;
        }

        QString acc = m_slot->account();
        if (acc.isEmpty()) { cancel(); return; }

        // 找到账号标签，往右点（输入框在标签右边）
        QPoint accPt;
        if (matchTemplate(frame, "\u8d26\u53f7", &accPt, 0.80)) {
            int clickX = accPt.x() + 80;  // 往右偏移到输入框
            int clickY = accPt.y();
            loginLog(QString("✅ 检测到账号标签 (%1,%2) → 点击输入框 (%3,%4)")
                .arg(accPt.x()).arg(accPt.y()).arg(clickX).arg(clickY));
            humanClick(clickX, clickY);
            QThread::msleep(500);
        } else {
            loginLog("未检测到账号标签，Tab切换");
            pressKey(KEY_TAB); QThread::msleep(100);
            pressKey(KEY_TAB); QThread::msleep(500);
        }

        // 点击输入框后直接输入
        loginLog(QString("输入账号 %1").arg(acc));
        typeText(acc);
        QThread::msleep(200);
        pressKey(KEY_RETURN); QThread::msleep(150);
        pressKey(KEY_RETURN);  // 两次回车
        QThread::msleep(300);

        transitionTo(LoginPhase::TypePassword);
        break;
    }

    // ── 7. 点击密码输入框 → 输入密码 ──
    case LoginPhase::TypePassword:
    {
        cv::Mat frame = screenToMat();
        if (frame.empty()) { return; }

        QString pwd = m_slot->password();

        // 找到密码标签，往右点（输入框在标签右边）
        QPoint pwdPt;
        if (matchTemplate(frame, "\u5bc6\u7801", &pwdPt, 0.80)) {
            int clickX = pwdPt.x() + 80;  // 往右偏移到输入框
            int clickY = pwdPt.y();
            loginLog(QString("✅ 检测到密码标签 (%1,%2) → 点击输入框 (%3,%4)")
                .arg(pwdPt.x()).arg(pwdPt.y()).arg(clickX).arg(clickY));
            humanClick(clickX, clickY);
            QThread::msleep(500);
        } else {
            loginLog("未检测到密码标签，Tab切换");
            pressKey(KEY_TAB); QThread::msleep(500);
        }

        // 点击输入框后直接输入
        // 点击密码输入框后直接输入
        loginLog("输入密码 ***");
        typeText(pwd);
        QThread::msleep(200);

        transitionTo(LoginPhase::ClickLoginBtn);
        emit statusMessage(QString("[窗口%1] 账号密码已输入")
            .arg(m_slot->index() + 1));
        break;
    }

    // ── 8. 检测登录按钮 → 点击 ──
    case LoginPhase::ClickLoginBtn:
    {
        cv::Mat frame = screenToMat();
        QPoint center;
        bool found = !frame.empty() && matchTemplate(frame, "\u767b\u5f55", &center, 0.80);

        if (found) {
            loginLog(QString("✅ 点击登录按钮 (%1,%2)").arg(center.x()).arg(center.y()));
            humanClick(center.x(), center.y());
        } else {
            loginLog("未检测到登录按钮，按回车");
            pressKey(KEY_RETURN);
        }
        QThread::msleep(1500);

        transitionTo(LoginPhase::WaitServerSelect);
        emit statusMessage(QString("[窗口%1] 已点击登录，等待服务器...")
            .arg(m_slot->index() + 1));
        break;
    }

    // ── 9. 等待服务器选择 ──
    case LoginPhase::WaitServerSelect:
    {
        if (m_phaseTicks > 12) {
            transitionTo(LoginPhase::ConfirmServer);
            return;
        }
        cv::Mat frame = screenToMat();
        if (frame.empty()) return;

        if (matchTemplate(frame, "服务器确定", nullptr, 0.80)) {
            transitionTo(LoginPhase::ConfirmServer);
        }
        break;
    }

    // ── 10. 确认服务器 ──
    case LoginPhase::ConfirmServer:
    {
        cv::Mat frame = screenToMat();
        QPoint center;
        bool found = !frame.empty() && matchTemplate(frame, "服务器确定", &center, 0.80);

        if (found) {
            loginLog(QString("✅ 点击确认 (%1,%2)").arg(center.x()).arg(center.y()));
            humanClick(center.x(), center.y());
        } else {
            pressKey(KEY_RETURN);
        }
        QThread::msleep(2000);

        transitionTo(LoginPhase::WaitCharSelect);
        break;
    }

    // ── 11. 等待角色选择 ──
    // ── 11. 等待角色选择 → 只有一个角色, 默认已选中, 直接点进入游戏 ──
    case LoginPhase::WaitCharSelect:
    {
        // 等1秒让界面加载稳定
        if (m_phaseTicks <= 2) {
            if (m_phaseTicks == 1) QThread::msleep(1000);
            return;
        }
        if (m_phaseTicks > 20) {
            loginLog("⚠ 角色选择界面超时，尝试直接进入");
            transitionTo(LoginPhase::EnterGame);
            return;
        }
        cv::Mat frame = screenToMat();
        if (frame.empty()) return;

        // 先看"进入游戏"按钮：有=角色已选中, 直接进
        QPoint enterPt;
        if (matchTemplate(frame, "\u8fdb\u5165\u6e38\u620f", &enterPt, 0.80)) {
            loginLog(QString("点击进入游戏 (%1,%2)").arg(enterPt.x()).arg(enterPt.y()));
            humanClick(enterPt.x(), enterPt.y());
            transitionTo(LoginPhase::VerifyInGame);
            emit statusMessage(QString("[窗口%1] 进入游戏...").arg(m_slot->index() + 1));
            break;
        }

        // 没有进入游戏 → 可能没角色, 看"创建角色"按钮
        QPoint createBtn;
        if (matchTemplate(frame, "\u521b\u5efa\u89d2\u8272", &createBtn, 0.80)) {
            loginLog("🔍 无进入游戏按钮, 有创建角色 → 去创建");
            transitionTo(LoginPhase::CharCreate);
        }
        break;
    }

    // ── 12. 创建新角色 ──
    case LoginPhase::CharCreate:
    {
        cv::Mat frame = screenToMat();
        if (frame.empty()) return;


        // 阶段0: 在角色列表页，点"创建角色"
        if (m_charCreateStep == 0) {
            QPoint center;
            if (matchTemplate(frame, "\u521b\u5efa\u89d2\u8272", &center, 0.80)) {
                loginLog("点击创建角色");
                humanClick(center.x(), center.y());
                m_charCreateStep = 1;
                m_phaseTicks = 0;
            }
            return;
        }

        // 阶段1: 在创建界面，选门派
        if (m_charCreateStep == 1) {
            if (m_phaseTicks > 10) {
                loginLog("⚠️ 创建角色界面超时");
                transitionTo(LoginPhase::Failed);
                return;
            }
            QString faction = m_slot->charFaction();
            if (faction.isEmpty()) {
                loginLog("❌ 未配置门派，跳过创建");
                transitionTo(LoginPhase::Failed);
                return;
            }
            QPoint fc;
            if (matchTemplate(frame, faction, &fc, 0.80, "roles")) {
                loginLog(QString("选择门派: %1").arg(faction));
                humanClick(fc.x(), fc.y());
                QThread::msleep(500);
                m_charCreateStep = 2;
                m_phaseTicks = 0;
            } else {
                loginLog(QString("未找到门派模板: %1").arg(faction));
            }
            return;
        }

        // 阶段2: 点角色名输入框 → 输名字 → 必出验证码 → 阻塞等待
        if (m_charCreateStep == 2) {
            if (m_phaseTicks > 10) {
                loginLog("⚠️ 未找到角色名输入框");
                transitionTo(LoginPhase::Failed);
                return;
            }
            QPoint namePt;
            if (matchTemplate(frame, "\u89d2\u8272\u540d", &namePt, 0.80)) {
                // 标签在左，输入框在右
                humanClick(namePt.x() + 80, namePt.y());
                QThread::msleep(300);
                QString name = m_slot->charName();
                loginLog(QString("输入角色名: %1").arg(name));
                typeText(name);
                QThread::msleep(200);
                pressKey(KEY_RETURN);  // 回车确认名字
                QThread::msleep(300);
                // 验证码必出，立即阻塞等用户填写
                m_captchaPending = true;
                emit captchaRequired(m_slot->index());
                loginLog("🔐 验证码弹窗, 等待人工输入...");
                m_charCreateStep = 3;
                m_phaseTicks = 0;
            }
            return;
        }

        // 阶段3: 等验证码填完 → 点确认创建
        if (m_charCreateStep == 3) {
            if (m_captchaPending) return;
            if (m_phaseTicks > 60) {
                loginLog("⚠️ 确认创建超时");
                transitionTo(LoginPhase::Failed);
                return;
            }
            QPoint ok;
            if (matchTemplate(frame, "\u89d2\u8272\u547d\u540d\u786e\u5b9a", &ok, 0.80)) {
                loginLog("点击确认创建");
                humanClick(ok.x(), ok.y());
                QThread::msleep(5000);
                m_charCreateStep = 0;
                m_phaseTicks = 0;
                m_retryCount = 0;
                transitionTo(LoginPhase::CharSelect);  // 直接选角色进游戏，不回WaitCharSelect
            }
            return;
        }
        break;
    }

    // ── 14. 进入游戏（兜底，直接按回车）──
    case LoginPhase::EnterGame:
    {
        cv::Mat frame = screenToMat();
        QPoint center;
        if (!frame.empty() && matchTemplate(frame, "\u5f00\u59cb\u6e38\u620f", &center, 0.80)) {
            humanClick(center.x(), center.y());
        } else {
            pressKey(KEY_RETURN);
        }

        transitionTo(LoginPhase::VerifyInGame);
        emit statusMessage(QString("[窗口%1] 进入游戏...").arg(m_slot->index() + 1));
        break;
    }

    // ── 13. 验证已进入游戏 ──
    case LoginPhase::VerifyInGame:
    {
        // 等 60 tick（~48秒），游戏加载可能很慢
        if (m_phaseTicks > 60) {
            loginLog("❌ 登录验证超时");
            cancel();
            return;
        }

        cv::Mat frame = screenToMat();
        if (frame.empty()) return;

        auto &gu = GameUtils::Instance();
        auto locMatch = gu.detectLocation(frame);
        bool hasLocation = !locMatch.name.isEmpty() && locMatch.confidence > 0.6;

        if (hasLocation) {
            m_slot->setLoggedIn(true);
            loginLog(QString("✅ 登录成功！ location=%1 → Done")
                .arg(locMatch.name.toStdString()));
            transitionTo(LoginPhase::Done);
            emit statusMessage(QString("[窗口%1] 登录完成")
                .arg(m_slot->index() + 1));
            return;
        }
        break;
    }

    // ── 关闭初始弹窗 ──
    case LoginPhase::ClosePopups:
    {
        if (m_phaseTicks > 30) {
            loginLog("弹窗已处理（超时）");
            transitionTo(LoginPhase::OpenSettings);
            return;
        }
        cv::Mat frame = screenToMat();
        if (frame.empty()) return;

        auto &gu = GameUtils::Instance();
        QString dir = gu.templateRoot() + "/popups";

        if (m_charCreateStep == 0) {
            auto r1 = gu.bestMatch(frame, dir, "\u8df3\u8fc7\u6e38\u620f\u521d\u59cb\u9636\u6bb5");
            loginLog(QString("跳过游戏初始阶段 conf=%1 at=(%2,%3)")
                .arg(r1.confidence, 0, 'f', 3).arg(r1.centerX).arg(r1.centerY));
            if (r1.confidence > 0.80) {
                loginLog("→ 点取消");
                auto r2 = gu.bestMatch(frame, dir, "\u8df3\u8fc7\u5f39\u7a97\u53d6\u6d88");
                if (r2.confidence > 0.80)
                    humanClick(r2.centerX, r2.centerY);
                else
                    humanClick(r1.centerX + 200, r1.centerY + 150);
                QThread::msleep(1000);
                m_charCreateStep = 1;
                m_phaseTicks = 0;
                return;
            }
            // 没匹配到 → 弹窗不存在，直接下一步
            m_charCreateStep = 1;
            return;
        }

        if (m_charCreateStep == 1) {
            auto r3 = gu.bestMatch(frame, dir, "\u5c55\u5f00\u7684\u6d3b\u52a8");
            loginLog(QString("展开的活动 conf=%1 at=(%2,%3)")
                .arg(r3.confidence, 0, 'f', 3).arg(r3.centerX).arg(r3.centerY));
            if (r3.confidence > 0.80) {
                loginLog("→ 关闭");
                humanClick(r3.centerX, r3.centerY);
                QThread::msleep(1000);
            }
            loginLog("弹窗处理完毕");
            transitionTo(LoginPhase::OpenSettings);
        }
        break;
    }

    // ── 点击设置图标 ──
    case LoginPhase::OpenSettings:
    {
        if (m_phaseTicks > 10) {
            loginLog("设置图标超时，跳过");
            transitionTo(LoginPhase::PressF11);
            return;
        }
        cv::Mat frame = screenToMat();
        if (frame.empty()) return;

        QPoint pt;
        if (matchTemplate(frame, "\u8bbe\u7f6e", &pt, 0.70, "icons")) {
            humanClick(pt.x(), pt.y());
            loginLog(QString("点击设置图标 → 等待界面打开... (帧#%1)").arg(m_frameCount));
            QThread::msleep(1500);
            // —— 调试：保存点击设置后的画面 ——
            cv::Mat frame2 = screenToMat();
            loginLog(QString("调试帧 帧#%1, 尺寸=%2x%3").arg(m_frameCount).arg(frame2.cols).arg(frame2.rows));
            if (!frame2.empty()) {
                QString debugPath = QString::fromUtf8("debug_open_settings_%1.png").arg(m_frameCount);
                cv::imwrite(debugPath.toStdString(), frame2);
                loginLog(QString("已保存 %1").arg(debugPath));
            }
            transitionTo(LoginPhase::GameSettings);
        }
        break;
    }

    // ── 点游戏设置 → 最低画质 ──
    case LoginPhase::GameSettings:
    {
        if (m_phaseTicks > 20) {
            loginLog("游戏设置超时，跳过");
            transitionTo(LoginPhase::FeatureSettings);
            return;
        }
        cv::Mat frame = screenToMat();
        if (frame.empty()) return;

        QPoint pt;
        // 先用低阈值找"游戏设置"tab定位设置面板
        if (matchTemplate(frame, "\u6e38\u620f\u8bbe\u7f6e", &pt, 0.55, "settings")) {
            // 竖排文字模板，点文字右侧的tab按钮区域
            humanClick(pt.x() + 40, pt.y());
            QThread::msleep(500);
            frame = screenToMat();
            auto &gu = GameUtils::Instance();
            // 从ROI配置获取设置面板（窗口相对→屏幕绝对）
            QRect roi = gu.settingsPanelROI();
            cv::Rect panel;
            if (!roi.isEmpty() && m_gameHwnd) {
                RECT wr; GetWindowRect(m_gameHwnd, &wr);
                panel = cv::Rect(wr.left + roi.x(), wr.top + roi.y(), roi.width(), roi.height());
            }
            panel &= cv::Rect(0, 0, frame.cols, frame.rows);
            if (panel.width > 0 && panel.height > 0) {
                cv::Mat panelFrame = frame(panel);
                auto mr = gu.bestMatch(panelFrame, gu.templateRoot() + "/settings", "\u6700\u4f4e\u753b\u8d28");
                if (mr.confidence > 0.90) {
                    loginLog("点击最低画质");
                    humanClick(panel.x + mr.centerX, panel.y + mr.centerY);
                    QThread::msleep(300);
                }
            }
            transitionTo(LoginPhase::FeatureSettings);
            emit statusMessage(QString("[窗口%1] 功能设置...").arg(m_slot->index() + 1));
        }
        break;
    }

    // ── 点击功能 → 批量开关 ──
    case LoginPhase::FeatureSettings:
    {
        if (m_phaseTicks > 20) {
            loginLog("功能设置超时，跳过");
            transitionTo(LoginPhase::PressF11);
            return;
        }
        cv::Mat frame = screenToMat();
        if (frame.empty()) return;

        QPoint pt;
        if (matchTemplate(frame, "\u529f\u80fd", &pt, 0.90, "settings")) {
            humanClick(pt.x(), pt.y(), 300);
            QThread::msleep(5000);
            loginLog("切换至功能页，批量关闭开关...");
            frame = screenToMat();
            auto &gu = GameUtils::Instance();
            cv::imwrite((gu.templateRoot() + "/../debug_feature_frame.png").toStdString(), frame);
            loginLog(QString("已保存调试帧: %1x%2 (帧#%3)").arg(frame.cols).arg(frame.rows).arg(m_frameCount));
            // 从ROI配置获取设置面板（窗口相对→屏幕绝对）
            QRect roi = gu.settingsPanelROI();
            cv::Rect panel;
            if (!roi.isEmpty() && m_gameHwnd) {
                RECT wr; GetWindowRect(m_gameHwnd, &wr);
                panel = cv::Rect(wr.left + roi.x(), wr.top + roi.y(), roi.width(), roi.height());
            }
            panel &= cv::Rect(0, 0, frame.cols, frame.rows);

            static const QStringList toggles = {
                "\u5173\u95ed\u4ed9\u5b50\u6307\u5357",
                "\u5173\u95ed\u65b0\u624b\u5e2e\u52a9",
                "\u5173\u95ed\u804a\u5929\u6ce1\u6ce1",
                "\u53d6\u6d88\u4efb\u52a1\u680f\u95ea\u70c1",
                "\u6218\u573a\u81ea\u52a8\u7ec4\u961f",
                "\u62d2\u7edd\u5207\u78cb",
                "\u9690\u85cf\u5934\u76d4",
                "\u81ea\u52a8\u53d1\u9001\u9519\u8bef\u6587\u4ef6"
            };

            if (panel.width > 0 && panel.height > 0) {
                cv::Mat panelFrame = frame(panel);
                cv::imwrite((gu.templateRoot() + "/../debug_feature_frame_1.png").toStdString(), panelFrame);
                for (const QString &tpl : toggles) {
                    if (m_phase != LoginPhase::FeatureSettings) return;
                    auto r = gu.bestMatch(panelFrame, gu.templateRoot() + "/settings", tpl);
                    if (r.confidence > 0.80) {
                        loginLog(QString("点击: %1 conf=%.3f").arg(tpl).arg(r.confidence));
                        humanClick(panel.x + r.centerX, panel.y + r.centerY);
                        QThread::msleep(300);
                        frame = screenToMat();
                        QThread::msleep(300);
                        if (panel.width > 0 && panel.height > 0)
                            panelFrame = frame(panel);
                    }
                }
            }

            loginLog(QString("功能设置完成 (帧#%1)").arg(m_frameCount));
            // 点击设置确定关闭弹窗
            if (matchTemplate(frame, "\u8bbe\u7f6e\u786e\u5b9a", &pt, 0.70, "settings")) {
                humanClick(pt.x(), pt.y(), 200);
                loginLog("点击设置确定关闭弹窗");
                QThread::msleep(1000);
                // 点击返回游戏
                frame = screenToMat();
                if (matchTemplate(frame, "\u8fd4\u56de\u6e38\u620f", &pt, 0.70, "settings")) {
                    humanClick(pt.x(), pt.y(), 200);
                    loginLog("点击返回游戏");
                }
            } else {
                loginLog("未找到设置确定按钮");
            }
            // —— 调试：保存功能页处理后的画面 ——
            cv::Mat debugFrame = screenToMat();
            if (!debugFrame.empty()) {
                cv::imwrite((gu.templateRoot() + "/../debug_feature_done.png").toStdString(), debugFrame);
                loginLog(QString("已保存 debug_feature_done.png (帧#%1)").arg(m_frameCount));
            }
            transitionTo(LoginPhase::PressF11);
        }
        break;
    }

    // ── F11屏蔽其他玩家 ──
    case LoginPhase::PressF11:
    {
        pressKey(KEY_F11);
        loginLog(QString("按F11屏蔽其他玩家 (帧#%1)").arg(m_frameCount));
        QThread::msleep(500);
        // —— 调试：保存关闭设置后的画面 ——
        cv::Mat frame = screenToMat();
        loginLog(QString("F11后 帧#%1, 尺寸=%2x%3").arg(m_frameCount).arg(frame.cols).arg(frame.rows));
        if (!frame.empty()) {
            cv::imwrite((GameUtils::Instance().templateRoot() + "/../debug_after_f11.png").toStdString(), frame);
            loginLog(QString("已保存 debug_after_f11.png (帧#%1)").arg(m_frameCount));
        }
        transitionTo(LoginPhase::Done);
        break;
    }

    // ── 登录/初始化完成 ──
    case LoginPhase::Done:
    {
        if (m_isPostInit) {
            loginLog("========== 初始化完成 ==========");
            m_isPostInit = false;
            m_timer->stop();
            emit postInitDone(m_slot->index());
        } else {
            loginLog("登录完成");
            m_timer->stop();
            emit finished(true);
        }
        break;
    }

    // ── 失败 ──
    case LoginPhase::Failed:
        m_timer->stop();
        emit finished(false);
        break;

    default:
        break;
    }
}

// ════════════════════════════════════════════════
// 视觉检测
// ════════════════════════════════════════════════
bool AutoLogin::matchTemplate(const cv::Mat &frame, const QString &tplName,
                              QPoint *outCenter, double minConf, const QString &subDir)
{
    if (frame.empty()) return false;
    auto &gu = GameUtils::Instance();
    QString dir = subDir.isEmpty() ? gu.templateRoot() + "/login" : gu.templateRoot() + "/" + subDir;
    auto result = gu.bestMatch(frame, dir, tplName);
    bool ok = result.confidence > minConf;
    if (ok && outCenter) {
        outCenter->setX(result.centerX);
        outCenter->setY(result.centerY);
    }
    return ok;
}

// ════════════════════════════════════════════════
// 动作
// ════════════════════════════════════════════════
void AutoLogin::humanClick(int sx, int sy, int delayMs)
{
    auto &km = MouseKeyboardManager::Instance();
    km.mouseMoveDirect(sx, sy);
    if (delayMs > 0) QThread::msleep(delayMs);
    km.mouseClick();
    loginLog(QString("点击 (%1,%2)").arg(sx).arg(sy));
}

void AutoLogin::typeText(const QString &text)
{
    auto &km = MouseKeyboardManager::Instance();
    for (QChar ch : text) {
        char c = ch.toLatin1();

        if (c == '@') {
            // @ = Shift + 2
            km.keyPress(KEY_LEFT_SHIFT);  QThread::msleep(30);
            km.clickButton('2');
            km.keyRelease(KEY_LEFT_SHIFT);
        } else {
            km.clickButton(c);
        }
        QThread::msleep(50 + (rand() % 70));
    }
}

void AutoLogin::pressKey(int vkCode)
{
    MouseKeyboardManager::Instance().clickButton(vkCode);
}

// ════════════════════════════════════════════════
// 工具
// ════════════════════════════════════════════════
void AutoLogin::transitionTo(LoginPhase next)
{
    if (m_phase != next) {
        LoginPhase old = m_phase;
        m_phase = next;
        m_phaseTicks = 0;
        m_retryCount = 0;
        if (next == LoginPhase::ClosePopups) m_charCreateStep = 0;  // 复用子步骤计数
        loginLog(QString("[%1 → %2]").arg(phaseName(old)).arg(phaseName(next)));
    }
}

QString AutoLogin::phaseName(LoginPhase p) const
{
    static const char *texts[] = {
        "Idle","LaunchGame","WaitLauncher","ClickStartGame","WaitGameWindow",
        "WaitLoading",
        "DetectLogin","TypeAccount","TypePassword","ClickLoginBtn",
        "WaitServerSelect","ConfirmServer","WaitCharSelect","CharSelect","CharCreate","EnterGame",
        "VerifyInGame","ClosePopups","OpenSettings","GameSettings","FeatureSettings","PressF11","Done","Failed"
    };
    int idx = (int)p;
    if (idx < 0 || idx >= (int)(sizeof(texts) / sizeof(texts[0])))
        return "???";
    return texts[idx];
}

void AutoLogin::loginLog(const QString &msg)
{
    QString log = QString("[登录%1] %2").arg(m_slot->index() + 1).arg(msg);
    infof(log.toStdString());
}

void AutoLogin::onCaptchaDone()
{
    loginLog("验证码已填，继续创建角色");
    m_captchaPending = false;
}
