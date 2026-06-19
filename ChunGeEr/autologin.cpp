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

    // 订阅 D3D11 桌面截图
    connect(&ScreenCaptureManager::Instance(), &ScreenCaptureManager::capturedScreen,
            this, &AutoLogin::onCaptureScreen);
}

QString AutoLogin::phaseText() const
{
    static const char *texts[] = {
        "Idle","LaunchGame","WaitLauncher","ClickStartGame","WaitGameWindow",
        "WaitLoading",
        "DetectLogin","TypeAccount","TypePassword","ClickLoginBtn",
        "WaitServerSelect","ConfirmServer","WaitCharSelect","CharSelect","CharCreate","EnterGame",
        "VerifyInGame","Done","Failed"
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

void AutoLogin::cancel()
{
    m_timer->stop();
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(2000);
    }
    transitionTo(LoginPhase::Failed);
    emit finished(false);
}

// ════════════════════════════════════════════════
// D3D11 截图回调
// ════════════════════════════════════════════════
void AutoLogin::onCaptureScreen(ScreenCaptureManager::ScreenData data)
{
    m_picMutex.lock();
    m_curPic = data;
    m_picMutex.unlock();
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
            emit finished(true);
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
        m_retryCount++;
        break;
    }

    // ── 11. 等待角色选择 ──
    case LoginPhase::WaitCharSelect:
    {
        if (m_phaseTicks > 20) {
            loginLog("⚠ 角色选择界面超时，尝试直接进入");
            transitionTo(LoginPhase::EnterGame);
            return;
        }
        cv::Mat frame = screenToMat();
        if (frame.empty()) return;

        // 判断有无角色：灰色空槽位 vs 金色角色按钮
        bool noChar = matchTemplate(frame, "\u6ca1\u6709\u89d2\u8272", nullptr, 0.80);
        bool canCreate = matchTemplate(frame, "\u521b\u5efa\u89d2\u8272", nullptr, 0.80);

        if (noChar || canCreate) {
            if (noChar) {
                loginLog("🔍 灰色空槽位 → 无角色");
                transitionTo(LoginPhase::CharCreate);
            } else {
                loginLog("🔍 金色角色按钮 → 有角色");
                transitionTo(LoginPhase::CharSelect);
            }
        }
        break;
    }

    // ── 12. 选择已有角色 → 进入游戏 ──
    case LoginPhase::CharSelect:
    {
        loginLog(QString("有角色, 进入游戏..."));
        cv::Mat frame = screenToMat();
        if (frame.empty()) return;

        QPoint createBtn;
        if (matchTemplate(frame, "\u521b\u5efa\u89d2\u8272", &createBtn, 0.80)) {
            // 角色槽位在"创建角色"按钮上方
            // TODO: 用金色角色按钮模板精确点击
            QPoint charSlot(createBtn.x(), createBtn.y() - 150);
            loginLog(QString("点击角色槽位 (%1,%2)").arg(charSlot.x()).arg(charSlot.y()));
            humanClick(charSlot.x(), charSlot.y());
            QThread::msleep(3000);
        } else {
            loginLog("⚠ 未找到创建角色按钮, 尝试点击屏幕中央");
            humanClick(frame.cols / 2, frame.rows / 2);
            QThread::msleep(3000);
        }
        transitionTo(LoginPhase::VerifyInGame);
        emit statusMessage(QString("[窗口%1] 进入游戏...").arg(m_slot->index() + 1));
        break;
    }

    // ── 13. 创建新角色 ──
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
            // 模板匹配门派图标（需 images/roles/门派名.png）
            QPoint fc;
            if (matchTemplate(frame, faction, &fc, 0.80)) {
                loginLog(QString("选择门派: %1").arg(faction));
                humanClick(fc.x(), fc.y());
                m_charCreateStep = 2;
                m_phaseTicks = 0;
            } else {
                loginLog(QString("未找到门派模板: %1").arg(faction));
            }
            return;
        }

        // 阶段2: 输入角色名
        if (m_charCreateStep == 2) {
            if (m_phaseTicks == 0) {
                QThread::msleep(500);
                QString name = m_slot->charName();
                loginLog(QString("输入角色名: %1").arg(name));
                typeText(name);
                m_charCreateStep = 3;
                m_phaseTicks = 0;
            }
            return;
        }

        // 阶段3: 确认创建
        if (m_charCreateStep == 3) {
            if (m_phaseTicks > 8) {
                loginLog("⚠️ 确认创建超时");
                transitionTo(LoginPhase::Failed);
                return;
            }
            QPoint ok;
            if (matchTemplate(frame, "\u786e\u5b9a", &ok, 0.80) ||
                matchTemplate(frame, "\u521b\u5efa", &ok, 0.80)) {
                loginLog("点击确认创建");
                humanClick(ok.x(), ok.y());
                QThread::msleep(2000);
                // 创建完成，回到角色选择
                m_charCreateStep = 0;
                m_phaseTicks = 0;
                m_retryCount = 0;
                transitionTo(LoginPhase::WaitCharSelect);
            }
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
        QThread::msleep(3000);

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
        // 空帧不消耗重试（游戏中加载/黑屏时 D3D11 可能截不到）
        if (frame.empty()) {
            return;
        }

        auto &gu = GameUtils::Instance();

        auto locMatch = gu.detectLocation(frame);
        bool hasLocation = !locMatch.name.isEmpty() && locMatch.confidence > 0.6;

        if (hasLocation) {
            m_slot->setLoggedIn(true);
            loginLog(QString("✅ 登录成功！ location=%1")
                .arg(locMatch.name.toStdString()));
            transitionTo(LoginPhase::Done);
            emit statusMessage(QString("[窗口%1] 登录成功！")
                .arg(m_slot->index() + 1));
            emit finished(true);
            return;
        }

        break;
    }

    default:
        break;
    }
}

// ════════════════════════════════════════════════
// 视觉检测
// ════════════════════════════════════════════════
bool AutoLogin::matchTemplate(const cv::Mat &frame, const QString &tplName,
                              QPoint *outCenter, double minConf)
{
    if (frame.empty()) return false;
    auto &gu = GameUtils::Instance();
    auto result = gu.bestMatch(frame, gu.templateRoot() + "/login", tplName);
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
void AutoLogin::humanClick(int sx, int sy)
{
    auto &km = MouseKeyboardManager::Instance();
    km.mouseMoveDirect(sx, sy);
    QThread::msleep(80);
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
        loginLog(QString("[%1 → %2]").arg(phaseName(old)).arg(phaseName(next)));
        if (next == LoginPhase::Done || next == LoginPhase::Failed) {
            m_timer->stop();
        }
    }
}

QString AutoLogin::phaseName(LoginPhase p) const
{
    static const char *texts[] = {
        "Idle","LaunchGame","WaitLauncher","ClickStartGame","WaitGameWindow",
        "WaitLoading",
        "DetectLogin","TypeAccount","TypePassword","ClickLoginBtn",
        "WaitServerSelect","ConfirmServer","WaitCharSelect","CharSelect","CharCreate","EnterGame",
        "VerifyInGame","Done","Failed"
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


