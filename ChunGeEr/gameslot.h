#ifndef GAMESLOT_H
#define GAMESLOT_H

#include <QObject>
#include <QString>
#include <QRect>
#include <QStringList>
#include <QHash>
#include <opencv2/opencv.hpp>
#include <windows.h>

class BaseService;

// ════════════════════════════════════════════
// 一个游戏账号 = 一个 GameSlot
// 通过角色名截图匹配窗口，独立任务配置
// ════════════════════════════════════════════
class GameSlot : public QObject
{
    Q_OBJECT
public:
    enum TaskType { None, Dungeon, MainQuest, Adventure, YiTiaoLong };
    enum State { Idle, Searching, Running, Paused, Error };

    explicit GameSlot(int index, QObject *parent = nullptr);

    int index() const { return m_index; }

    // ── 账号信息 ──
    void setAccount(const QString &acc, const QString &pwd);
    QString account() const { return m_account; }
    QString password() const { return m_password; }

    // 角色名（用于模板匹配识别窗口归属）
    void setCharName(const QString &name) { m_charName = name; }
    QString charName() const { return m_charName; }

    // ── 窗口 ──
    HWND findMatchingWindow(const cv::Mat &charNameROI); // 在所有游戏窗口中匹配这个角色的截图
    HWND hwnd() const { return m_hwnd; }
    void setHwnd(HWND h) { m_hwnd = h; }
    bool bringToFront();
    bool isForeground() const;

    // ── 登录状态 ──
    bool isLoggedIn() const { return m_loggedIn; }
    void setLoggedIn(bool v) { m_loggedIn = v; }

    // ── ROI（相对游戏窗口）──
    void setROI(int type, const QRect &r);
    QRect roi(int type) const;   // 0=location 1=level 2=mainquest 3=skills

    // ── 任务配置（独立于其他slot）──
    void setTask(TaskType t, const QString &param);
    TaskType taskType() const { return m_taskType; }
    QString taskParam() const { return m_taskParam; }
    QString taskName() const;
    bool taskEnabled() const { return m_taskEnabled; }
    void setTaskEnabled(bool v) { m_taskEnabled = v; }

    // ── 状态 ──
    State state() const { return m_state; }
    void setState(State s);
    QString stateText() const;

    // ── 任务服务 ──
    BaseService *service() const { return m_service; }
    void setService(BaseService *svc) { m_service = svc; }
    void stopService();

    // ── 检测刷新 ──
    void detectAll(const class cv::Mat &frame);

    QString currentMap() const { return m_curMap; }
    QString currentQuest() const { return m_curQuest; }
    QString currentLevel() const { return m_curLevel; }

signals:
    void stateChanged(int index, State state);
    void detected(int index);
    void windowFound(HWND hwnd);

private:
    int m_index;
    QString m_account;
    QString m_password;
    QString m_charName;    // 角色名，存为文件名如"张三.png"
    HWND m_hwnd = nullptr;
    bool m_loggedIn = false;

    QRect m_rois[4];

    TaskType m_taskType = None;
    QString m_taskParam;
    bool m_taskEnabled = true;

    State m_state = Idle;
    BaseService *m_service = nullptr;

    // ── 模板缓存 ──
    QHash<QString, cv::Mat> m_templateCache;
    cv::Mat loadTemplate(const QString &path);

    QString m_curMap;
    QString m_curQuest;
    QString m_curLevel;
    QStringList m_curSkills;
};

#endif // GAMESLOT_H
