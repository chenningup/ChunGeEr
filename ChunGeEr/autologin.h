#ifndef AUTOLOGIN_H
#define AUTOLOGIN_H

#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QFileInfo>
#include <opencv2/opencv.hpp>
#include "gameslot.h"

// ════════════════════════════════════════════
// 自动登录：启动游戏 → 检测登录界面 → 输入账号密码
// ════════════════════════════════════════════
class AutoLogin : public QObject
{
    Q_OBJECT
public:
    enum Phase { Idle, Launching, WaitingWindow, DetectingLogin, Typing, LoggingIn, Verifying, Done, Failed };

    explicit AutoLogin(GameSlot *slot, QObject *parent = nullptr);

    void start();
    void cancel();
    Phase phase() const { return m_phase; }
    QString phaseText() const;

signals:
    void finished(bool success);
    void phaseChanged(Phase phase);

private slots:
    void onTick();

private:
    bool ensureWindow();         // 确保游戏窗口存在
    bool launchGame();           // 启动游戏进程
    bool detectLogin(const cv::Mat &frame);
    bool typeCredentials();      // Arduino输入账号密码
    bool clickLogin();           // 点登录按钮
    bool verifyInGame(const cv::Mat &frame);  // 确认进入游戏
    cv::Mat captureScreen();     // 截屏
    void setPhase(Phase p);

    GameSlot *m_slot;
    Phase m_phase = Idle;
    QProcess *m_process = nullptr;
    QTimer *m_timer;
    int m_tickCount = 0;
    int m_maxWait = 120;         // 最多等120秒
};

#endif // AUTOLOGIN_H
