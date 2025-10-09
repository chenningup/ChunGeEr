#include "keyboardlistener.h"
#include <Windows.h>
#include <QDebug>
// 声明钩子句柄
HHOOK g_hKeyboardHook = NULL;

 //钩子过程函数
LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION)
    {
        KBDLLHOOKSTRUCT *pKeyStruct = (KBDLLHOOKSTRUCT*)lParam;
        // 判断是按键按下(WM_KEYDOWN)还是弹起(WM_KEYUP)
        if (wParam == WM_KEYDOWN)
        {
            // 输出按下的虚拟键码
            qDebug()<<"Key Pressed: VK_Code = " <<pKeyStruct->vkCode;
            // 示例：拦截A键
            emit Keyboardlistener::Instance().keyPressEvent(pKeyStruct->vkCode);
        }
    }

    // 将消息传递给下一个钩子或目标窗口
    return CallNextHookEx(g_hKeyboardHook, nCode, wParam, lParam);
}

Keyboardlistener::Keyboardlistener(QObject *parent)
    : QThread{parent},isListen(false)
{

}

Keyboardlistener &Keyboardlistener::Instance()
{
    static Keyboardlistener mKeyboardlistener;
    return mKeyboardlistener;
}

void Keyboardlistener::run()
{
    g_hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandle(NULL), 0);

    if (g_hKeyboardHook == NULL)
    {
        qDebug()<<"Failed to install hook!";
        return;
    }

    qDebug()<<"Global keyboard hook is running. Press keys to test, press ESC to quit.";
    // 消息循环，保持程序运行
    MSG msg;
    while (isListen && GetMessage(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // 卸载钩子
    UnhookWindowsHookEx(g_hKeyboardHook);
}

void Keyboardlistener::startListen()
{
    if(isListen)
    {
        return;
    }
    isListen = true;
    start();
}

void Keyboardlistener::stopListen()
{
    isListen = false;
}
