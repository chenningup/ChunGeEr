#include "autologin.h"
#include <QCoreApplication>
#include "gameutils.h"
#include "gameslot.h"
#include "LeoControl/mousekeyboardmanager.h"
#include "XuLog.h"
#include <QThread>
#include <QFileInfo>
#include <QSettings>
#include <QProcess>
#include <QRandomGenerator>
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <opencv2/imgproc.hpp>

// ── 超时常量 ──
static const int MAX_RETRIES = 8;
static const int LAUNCHER_TIMEOUT = 30;

// ════════════════════════════════════════════════
// 构造
// ════════════════════════════════════════════════
AutoLogin::AutoLogin(GameSlot *slot, QObject *parent)
    : QObject(parent), m_slot(slot)
{
    m_timer = new QTimer(this);
    m_timer->setInterval(m_loopIntervalMs);
    connect(m_timer, &QTimer::timeout, this, &AutoLogin::processState);

    connect(&ScreenCaptureManager::Instance(), &ScreenCaptureManager::capturedScreen,
            this, &AutoLogin::onCaptureScreen);
}

QString AutoLogin::phaseText() const
{
    static const char *texts[] = {
        "Idle","LaunchGame","WaitLauncher","ClickStartGame","WaitGameWindow",
        "WaitLoading",
        "DetectLogin","TypeAccount","TypePassword","ClickLoginBtn",
        "WaitServerSelect","ConfirmServer","WaitCharSelect","EnterGame",
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

cv::Mat AutoLogin::screenToMat()
{
    m_picMutex.lock();
    auto pic = m_curPic;
    m_picMutex.unlock();

    if (pic.des.Width <= 0 || pic.des.Height <= 0 || !pic.data || pic.data->empty())
        return cv::Mat();

    int w = (int)pic.des.Width, h = (int)pic.des.Height;
    int step = (pic.RowPitch > 0) ? pic.RowPitch : (int)(pic.des.Width * 4);
    cv::Mat bgra = cv::Mat(h, w, CV_8UC4, (void*)pic.data->data(), (size_t)step).clone();
    if (bgra.empty()) return cv::Mat();

    cv::Mat bgr;
    cv::cvtColor(bgra, bgr, cv::COLOR_BGRA2BGR);
    return bgr;
}

// ════════════════════════════════════════════════
// 状态机主循环
// ════════════════════════════════════════════════
void AutoLogin::processState()
{
    m_phaseTicks++;

    switch (m_phase) {

    case LoginPhase::LaunchGame:
    {
        loginLog("启动游戏进程...");
        if (m_process) {
            m_process->deleteLater();
            m_process = nullptr;
        }

        QString exePath = m_gamePath;
        if (m_gamePath.endsWith(".lnk", Qt::CaseInsensitive)) {
            wchar_t target[MAX_PATH] = {0};
            IShellLinkW *psl;
            IPersistFile *ppf;
            HRESULT hr = CoInitialize(nullptr);
            if (SUCCEEDED(hr) &&
                SUCCEEDED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                    IID_IShellLinkW, (void**)&psl))) {
                if (SUCCEEDED(psl->QueryInterface(IID_IPersistFile, (void**)&ppf))) {
                    if (SUCCEEDED(ppf->Load(reinterpret_cast<LPCWSTR>(m_gamePath.utf16()), 0)) &&
                        SUCCEEDED(psl->GetPath(target, MAX_PATH, nullptr, 0))) {
                        exePath = QString::fromWCharArray(target);
                    }
                    ppf->Release();
                }
                psl->Release();
            }
            CoUninitialize();
        }

        loginLog(QString("启动: %1").arg(exePath));
        bool ok = QProcess::startDetached(exePath, {}, QFileInfo(exePath).absolutePath());
        if (!ok) {
            loginLog("❌ QProcess::startDetached 启动失败");
            emit statusMessage(QString("[窗口%1] 启动失败，请检查路径")
                .arg(m_slot->index() + 1));
            cancel();
            return;
        }
        transitionTo(LoginPhase::WaitLauncher);
        emit statusMessage(QString("[窗口%1] 进程已启动，等待启动器...")
            .arg(m_slot->index() + 1));
        break;
    }

    // ── 1. 等待启动器窗口 ──
    case LoginPhase::WaitLauncher:
    {
        HWND hwnd = FindWindowW(nullptr, L"无双启动器");
        if (!hwnd) hwnd = FindWindowW(L"Qt5152QWindowIcon", nullptr);
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

        auto result = GameUtils::Instance().bestMatch(frame, GameUtils::Instance().templateRoot() + "/login", "开始游戏");
        if (result.confidence >= 0.6) {
            loginLog(QString("✅ 检测到开始游戏按钮 (%1,%2) conf=%3")
                .arg(result.centerX).arg(result.centerY).arg(result.confidence));
            QPoint screenPos = frameToScreen(
                QPoint(result.centerX, result.centerY), m_launcherHwnd);
            humanClick(screenPos.x(), screenPos.y());
            QThread::msleep(1000);
            transitionTo(LoginPhase::WaitGameWindow);
            emit statusMessage(QString("[窗口%1] 已点击开始游戏")
                .arg(m_slot->index() + 1));
            return;
        }

        if (m_phaseTicks > 40) {
            loginLog("❌ 找不到开始游戏按钮");
            cancel();
        }
        break;
    }

    // ── 3. 等待游戏客户端窗口 ──
    case LoginPhase::WaitGameWindow:
    {
        HWND hwnd = FindWindowW(nullptr, L"大唐无双");
        if (hwnd && hwnd != m_launcherHwnd) {
            m_gameHwnd = hwnd;
            m_slot->setHwnd(hwnd);
            QThread::msleep(500);
            loginLog(QString("✅ 检测到游戏窗口 hwnd=%1").arg((uintptr_t)hwnd, 0, 16));
            transitionTo(LoginPhase::WaitLoading);
            emit statusMessage(QString("[窗口%1] 检测到游戏窗口")
                .arg(m_slot->index() + 1));
            return;
        }

        if (m_phaseTicks > 60) {
            loginLog("❌ 等待游戏窗口超时");
            cancel();
        }
        break;
    }

    // ── 4. 等待游戏加载完成 ──
    case LoginPhase::WaitLoading:
    {
        cv::Mat frame = screenToMat();
        if (frame.empty()) {
            if (++m_retryCount > MAX_RETRIES + 5) { cancel(); }
            return;
        }

        auto result = GameUtils::Instance().bestMatch(frame, GameUtils::Instance().templateRoot() + "/login", "登录界面");
        if (result.confidence >= 0.5) {
            loginLog(QString("✅ 检测到登录界面 (%1,%2) conf=%3")
                .arg(result.centerX).arg(result.centerY).arg(result.confidence));
            transitionTo(LoginPhase::DetectLogin);
            emit statusMessage(QString("[窗口%1] 进入登录界面")
                .arg(m_slot->index() + 1));
            return;
        }

        if (m_phaseTicks > 60) {
            loginLog("❌ 等待登录界面超时");
            cancel();
        }
        break;
    }

    // ── 5. 检测登录界面 → 账号/密码框 ──
    case LoginPhase::DetectLogin:
    {
        cv::Mat frame = screenToMat();
        if (frame.empty()) {
            if (++m_retryCount > MAX_RETRIES + 5) { cancel(); }
            return;
        }

        auto accResult = GameUtils::Instance().bestMatch(frame, GameUtils::Instance().templateRoot() + "/login", "账号");
        auto pwdResult = GameUtils::Instance().bestMatch(frame, GameUtils::Instance().templateRoot() + "/login", "密码");

        if (accResult.confidence >= 0.5 && pwdResult.confidence >= 0.5) {
            loginLog(QString("✅ 检测到账号框(%1,%2) 密码框(%3,%4)")
                .arg(accResult.centerX).arg(accResult.centerY)
                .arg(pwdResult.centerX).arg(pwdResult.centerY));

            // 点账号框右侧区域聚焦
            QPoint screenPos = frameToScreen(
                QPoint(accResult.centerX + 120, accResult.centerY), m_gameHwnd);
            humanClick(screenPos.x(), screenPos.y());
            QThread::msleep(300);
            transitionTo(LoginPhase::TypeAccount);
            emit statusMessage(QString("[窗口%1] 输入账号...")
                .arg(m_slot->index() + 1));
            return;
        }

        if (m_phaseTicks > 10) {
            loginLog("账号/密码框未检测到，尝试盲输");
            transitionTo(LoginPhase::TypeAccount);
        }
        break;
    }

    // ── 6. 输入账号 ──
    case LoginPhase::TypeAccount:
    {
        QSettings settings(QCoreApplication::applicationDirPath() + "/config.ini", QSettings::IniFormat);
        QString sk = QString("Slot%1").arg(m_slot->index());
        QString account = settings.value("Accounts/" + sk + "/Account").toString();

        loginLog(QString("输入账号: %1").arg(account));
        typeText(account);

        QThread::msleep(300);
        pressKey(VK_RETURN);
        QThread::msleep(200);
        pressKey(VK_RETURN);
        QThread::msleep(500);

        transitionTo(LoginPhase::TypePassword);
        emit statusMessage(QString("[窗口%1] 输入密码...")
            .arg(m_slot->index() + 1));
        break;
    }

    // ── 7. 输入密码 ──
    case LoginPhase::TypePassword:
    {
        QSettings settings(QCoreApplication::applicationDirPath() + "/config.ini", QSettings::IniFormat);
        QString sk = QString("Slot%1").arg(m_slot->index());
        QString password = settings.value("Accounts/" + sk + "/Password").toString();

        loginLog("输入密码");
        typeText(password);
        QThread::msleep(300);

        transitionTo(LoginPhase::ClickLoginBtn);
        break;
    }

    // ── 8. 点击登录按钮 ──
    case LoginPhase::ClickLoginBtn:
    {
        cv::Mat frame = screenToMat();
        if (frame.empty()) {
            if (++m_retryCount > MAX_RETRIES + 5) { cancel(); }
            return;
        }

        auto result = GameUtils::Instance().bestMatch(frame, GameUtils::Instance().templateRoot() + "/login", "登录");
        if (result.confidence >= 0.5) {
            loginLog(QString("✅ 点击登录按钮 (%1,%2) conf=%3")
                .arg(result.centerX).arg(result.centerY).arg(result.confidence));
            QPoint screenPos = frameToScreen(
                QPoint(result.centerX, result.centerY), m_gameHwnd);
            humanClick(screenPos.x(), screenPos.y());
            QThread::msleep(2000);
            transitionTo(LoginPhase::WaitServerSelect);
            return;
        }

        if (m_phaseTicks > 20) {
            loginLog("❌ 找不到登录按钮");
            cancel();
        }
        break;
    }

    // ── 9. 等待服务器选择 ──
    case LoginPhase::WaitServerSelect:
    {
        cv::Mat frame = screenToMat();
        if (frame.empty()) {
            if (++m_retryCount > MAX_RETRIES + 10) { cancel(); }
            return;
        }

        auto result = GameUtils::Instance().bestMatch(frame, GameUtils::Instance().templateRoot() + "/login", "服务器确定");
        if (result.confidence >= 0.5) {
            QPoint screenPos = frameToScreen(
                QPoint(result.centerX, result.centerY), m_gameHwnd);
            humanClick(screenPos.x(), screenPos.y());
            QThread::msleep(1500);
            transitionTo(LoginPhase::WaitCharSelect);
            emit statusMessage(QString("[窗口%1] 确认服务器")
                .arg(m_slot->index() + 1));
            return;
        }

        if (m_phaseTicks > 20) {
            loginLog("服务器选择超时，跳过");
            transitionTo(LoginPhase::WaitCharSelect);
        }
        break;
    }

    // ── 10. 等待角色选择 → 进入游戏 ──
    case LoginPhase::WaitCharSelect:
    {
        if (m_phaseTicks < 3) { break; }

        cv::Mat frame = screenToMat();
        if (frame.empty()) {
            if (++m_retryCount > MAX_RETRIES + 10) { cancel(); }
            return;
        }

        auto result = GameUtils::Instance().bestMatch(frame, GameUtils::Instance().templateRoot() + "/login", "进入游戏");
        if (result.confidence >= 0.5) {
            QPoint screenPos = frameToScreen(
                QPoint(result.centerX, result.centerY), m_gameHwnd);
            humanClick(screenPos.x(), screenPos.y());
            QThread::msleep(2000);
        }

        transitionTo(LoginPhase::VerifyInGame);
        break;
    }

    // ── 11. 验证进入游戏 ──
    case LoginPhase::VerifyInGame:
    {
        if (m_phaseTicks > 15) {
            loginLog("✅ 进入游戏完成");
            transitionTo(LoginPhase::Done);
            emit statusMessage(QString("[窗口%1] 登录成功")
                .arg(m_slot->index() + 1));
            emit finished(true);
        }
        break;
    }

    case LoginPhase::Done:
    case LoginPhase::Failed:
        break;

    default:
        loginLog(QString("未知阶段: %1").arg((int)m_phase));
        break;
    }
}

