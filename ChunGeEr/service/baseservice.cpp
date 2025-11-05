#include "baseservice.h"
#include "../KeyboardListener/keyboardlistener.h"
#include "../LeoControl/mousekeyboardmanager.h"
#include <windows.h>
BaseService::BaseService(QObject *parent)
    : QThread{parent},toRun(false)
{
    connect(&ScreenCaptureManager::Instance(),&ScreenCaptureManager::capturedScreen,this,&BaseService::receiveCaptureScreen,Qt::QueuedConnection);
    connect(&WsManager::Instance(),&WsManager::clientRecMeg,this,&BaseService::clientRecMegSlot,Qt::QueuedConnection);
    connect(&Keyboardlistener::Instance(),&Keyboardlistener::keyPressEvent,this,&BaseService::keyPressEventSlot,Qt::QueuedConnection);
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
    MouseKeyboardManager::Instance().mouseMoveDirect(200,10);
    QThread::msleep(500);
    MouseKeyboardManager::Instance().mousePress(MOUSE_LEFT);
    QThread::msleep(100);
    MouseKeyboardManager::Instance().mouseRelease(MOUSE_LEFT);
}

void BaseService::chooseRightGame()
{
    MouseKeyboardManager::Instance().mouseMoveDirect(1500,10);
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
    cv::imshow("hsv", hsv);
    cv::waitKey();
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

    // 阈值可根据实际情况调整
    if (redRatio > 0.10 && redRatio > whiteRatio * 1.2)
        return NAME_RED;
    if (whiteRatio > 0.10 && whiteRatio > redRatio * 1.2)
        return NAME_WHITE;

    return NAME_UNKNOWN;
}

void BaseService::setDatangWindowPos()
{
    QList<HWND>hwndlist;
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
    for (int i = 0; i < hwndlist.size(); ++i)
    {
        RECT rect;
        if (GetClientRect(hwndlist[i], &rect))
        {
            if(i == 0 )
            {
                ::SetWindowPos(hwndlist[i], HWND_TOP, 0, 0, rect.right - rect.left, rect.bottom - rect.top, SWP_SHOWWINDOW);
            }
            else
            {
                ::SetWindowPos(hwndlist[i], HWND_TOP, 1920 - 1400, 0, rect.right - rect.left, rect.bottom - rect.top, SWP_SHOWWINDOW);
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
