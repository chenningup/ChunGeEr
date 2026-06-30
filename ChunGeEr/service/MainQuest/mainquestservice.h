#ifndef MAINQUESTSERVICE_H
#define MAINQUESTSERVICE_H

#include "../baseservice.h"
#include "../../bitmapfontlib.h"
#include <QRect>
#include <QElapsedTimer>

// ═══════════════════════════════════════════════════════════════
// 主线任务状态机（v3）
//
// 流程:
//   ReadQuestTrack → 识别任务追踪面板文字 → 点击寻路
//     → WaitAutoPath(地图坐标帧差) → DetectTaskType
//         ├── 对话: 检测对话框→点对话按钮(循环直到对话结束)
//         └── 打怪: 字库找目标→Tab切目标→技能打怪(循环直到目标消失)
//     → ReadQuestTrack(检查进度)
//         ├── 未完成 → 继续循环
//         └── 已完成 → WaitAutoPath → DialogSubmit → Done
// ═══════════════════════════════════════════════════════════════

enum class MainQuestState {
    Idle,
    ReadQuestTrack,     // 读取画面右侧任务追踪面板
    WaitAutoPath,       // 地图坐标区域帧差检测等待到达
    DetectTaskType,     // 做任务：区分对话/打怪，循环执行直到完成
    DialogSubmit,       // 已完成→对话交付
    Done,               // 完成，循环
};

enum class QuestType {
    Unknown,
    Talk,       // 对话任务
    Kill,       // 杀怪任务
    UseItem,    // 熟悉药品
    LearnSkill, // 学习技能
};

class MainQuestService : public BaseService
{
    Q_OBJECT
public:
    explicit MainQuestService(QObject *parent = nullptr);
    void run() override;
    void startService() override;
    void stopService() override;
    void clientHandleRecMsg(const json &data) override;

    // 后台任务暂停/恢复
    void pauseBackground()  { m_bgPaused = true; }
    void resumeBackground() { m_bgPaused = false; }

private:
    void transitionTo(MainQuestState next);
    void processState();

    // ── 视觉检测（主线特有）──

    // 蓝色高亮条目（活跃任务）
    QList<QRect> findBlueHighlights(const QRect &roi);
    // 坐标链接 (数字,数字) — 青色文字
    QList<QRect> findCoordinateLinks(const QRect &roi);

    // 地图坐标区域帧差 — 只对比这一小块区域
    bool   isMapCoordChanging();

    // 检测是否在战斗中
    bool   isInCombat();

    // 用字库识别任务追踪面板文字
    QString recognizeQuestText();

    // ── 日志 ──
    void questLog(const QString &msg);

    // WASD随机移动（改变视角/位置，用于找不到任务名/交付人时重试）
    void moveRandomWASD(int durationMs);

    friend class BackgroundTaskService;

    MainQuestState currentState = MainQuestState::Idle;
    QuestType currentQuestType = QuestType::Unknown;
    QString m_currentQuestName;

    int retryCount = 0;
    int m_wasdRetry = 0;     // WASD移动重试计数（找不到任务名/交付人时）
    static const int MAX_RETRIES = 6;

    // 时间参数
    int loopIntervalMs    = 500;
    int autoPathTimeoutMs = 60000;
    int frameStableFrames = 5;
    int frameCheckMs      = 500;
    int combatTimeoutMs   = 120000;

    // ── ROI（相对游戏窗口坐标，config.ini 可配）──
    QRect questTrackRoi   = {716, 297, 267, 183};
    QRect mapCoordRoi     = {882, 57, 108, 26};
    QRect dialogBtnRoi    = {300, 550, 400, 100};
    QRect combatCheckRoi  = {0, 0, 1040, 80};
    QRect targetAvatarRoi = {5, 80, 180, 60};

    QuestType questTypeFromName(const QString &name) const;
    QString monsterNameFromQuest(const QString &questName) const;

    // ── 学习技能：门派→技能图映射 ──
    // 每个门派可以有多张技能图（按顺序尝试匹配+升级）
    // 图片放在 images/skills/ 目录下
    struct SkillLearnEntry {
        QString school;           // 门派名（如 "蜀山" "昆仑"）
        QStringList skillImages;  // 技能图片名（如 "琴心三叠" "剑气长江"）

        SkillLearnEntry() = default;
        SkillLearnEntry(const QString &s, const QStringList &imgs)
            : school(s), skillImages(imgs) {}
    };
    QList<SkillLearnEntry> m_skillTable;
    QString m_lastSchool;      // 当前账号门派（缓存，首次匹配后记住）
    void initSkillTable();     // 初始化门派技能映射表
    QStringList skillsForQuest(const QString &questName) const;  // 任务名→技能图列表

    // 字库
    bool   m_fontLoaded = false;
    QString m_fontPath;

    // 帧差用
    cv::Mat m_prevMapCoord;
    bool    m_hasPrevMap = false;
    bool    m_delivering = false;
    bool    m_bgPaused = false;
    QElapsedTimer m_elapsed;
};

#endif // MAINQUESTSERVICE_H