// ════════════════════════════════════════════════
// 截图坐标 → 屏幕绝对坐标
// ════════════════════════════════════════════════
QPoint AutoLogin::frameToScreen(const QPoint &framePt, HWND hwnd)
{
    if (!hwnd) return framePt;
    RECT wndRect;
    if (!GetWindowRect(hwnd, &wndRect)) return framePt;
    return QPoint(wndRect.left + framePt.x(), wndRect.top + framePt.y());
}

// ════════════════════════════════════════════════
// 硬件鼠标点击
// ════════════════════════════════════════════════
void AutoLogin::humanClick(int sx, int sy)
{
    MouseKeyboardManager::Instance().moveMouse(sx, sy);
    QThread::msleep(50);
    MouseKeyboardManager::Instance().mouseClick();
}

// ════════════════════════════════════════════════
// 键盘输入（通过 Arduino）
// ════════════════════════════════════════════════
void AutoLogin::typeText(const QString &text)
{
    for (const QChar &ch : text) {
        MouseKeyboardManager::Instance().clickButton(QString(ch));
        QThread::msleep(50 + (QRandomGenerator::global()->bounded(70)));
    }
}

void AutoLogin::pressKey(int vkCode)
{
    MouseKeyboardManager::Instance().clickButton(vkCode);
}

// ════════════════════════════════════════════════
// 状态转换
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
        "WaitServerSelect","ConfirmServer","WaitCharSelect","EnterGame",
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
