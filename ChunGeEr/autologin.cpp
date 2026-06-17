#include "autologin.h"
#include "gameutils.h"
#include "LeoControl/mousekeyboardmanager.h"
#include <QThread>
#include <QDebug>
#include <QPixmap>
#include <QImage>
#include <QScreen>
#include <QGuiApplication>
#include <QWindow>
#include <Windows.h>

AutoLogin::AutoLogin(GameSlot *slot, QObject *parent)
    : QObject(parent), m_slot(slot)
{
    m_timer = new QTimer(this);
    m_timer->setInterval(2000);  // 每2秒检测一次
    connect(m_timer, &QTimer::timeout, this, &AutoLogin::onTick);
}

QString AutoLogin::phaseText() const
{
    static const char *texts[] = {
        "空闲","启动进程","等启动器","检测更新","等更新完成",
        "点进入游戏","等游戏窗口","检测登录页","输入账号","点登录",
        "选服","选角色","验证进入","完成","失败"
    };
    return texts[m_phase];
}

void AutoLogin::start(const QString &gamePath)
{
    m_gamePath = gamePath;
    m_tickCount = 0;
    m_phaseTicks = 0;
    setPhase(Launching);
    m_timer->start();
}

void AutoLogin::cancel()
{
    m_timer->stop();
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(2000);
    }
    setPhase(Failed);
    emit finished(false);
}

