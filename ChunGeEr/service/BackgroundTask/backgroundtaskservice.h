#ifndef BACKGROUNDTASKSERVICE_H
#define BACKGROUNDTASKSERVICE_H

#include "../baseservice.h"
#include <QAtomicInt>

// ═══════════════════════════════════════════════════════════════
// 后台任务服务
//
// 所有任务服务共享的后台检测：
//   - 登录奖励领取（已实现）
//   - 人物死亡检测（占位）
//   - 掉线检测（占位）
//   - 人物快没血（占位）
//
// 当后台任务需要控制鼠标时：
//   1. 发送 pauseRequested 信号 → 上层任务暂停
//   2. 执行鼠标操作
//   3. 发送 resumeRequested 信号 → 上层任务恢复
// ═══════════════════════════════════════════════════════════════

class GameSlot;

class BackgroundTaskService : public BaseService
{
    Q_OBJECT
public:
    explicit BackgroundTaskService(QObject *parent = nullptr);
    void run() override;
    void startService() override;
    void stopService() override;

    // 上层任务调用：暂停/恢复后台检测
    void pauseBackground()  { m_paused.storeRelaxed(1); }
    void resumeBackground() { m_paused.storeRelaxed(0); }

    // 设置账号信息（掉线重连用）
    void setAccount(const QString &acc, const QString &pwd) {
        m_account = acc; m_password = pwd;
    }

signals:
    void pauseRequested();
    void resumeRequested();
    void log(const QString &msg);
    void reconnectStarted(int slotIndex);
    void reconnectFinished(int slotIndex, bool success);

private:
    // 检测并领取登录奖励，返回是否执行了领取
    bool checkLoginReward();

    // 检测掉线并重连，返回是否执行了重连
    bool checkDisconnect();

    void bgLog(const QString &msg);

    // 登录奖励领取区域（config.ini ROIs/LoginReward）
    QRect loginRewardRoi;

    // 掉线检测区域（config.ini ROIs/Disconnect）
    QRect disconnectRoi;

    QAtomicInt m_paused{0};  // 0=运行, 1=暂停

    // 账号信息（掉线重连用）
    QString m_account;
    QString m_password;

    // 掉线重连冷却（避免短时间内反复重连）
    qint64 m_lastReconnectMs = 0;
};

#endif // BACKGROUNDTASKSERVICE_H
