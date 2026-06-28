#include "baseservice.h"
#include "../KeyboardListener/keyboardlistener.h"
#include "../LeoControl/mousekeyboardmanager.h"
#include <windows.h>
#include <QSettings>
#include <QCoreApplication>
#include <QThread>
#include <QRandomGenerator>
#include <algorithm>
#include <fstream>
#include <iterator>
#include "XuLog.h"
BaseService::BaseService(QObject *parent)
    : QThread{parent}, toRun(false)
{
    m_templateRoot = QCoreApplication::applicationDirPath() + "/images/";

    // DirectConnection: 截图回调在 ScreenCaptureManager 线程直接执行
    // receiveCaptureScreen 只做加锁拷贝，线程安全由 picMutex 保证
    // 不用 QueuedConnection 因为 QThread::run() 没有 exec() 事件循环
    connect(&ScreenCaptureManager::Instance(), &ScreenCaptureManager::capturedScreen,
            this, &BaseService::receiveCaptureScreen, Qt::DirectConnection);
    connect(&WsManager::Instance(), &WsManager::clientRecMeg,
            this, &BaseService::clientRecMegSlot, Qt::QueuedConnection);
    connect(&Keyboardlistener::Instance(), &Keyboardlistener::keyPressEvent,
            this, &BaseService::keyPressEventSlot, Qt::QueuedConnection);
}

void BaseService::run()
{
}

void BaseService::clientHandleRecMsg(const json &data)
{
}

void BaseService::handlePressEvent(int vkCode)
{
}

void BaseService::chooseLeftGame()
{
    MouseKeyboardManager::Instance().mouseMoveDirect(200, 10);
    QThread::msleep(500);
    MouseKeyboardManager::Instance().mousePress(MOUSE_LEFT);
    QThread::msleep(100);
    MouseKeyboardManager::Instance().mouseRelease(MOUSE_LEFT);
}

void BaseService::chooseRightGame()
{
    MouseKeyboardManager::Instance().mouseMoveDirect(1500, 10);
    QThread::msleep(500);
    MouseKeyboardManager::Instance().mousePress(MOUSE_LEFT);
    QThread::msleep(100);
    MouseKeyboardManager::Instance().mouseRelease(MOUSE_LEFT);
}

NameColor BaseService::detectNameColor(const cv::Mat &image)
{
    if (image.empty()) return NAME_UNKNOWN;

    cv::Mat hsv;
    cv::cvtColor(image, hsv, cv::COLOR_BGR2HSV);
    // 红色的两个HSV区间（红色在H=0附近会跨两个区间）
    cv::Mat mask1, mask2, redMask;
    cv::inRange(hsv, cv::Scalar(0, 100, 100), cv::Scalar(10, 255, 255), mask1);
    cv::inRange(hsv, cv::Scalar(160, 100, 100), cv::Scalar(179, 255, 255), mask2);
    redMask = mask1 | mask2;

    // 白色检测（亮度高，饱和度低）
    cv::Mat whiteMask;
    cv::inRange(hsv, cv::Scalar(0, 0, 200), cv::Scalar(180, 60, 255), whiteMask);

    int redCount   = cv::countNonZero(redMask);
    int whiteCount = cv::countNonZero(whiteMask);
    int total      = image.rows * image.cols;

    double redRatio   = (double)redCount / total;
    double whiteRatio = (double)whiteCount / total;
    infof("redRatio:{},whiteRatio:{}", redRatio, whiteRatio);
    if (redRatio > 0.10 && redRatio > whiteRatio * 1.2)
        return NAME_RED;
    if (whiteRatio > 0.05 && whiteRatio > redRatio * 1.2)
        return NAME_WHITE;

    return NAME_UNKNOWN;
}

void BaseService::setDatangWindowPos()
{
    QList<HWND> hwndlist;
    HWND hWnd;
    hWnd = GetDesktopWindow();

    HWND hCurrentWindow = GetWindow(hWnd, GW_CHILD);
    TCHAR buff[255];

    while (hCurrentWindow != 0)
    {
        memset(buff, 0, 255);
        if ((GetWindowText(hCurrentWindow, buff, 255) > 0) &&
            IsWindowVisible(hCurrentWindow))
        {
            if (QString::fromWCharArray(buff).contains("大唐无双"))
            {
                hwndlist.push_back(hCurrentWindow);
                memset(buff, 0, 255);
            }
        }
        hCurrentWindow = GetNextWindow(hCurrentWindow, GW_HWNDNEXT);
    }
    infof("hwndlist size :{}", hwndlist.size());
    for (int i = 0; i < hwndlist.size(); ++i)
    {
        RECT rect;
        if (GetClientRect(hwndlist[i], &rect))
        {
            if (i == 0)
            {
                infof("hwndlist size :{},{},{},{}", rect.right, rect.left, rect.bottom, rect.top);
                ::SetWindowPos(hwndlist[i], HWND_TOP, 0, 0,
                               rect.right - rect.left, rect.bottom - rect.top,
                               SWP_SHOWWINDOW);
            }
            else
            {
                ::SetWindowPos(hwndlist[i], HWND_TOP, 1920 - 1400, 0,
                               rect.right - rect.left, rect.bottom - rect.top,
                               SWP_SHOWWINDOW);
            }
        }
    }
}