void AutoLogin::onTick()
{
    m_tickCount++;
    m_phaseTicks++;

    if (m_tickCount > m_maxWait) {
        emit statusMessage(QString("[窗口%1] 登录超时").arg(m_slot->index() + 1));
        cancel();
        return;
    }

    cv::Mat frame = captureScreen();
    if (frame.empty()) return;

    auto &gu = GameUtils::Instance();
    auto &km = MouseKeyboardManager::Instance();
    QString loginDir = gu.templateRoot() + "/login";

    switch (m_phase) {

    case Launching:
        if (launchGame(m_gamePath)) {
            setPhase(WaitLauncher);
            m_phaseTicks = 0;
            emit statusMessage(QString("[窗口%1] 进程已启动，等待启动器...").arg(m_slot->index() + 1));
        } else {
            emit statusMessage(QString("[窗口%1] 启动游戏进程失败").arg(m_slot->index() + 1));
            cancel();
        }
        break;

    case WaitLauncher:
    {
        HWND hwnd = FindWindowW(nullptr, L"大唐无双");
        if (!hwnd) hwnd = FindWindowW(L"Qt5152QWindowIcon", nullptr);
        if (hwnd) {
            ::SetForegroundWindow(hwnd);
            m_slot->setHwnd(hwnd);
            setPhase(CheckUpdate);
            m_phaseTicks = 0;
            emit statusMessage(QString("[窗口%1] 检测到启动器").arg(m_slot->index() + 1));
        } else if (m_phaseTicks > 10) {
            // 超30秒没启动器，假设是直接进游戏
            setPhase(WaitGameWindow);
            m_phaseTicks = 0;
        }
        break;
    }

    case CheckUpdate:
    {
        auto match = gu.bestMatch(frame, loginDir, "update_btn");
        if (match.confidence > 0.7) {
            km.humanMouseMove(match.centerX, match.centerY);
            QThread::msleep(150);
            km.mouseClick();
            emit statusMessage(QString("[窗口%1] 正在更新...").arg(m_slot->index() + 1));
            setPhase(WaitUpdateDone);
            m_phaseTicks = 0;
        } else {
            // 没更新按钮，直接进入下一阶段
            setPhase(WaitUpdateDone);
            m_phaseTicks = 0;
        }
        break;
    }

    case WaitUpdateDone:
    {
        // 每2秒检测一次，进入游戏按钮
        auto enterMatch = gu.bestMatch(frame, loginDir, "enter_game_btn");
        if (enterMatch.confidence > 0.7) {
            km.humanMouseMove(enterMatch.centerX, enterMatch.centerY);
            QThread::msleep(150);
            km.mouseClick();
            emit statusMessage(QString("[窗口%1] 点击进入游戏").arg(m_slot->index() + 1));
            setPhase(ClickEnterGame);
            m_phaseTicks = 0;
        } else if (m_phaseTicks > 15) {
            // 超30秒还没进入按钮，尝试跳过
            setPhase(WaitGameWindow);
            m_phaseTicks = 0;
        }
        break;
    }

    case ClickEnterGame:
    {
        // 点了进入游戏按钮后，等待游戏主窗口
        auto enterMatch = gu.bestMatch(frame, loginDir, "enter_game_btn");
        if (enterMatch.confidence > 0.7) {
            // 还在，重复点击
            km.humanMouseMove(enterMatch.centerX, enterMatch.centerY);
            QThread::msleep(150);
            km.mouseClick();
        }
        setPhase(WaitGameWindow);
        m_phaseTicks = 0;
        break;
    }

    case WaitGameWindow:
    {
        HWND hwnd = FindWindowW(nullptr, L"大唐无双");
        if (!hwnd) hwnd = FindWindowW(L"Qt5152QWindowIcon", nullptr);
        if (hwnd) {
            ::SetForegroundWindow(hwnd);
            m_slot->setHwnd(hwnd);
            emit statusMessage(QString("[窗口%1] 游戏窗口已找到，进入登录流程").arg(m_slot->index() + 1));
            setPhase(DetectLogin);
            m_phaseTicks = 0;
        }
        break;
    }

    case DetectLogin:
    {
        // 检测登录界面：账号输入框、密码输入框
        auto match = gu.bestMatch(frame, loginDir, "account");
        bool hasAccount = match.confidence > 0.65;
        match = gu.bestMatch(frame, loginDir, "password");
        bool hasPassword = match.confidence > 0.65;
        if (hasAccount || hasPassword) {
            emit statusMessage(QString("[窗口%1] 检测到登录界面，输入账号").arg(m_slot->index() + 1));
            setPhase(Typing);
            m_phaseTicks = 0;
        } else if (m_phaseTicks > 10) {
            // 没检测到登录界面，尝试直接输入
            setPhase(Typing);
            m_phaseTicks = 0;
        }
        break;
    }

    case Typing:
    {
        // 输入账号
        km.clickButton(KEY_TAB);
        QThread::msleep(100);
        km.clickButton(KEY_TAB);
        QThread::msleep(200);

        QString acc = m_slot->account();
        for (QChar ch : acc) {
            km.clickButton(ch.toUpper());
            QThread::msleep(30);
        }
        QThread::msleep(150);

        // Tab 到密码框
        km.clickButton(KEY_TAB);
        QThread::msleep(200);

        QString pwd = m_slot->password();
        for (QChar ch : pwd) {
            km.clickButton(ch.toUpper());
            QThread::msleep(30);
        }
        QThread::msleep(200);

        emit statusMessage(QString("[窗口%1] 账号密码已输入，点击登录").arg(m_slot->index() + 1));
        setPhase(ClickLogin);
        m_phaseTicks = 0;
        break;
    }

    case ClickLogin:
    {
        km.clickButton(KEY_RETURN);
        QThread::msleep(500);
        emit statusMessage(QString("[窗口%1] 等待服务器选择...").arg(m_slot->index() + 1));
        setPhase(SelectServer);
        m_phaseTicks = 0;
        break;
    }

    case SelectServer:
    {
        // 检测是否到选服界面（确定/进入按钮）
        auto match = gu.bestMatch(frame, loginDir, "confirm_btn");
        if (match.confidence > 0.7) {
            km.humanMouseMove(match.centerX, match.centerY);
            QThread::msleep(150);
            km.mouseClick();
            emit statusMessage(QString("[窗口%1] 确认服务器").arg(m_slot->index() + 1));
        }
        setPhase(SelectCharacter);
        m_phaseTicks = 0;
        break;
    }

    case SelectCharacter:
    {
        // 检测是否到角色选择界面（进入游戏按钮）
        auto match = gu.bestMatch(frame, loginDir, "enter_game_btn");
        if (match.confidence > 0.7) {
            km.humanMouseMove(match.centerX, match.centerY);
            QThread::msleep(150);
            km.mouseClick();
            emit statusMessage(QString("[窗口%1] 选择角色进入游戏").arg(m_slot->index() + 1));
        }
        setPhase(Verifying);
        m_phaseTicks = 0;
        break;
    }

    case Verifying:
    {
        // 检测是否进入游戏：地图名匹配 或 ingame 模板
        auto match = gu.bestMatch(frame, loginDir, "ingame");
        if (match.confidence > 0.7) {
            m_slot->setLoggedIn(true);
            setPhase(Done);
            emit statusMessage(QString("[窗口%1] 登录成功！").arg(m_slot->index() + 1));
            emit finished(true);
            break;
        }
        auto locMatch = gu.detectLocation(frame);
        if (!locMatch.name.isEmpty() && locMatch.confidence > 0.6) {
            m_slot->setLoggedIn(true);
            setPhase(Done);
            emit statusMessage(QString("[窗口%1] 登录成功！").arg(m_slot->index() + 1));
            emit finished(true);
            break;
        }
        if (m_phaseTicks > 15) {
            // 超30秒还没进入，标记失败
            emit statusMessage(QString("[窗口%1] 登录验证超时").arg(m_slot->index() + 1));
            cancel();
        }
        break;
    }

    default:
        break;
    }
}

