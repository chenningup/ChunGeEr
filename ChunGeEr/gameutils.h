#ifndef GAMEUTILS_H
#define GAMEUTILS_H

#include <QObject>
#include <QRect>
#include <QString>
#include <QStringList>
#include <QList>
#include <QHash>
#include <opencv2/opencv.hpp>

// ════════════════════════════════════════════════
// 游戏状态检测工具
// 每个检测类型在画面上有固定裁剪区域，截取后模板匹配
// ════════════════════════════════════════════════
class GameUtils : public QObject
{
    Q_OBJECT
public:
    struct MatchResult {
        QString name;           // 模板文件名（不含扩展名）
        double confidence;      // 0.0 ~ 1.0
        int centerX = 0;
        int centerY = 0;
    };

    static GameUtils &Instance();

    /// 设置模板根目录（images/ 路径）
    void setTemplateRoot(const QString &root);

    /// ── 裁剪区域配置（相对游戏窗口坐标）──

    // 地图名区域
    void setLocationROI(const QRect &r) { m_roiLocation = r; }
    QRect locationROI() const { return m_roiLocation; }

    // 等级数字区域
    void setLevelROI(const QRect &r) { m_roiLevel = r; }
    QRect levelROI() const { return m_roiLevel; }

    // 主线任务名区域
    void setMainQuestROI(const QRect &r) { m_roiMainQuest = r; }
    QRect mainQuestROI() const { return m_roiMainQuest; }

    // 技能栏区域
    void setSkillsROI(const QRect &r) { m_roiSkills = r; }
    QRect skillsROI() const { return m_roiSkills; }

    // 掉线提示区域
    void setDisconnectROI(const QRect &r) { m_roiDisconnect = r; }
    QRect disconnectROI() const { return m_roiDisconnect; }

    // 停止运动/卡住检测区域
    void setStoppedROI(const QRect &r) { m_roiStopped = r; }
    QRect stoppedROI() const { return m_roiStopped; }

    /// ── 检测接口 ──

    // 当前地图（模板匹配 locations/），未匹配到返回空
    MatchResult detectLocation(const cv::Mat &frame);

    // 角色等级（模板匹配 levels/）
    MatchResult detectLevel(const cv::Mat &frame);

    // 当前主线任务（模板匹配 mainquests/）
    MatchResult detectMainQuest(const cv::Mat &frame);

    // 技能栏（分格模板匹配 skills/），8个格子
    QStringList detectSkills(const cv::Mat &frame);

    // 是否掉线（模板匹配 disconnect/），匹配到说明掉线了
    bool isDisconnected(const cv::Mat &frame);

    // 是否停止运动/卡住（模板匹配 stopped/），匹配到说明角色没在动
    bool isStopped(const cv::Mat &frame);

    // 通用（给AutoLogin等内部用）
    QString templateRoot() const { return m_templateRoot; }
    MatchResult bestMatch(const cv::Mat &frame, const QString &templateDir, const QString &nameFilter = {});

private:
    // 裁剪 frame 中 roi 区域，若 roi 无效则返回空 Mat
    cv::Mat cropROI(const cv::Mat &frame, const QRect &roi);

    QString m_templateRoot;
    QHash<QString, cv::Mat> m_templateCache;
    cv::Mat loadCachedTemplate(const QString &path);

    double m_matchThreshold = 0.6;

    // 固定裁剪区域
    QRect m_roiLocation;
    QRect m_roiLevel;
    QRect m_roiMainQuest;
    QRect m_roiSkills;
    QRect m_roiDisconnect;
    QRect m_roiStopped;
};

#endif // GAMEUTILS_H