void BaseService::receiveCaptureScreen(ScreenCaptureManager::ScreenData data)
{
    picMutex.lock();
    curPic = data;
    picMutex.unlock();
}

void BaseService::clientRecMegSlot(const json &msg)
{
    clientHandleRecMsg(msg);
}

void BaseService::keyPressEventSlot(int vkCode)
{
    handlePressEvent(vkCode);
}

// ══════════════════════════════════════════════════
// 共用工具函数
// ══════════════════════════════════════════════════

cv::Mat BaseService::screenToMat()
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

void BaseService::clickAt(int sx, int sy)
{
    MouseKeyboardManager::Instance().mouseMoveDirect(sx, sy);
    QThread::msleep(50);
    MouseKeyboardManager::Instance().mouseClick();
    infof("点击(%1,%2)", sx, sy);

    // 点击后随机延时1~2秒，再随机移开100~200px
    randSleep(1000, 2000);
    int dx = 100 + (rand() % 101);
    int dy = 100 + (rand() % 101);
    if (rand() & 1) dx = -dx;
    if (rand() & 1) dy = -dy;
    MouseKeyboardManager::Instance().mouseMoveDirect(sx + dx, sy + dy);
}

void BaseService::clickCenter(const QRect &r)
{
    clickAt(r.center().x(), r.center().y());
}

void BaseService::randSleep(int minMs, int maxMs)
{
    int ms = minMs + QRandomGenerator::global()->bounded(maxMs - minMs + 1);
    QThread::msleep(ms);
}

QRect BaseService::findTemplate(const QString &name, double threshold)
{
    cv::Mat screen = screenToMat();
    if (screen.empty()) return {};

    cv::Mat tmpl = imreadUnicode(m_templateRoot + name + ".png");
    if (tmpl.empty()) return {};

    cv::Mat result;
    cv::matchTemplate(screen, tmpl, result, cv::TM_CCOEFF_NORMED);
    double maxVal;
    cv::Point maxLoc;
    cv::minMaxLoc(result, nullptr, &maxVal, nullptr, &maxLoc);

    if (maxVal >= threshold) {
        return QRect(maxLoc.x, maxLoc.y, tmpl.cols, tmpl.rows);
    }
    return {};
}

QRect BaseService::findTemplateInROI(const QString &name, double threshold,
                                      const QRect &roi)
{
    cv::Mat screen = screenToMat();
    if (screen.empty()) return {};

    cv::Mat tmpl = imreadUnicode(m_templateRoot + name+".png");
    if (tmpl.empty()) return {};

    // 在指定 ROI 区域内搜索
    QRect srch = roi;
    if (!roi.isNull() && !roi.isEmpty()) {
        srch = QRect(
            qBound(0, roi.x(), screen.cols - 1),
            qBound(0, roi.y(), screen.rows - 1),
            qMin(roi.width(), screen.cols - qMax(0, roi.x())),
            qMin(roi.height(), screen.rows - qMax(0, roi.y()))
        );
    } else {
        srch = QRect(0, 0, screen.cols, screen.rows);
    }
    if (srch.width() < tmpl.cols || srch.height() < tmpl.rows) return {};

    cv::Mat crop = screen(cv::Rect(srch.x(), srch.y(), srch.width(), srch.height()));
    cv::Mat result;
    cv::matchTemplate(crop, tmpl, result, cv::TM_CCOEFF_NORMED);
    double maxVal;
    cv::Point maxLoc;
    cv::minMaxLoc(result, nullptr, &maxVal, nullptr, &maxLoc);

    if (maxVal >= threshold) {
        return QRect(srch.x() + maxLoc.x, srch.y() + maxLoc.y,
                     tmpl.cols, tmpl.rows);
    }
    return {};
}

void BaseService::detectGameWindow()
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
            infof("游戏窗口: offset(%1,%2) 客户区:%3x%4",
                  gameOffsetX, gameOffsetY,
                  cr.right - cr.left, cr.bottom - cr.top);
        }
    } else {
        infof("⚠ 未找到游戏窗口");
    }
}

QRect BaseService::offsetROI(const QRect &r) const
{
    return QRect(r.x() + gameOffsetX, r.y() + gameOffsetY,
                 r.width(), r.height());
}

cv::Mat BaseService::imreadUnicode(const QString &path)
{
    std::ifstream f(path.toStdWString(), std::ios::binary);
    if (!f.is_open()) return {};
    std::vector<char> buf((std::istreambuf_iterator<char>(f)),
                           std::istreambuf_iterator<char>());
    return cv::imdecode(buf, cv::IMREAD_COLOR);
}