bool AutoLogin::launchGame(const QString &gamePath)
{
    if (m_process) {
        m_process->deleteLater();
        m_process = nullptr;
    }
    m_process = new QProcess;

    if (gamePath.endsWith(".lnk", Qt::CaseInsensitive)) {
        m_process->start("cmd", {"/c", "start", "", gamePath});
    } else {
        // runas 提权启动
        SHELLEXECUTEINFOW sei = {sizeof(sei)};
        sei.lpVerb = L"runas";
        sei.lpFile = reinterpret_cast<LPCWSTR>(gamePath.utf16());
        sei.lpDirectory = reinterpret_cast<LPCWSTR>(
            QFileInfo(gamePath).absolutePath().utf16());
        sei.nShow = SW_SHOWNORMAL;
        bool ok = ::ShellExecuteExW(&sei) != 0;
        if (!ok) {
            emit statusMessage(QString("启动失败，请检查路径"));
            return false;
        }
    }
    return true;
}

cv::Mat AutoLogin::captureScreen()
{
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) return {};

    // 如果已有 HWND，优先截取游戏窗口
    if (m_slot->hwnd()) {
        HDC hwndDC = ::GetDC(m_slot->hwnd());
        if (hwndDC) {
            RECT rect;
            ::GetClientRect(m_slot->hwnd(), &rect);
            int w = rect.right - rect.left;
            int h = rect.bottom - rect.top;
            if (w > 0 && h > 0) {
                HDC memDC = ::CreateCompatibleDC(hwndDC);
                HBITMAP hbm = ::CreateCompatibleBitmap(hwndDC, w, h);
                HBITMAP old = (HBITMAP)::SelectObject(memDC, hbm);
                ::BitBlt(memDC, 0, 0, w, h, hwndDC, 0, 0, SRCCOPY);

                BITMAPINFOHEADER bi = {sizeof(BITMAPINFOHEADER), w, -h, 1, 32, BI_RGB};
                cv::Mat mat(h, w, CV_8UC4);
                ::GetDIBits(memDC, hbm, 0, h, mat.data, (BITMAPINFO *)&bi, DIB_RGB_COLORS);

                ::SelectObject(memDC, old);
                ::DeleteObject(hbm);
                ::DeleteDC(memDC);
                ::ReleaseDC(m_slot->hwnd(), hwndDC);

                cv::cvtColor(mat, mat, cv::COLOR_BGRA2BGR);
                return mat;
            }
            ::ReleaseDC(m_slot->hwnd(), hwndDC);
        }
    }

    // Fallback: 全屏截图
    QPixmap pix = screen->grabWindow(0);
    QImage img = pix.toImage().convertToFormat(QImage::Format_BGR888);
    return cv::Mat(img.height(), img.width(), CV_8UC3,
                   const_cast<uchar *>(img.bits()), img.bytesPerLine()).clone();
}

void AutoLogin::setPhase(Phase p)
{
    if (m_phase != p) {
        m_phase = p;
        m_phaseTicks = 0;
        emit phaseChanged(p);
        if (p == Done || p == Failed)
            m_timer->stop();
    }
}
