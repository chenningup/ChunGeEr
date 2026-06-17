#ifndef AUTOLOGIN_H
#define AUTOLOGIN_H

#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QFileInfo>
#include <opencv2/opencv.hpp>
#include "gameslot.h"

// ════════════════════════════════════════════
// 自动登录：启动游戏 → 启动器流程 → 登录界面 → 输入账号密码 → 进入游戏
// 每个 GameSlot 一个 AutoLogin 实例，定时器驱动状态机
// ════════════════════════════════════════════
class AutoLogin : public QObject
{
    Q_OBJECT
public:
    enum Phase {
        Idle,
        Launching,       // 启动游戏进程
        WaitLauncher,    // 等启动器窗口出现
        CheckUpdate,     // 检测并点击更新按钮
        WaitUpdateDone,  // 等更新完成/检测进入游戏按钮
        ClickEnterGame,  // 点进入游戏
        WaitGameWindow,  // 等游戏主窗口出现
        DetectLogin,     // 检测登录界面（账号密码输入框）
        Typing,          // 输入账号密码
        ClickLogin,      // 点登录/回车
        SelectServer,    // 选服确认
        SelectCharacter, // 选角色进入
        Verifying,       // 验证是否已进入游戏画面
        Done,
        Failed
    };

    explicit AutoLogin(GameSlot *slot, QObject *parent = nullptr);

    /// 启动登录流程（gamePath 为游戏 exe 或 lnk 路径）
    void start(const QString &gamePath);
    void cancel();
    Phase phase() const { return m_phase; }
    QString phaseText() const;

signals:
    void finished(bool success);
    void phaseChanged(Phase phase);
    void statusMessage(const QString &msg);

private slots:
    void onTick();

private:
    bool launchGame(const QString &gamePath);
    cv::Mat captureScreen();
    void setPhase(Phase p);

    GameSlot *m_slot;
    QString m_gamePath;
    Phase m_phase = Idle;
    QProcess *m_process = nullptr;
    QTimer *m_timer;
    int m_tickCount = 0;
    int m_maxWait = 180;        // 最多等180秒（3分钟，更新可能慢）
    int m_phaseTicks = 0;       // 当前阶段已等待的 tick 数
};

#endif // AUTOLOGIN_H
