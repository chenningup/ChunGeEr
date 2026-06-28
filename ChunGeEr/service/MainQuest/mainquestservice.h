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

private:
    void transitionTo(MainQuestState next);
    void processState();

    // ── 视觉检测 ──

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

    // 模板匹配：在截图中找指定模板图片，返回匹配位置（屏幕绝对坐标），未找到返回空QRect
    QRect findTemplate(const QString &name, double threshold = 0.80);
    QRect findTemplateInROI(const QString &name, double threshold = 0.80, const QRect &roi = QRect());

    // ── 动作 ──
    void   clickAt(int sx, int sy);
    void   clickCenter(const QRect &r);

    // ── 工具 ──
    cv::Mat screenToMat();
    void   questLog(const QString &msg);
    void   detectGameWindow();
    QRect  offsetROI(const QRect &r) const;
    void   randSleep(int minMs, int maxMs);  // 随机延时，防检测

    MainQuestState currentState = MainQuestState::Idle;
    QuestType currentQuestType = QuestType::Unknown;
    QString m_currentQuestName;  // 当前识别到的任务名

    int retryCount = 0;
    static const int MAX_RETRIES = 6;

    // 时间参数
    int loopIntervalMs    = 500;
    int autoPathTimeoutMs = 60000;
    int frameStableFrames = 5;
    int frameCheckMs      = 500;
    int combatTimeoutMs   = 120000;

    // 窗口偏移
    int gameOffsetX = 0, gameOffsetY = 0;

    // ── ROI（相对游戏窗口坐标，config.ini 可配）──
    // 任务追踪面板（画面右侧）
    QRect questTrackRoi   = {716, 297, 267, 183};
    // 地图坐标区域（右上角小地图旁边）
    QRect mapCoordRoi     = {882, 57, 108, 26};
    // 对话按钮区
    QRect dialogBtnRoi    = {300, 550, 400, 100};
    // 战斗检测区（顶部血条）
    QRect combatCheckRoi  = {0, 0, 1040, 80};
    // 对手头像区（左上角目标头像）
    QRect targetAvatarRoi = {5, 80, 180, 60};

    // 任务名 → 任务类型 映射表（写死）
    QuestType questTypeFromName(const QString &name) const;
    // 任务名 → 怪物名 映射表（写死）
    QString monsterNameFromQuest(const QString &questName) const;

    // 字库
    bool   m_fontLoaded = false;
    QString m_fontPath;

    // 帧差用（只存地图坐标区域）
    cv::Mat m_prevMapCoord;
    bool    m_hasPrevMap = false;
    bool    m_delivering = false;  // 当前处于交付寻路中
    QElapsedTimer m_elapsed;

    // 模板缓存
    QHash<QString, cv::Mat> m_templateCache;
    QString m_templateRoot;
};

#endif // MAINQUESTSERVICE_H
