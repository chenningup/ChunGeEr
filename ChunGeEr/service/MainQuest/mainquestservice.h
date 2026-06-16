#ifndef MAINQUESTSERVICE_H
#define MAINQUESTSERVICE_H

#include "../baseservice.h"
#include <QSettings>
#include <QRegularExpression>
#include <QRect>
#include <tesseract/baseapi.h>

enum class MainQuestState {
    Idle,
    OpenQuestPanel,         // L 键→视觉确认面板开
    ClickQuestAndGo,        // 点击任务+前往（固定位+视觉兜底）
    WaitAutoPath,           // 帧差检测等待到达
    DetectDialog,           // 视觉检测对话框
    ClickDialogButton,      // 点击对话按钮
    CheckQuestProgress,     // 检查进度
    WaitAfterAction,        // 通用等待
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
    bool   isPanelOpen();           // 中央区域暗像素比例 > 30%
    bool   isDialogOpen();          // 对话框区域有暗色矩形
    bool   detectScrollbar(QRect &scrollBar); // 检测任务列表滚动条
    void   scrollToBottom();        // 滚动任务列表到底
    QList<QRect> findGoldButtons(const QRect &roi);  // 金色/黄色按钮
    QList<QRect> findBlueHighlights(const QRect &roi); // 蓝色高亮条目
    QList<QRect> findCoordinateLinks(const QRect &roi); // 坐标链接 (数字,数字)
    double darkRatio(const QRect &roi);               // ROI内暗像素比例
    
    // ── 动作 ──
    void   pressL();
    void   clickAt(int sx, int sy);  // 硬件鼠标点击

    // ── 工具 ──
    cv::Mat screenToMat();
    void   questLog(const QString &msg);
    void   detectGameWindow();

    MainQuestState currentState = MainQuestState::Idle;
    int retryCount = 0;
    static const int MAX_RETRIES = 6;

    // 时间参数
    int loopIntervalMs    = 500;
    int longWaitMs        = 2000;
    int autoPathTimeoutMs = 45000;  // 寻路最长等45秒
    int frameStableFrames = 5;      // 连续稳定帧数判定到达
    int frameCheckMs      = 500;    // 帧差检查间隔

    // 窗口偏移
    int gameOffsetX = 8, gameOffsetY = 30;

    // ── 视觉参数 ──
    // 面板检测 ROI（中央大面积）
    QRect panelCheckRoi   = {200, 100, 600, 500};
    // 按钮搜索 ROI（面板右下按钮位）
    QRect goBtnSearchRoi  = {550, 480, 250, 100};
    // 任务列表点击位（面板中央偏左）
    QRect questClickRoi   = {250, 220, 200, 250};
    // 任务描述区（右侧，找坐标链接）
    QRect questDescRoi    = {480, 200, 320, 350};
    // 对话框检测 ROI
    QRect dialogCheckRoi  = {150, 350, 700, 300};
    // 对话按钮 ROI
    QRect dialogBtnRoi    = {300, 550, 400, 100};
};

#endif // MAINQUESTSERVICE_H
