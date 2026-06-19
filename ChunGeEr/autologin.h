#ifndef AUTOLOGIN_H
#define AUTOLOGIN_H

#include <QObject>
#include <QString>
#include <QTimer>
#include <QProcess>
#include <QMutex>
#include <opencv2/opencv.hpp>
#include <Windows.h>
#include "screencapturemanager.h"

class GameSlot;

// ── 登录阶段 ──
enum class LoginPhase {
    Idle,
    LaunchGame,         // 启动游戏EXE
    WaitLauncher,       // 等启动器窗口 (无双启动器)
    ClickStartGame,     // 检测并点击启动器"开始游戏"按钮
    WaitGameWindow,     // 等游戏客户端窗口出现
    WaitLoading,        // 等游戏加载完成（画面变化）
    DetectLogin,        // 检测登录界面 (account/password)
    TypeAccount,        // 输入账号
    TypePassword,       // 输入密码
    ClickLoginBtn,      // 检测并点击登录按钮
    WaitServerSelect,   // 等服务器选择界面
    ConfirmServer,      // 确认服务器
    WaitCharSelect,     // 等角色选择界面
    CharSelect,         // 选择已有角色 → 进入游戏
    CharCreate,         // 创建新角色 → 选门派/输名字/确认
    EnterGame,          // 点击进入游戏
    VerifyInGame,       // 验证已进入游戏
    Done,
    Failed
};

class AutoLogin : public QObject
{
    Q_OBJECT
public:
    explicit AutoLogin(GameSlot *slot, QObject *parent = nullptr);

    QString phaseText() const;
    QString phaseName(LoginPhase p) const;

    void start(const QString &gamePath);
    void cancel();

signals:
    void statusMessage(const QString &msg);
    void finished(bool success);

private slots:
    void processState();
    void onCaptureScreen(ScreenCaptureManager::ScreenData data);

private:
    // ── 视觉检测 ──
    cv::Mat screenToMat();
    bool    matchTemplate(const cv::Mat &frame, const QString &tplName, QPoint *outCenter = nullptr, double minConf = 0.65);

    // ── 动作 ──
    void    humanClick(int sx, int sy);
    void    typeText(const QString &text);
    void    pressKey(int vkCode);

    // ── 工具 ──
    void    transitionTo(LoginPhase next);
    void    loginLog(const QString &msg);

    GameSlot *m_slot = nullptr;
    QTimer   *m_timer = nullptr;
    QProcess *m_process = nullptr;
    QString   m_gamePath;
    int       m_loopIntervalMs = 800;

    LoginPhase m_phase = LoginPhase::Idle;
    int        m_retryCount = 0;
    int        m_phaseTicks = 0;
    int        m_charCreateStep = 0; // CharCreate 子阶段
    HWND       m_launcherHwnd = nullptr;
    HWND       m_gameHwnd = nullptr;

    // 截图
    ScreenCaptureManager::ScreenData m_curPic;
    QMutex m_picMutex;

    static const int MAX_RETRIES = 8;
    static const int PHASE_TIMEOUT = 20;
    static const int LAUNCHER_TIMEOUT = 30;
};

#endif // AUTOLOGIN_H
