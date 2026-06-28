#ifndef BASESERVICE_H
#define BASESERVICE_H

#include <QThread>
#include <QRect>
#include <QHash>
#include <QRandomGenerator>
#include "screencapturemanager.h"
#include "../WsManager/wsmanager.h"
#include "../bitmapfontlib.h"
#include <opencv2/opencv.hpp>
#include <QMutex>
#include "../signalslotconnector.h"

enum NameColor {
    NAME_RED,
    NAME_WHITE,
    NAME_UNKNOWN
};

class BaseService : public QThread
{
    Q_OBJECT
public:
    explicit BaseService(QObject *parent = nullptr);

    virtual void run();
    virtual void clientHandleRecMsg(const json &data);
    virtual void handlePressEvent(int vkCode);
    virtual void startService()=0;
    virtual void stopService()=0;

    void chooseLeftGame();
    void chooseRightGame();

    NameColor detectNameColor(const cv::Mat& image);

    void setDatangWindowPos();

    // ── 共用工具函数（子类直接用）──

    // 截图转 cv::Mat（BGR）
    cv::Mat screenToMat();

    // 屏幕点击：移动→点击→随机延时→随机移开
    void clickAt(int sx, int sy);
    // 点击 QRect 中心
    void clickCenter(const QRect &r);

    // 随机延时
    void randSleep(int minMs, int maxMs);

    // 模板匹配：全屏找指定模板图片，返回屏幕绝对坐标
    QRect findTemplate(const QString &name, double threshold = 0.80);
    // 模板匹配：在指定 ROI 区域内找
    QRect findTemplateInROI(const QString &name, double threshold = 0.80, const QRect &roi = QRect());

    // 检测游戏窗口，设置 gameOffsetX/gameOffsetY
    void detectGameWindow();

    // ROI 偏移：相对窗口坐标 → 屏幕绝对坐标
    QRect offsetROI(const QRect &r) const;

    // 模板根目录
    void setTemplateRoot(const QString &root) { m_templateRoot = root; }
    QString templateRoot() const { return m_templateRoot; }

signals:
    void logMessage(const QString &msg);

public slots:
    void receiveCaptureScreen(ScreenCaptureManager::ScreenData data);
    void clientRecMegSlot(const json &msg);
    void keyPressEventSlot(int vkCode);

public:
    ScreenCaptureManager::ScreenData curPic;
    bool toRun;
    QStringList tasks;
    QMutex picMutex;

    // 窗口偏移（detectGameWindow 设置）
    int gameOffsetX = 0, gameOffsetY = 0;

protected:
    // 模板缓存
    QHash<QString, cv::Mat> m_templateCache;
    QString m_templateRoot;

    // 中文路径 imread 封装
    static cv::Mat imreadUnicode(const QString &path);
};

#endif // BASESERVICE_H
