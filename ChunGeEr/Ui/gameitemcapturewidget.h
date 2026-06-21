#ifndef GAMEITEMCAPTUREWIDGET_H
#define GAMEITEMCAPTUREWIDGET_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QTreeWidget>
#include <QTimer>
#include <QLineEdit>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QComboBox>
#include <QCheckBox>
#include <QHash>
#include <opencv2/opencv.hpp>
#include <windows.h>

// ════════════════════════════════════════════════
// 自定义图片标签：支持鼠标框选
// ════════════════════════════════════════════════
class CaptureImageLabel : public QLabel
{
    Q_OBJECT
public:
    explicit CaptureImageLabel(QWidget *parent = nullptr);

    void setSelectionRect(const QRect &r);
    QRect selectionRect() const { return m_selRect; }
    void clearSelection();
    bool hasSelection() const;

signals:
    void selectionChanged(const QRect &rect);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QRect m_selRect;       // 用户框选区域
    QPoint m_startPt;      // 鼠标按下起点
    bool m_selecting;      // 是否正在框选
};

// ════════════════════════════════════════════════
// 游戏物品截取窗口
// ════════════════════════════════════════════════
class GameItemCaptureWidget : public QWidget
{
    Q_OBJECT
public:
    explicit GameItemCaptureWidget(QWidget *parent = nullptr);
    ~GameItemCaptureWidget();

private slots:
    void onPauseToggle();           // 暂停/继续
    void onScreenshot();            // 截图保存选中区域
    void onTestMatch();             // 测试模板匹配
    void onOcrRecognize();          // OCR识别框选区域
    void onCaptureTick();           // 定时截图刷新
    void onSelectSaveDir();         // 选择保存目录
    void onRoiModeToggle();         // ROI模式切换

private:
    HWND findGameWindow();                       // 找游戏窗口
    cv::Mat captureGameWindow();                 // GDI截游戏窗口
    QImage matToQImage(const cv::Mat &mat);      // cv::Mat → QImage
    void addItemToList(const QString &name, const QPixmap &pixmap, const QString &filePath);
    void loadImagesFromDir();
    void rebuildActivePaths();                   // 从列表选中项重建路径+缓存
    void saveRoi(const QString &roiKey, const QRect &rect);  // 保存ROI到config

    // ── UI控件 ──
    QTimer          *m_captureTimer;
    CaptureImageLabel *m_imageLabel;
    QTreeWidget     *m_itemTree;
    QComboBox       *m_windowCombo;
    QPushButton     *m_pauseBtn;
    QPushButton     *m_screenshotBtn;
    QPushButton     *m_testBtn;
    QPushButton     *m_ocrBtn;
    QPushButton     *m_dirBtn;
    QPushButton     *m_roiBtn;
    QComboBox       *m_roiTypeCombo;
    QLineEdit       *m_nameEdit;

    // ── 状态 ──
    bool     m_paused;
    bool     m_roiMode;
    cv::Mat  m_currentFrame;
    QPixmap  m_currentPixmap;
    QString  m_saveDir;
    int      m_lastCategoryIdx;
    QStringList m_selectedPaths;
    bool     m_loadingList;          // 防止加载时触发信号
    QHash<QString, cv::Mat> m_templateCache;  // 模板缓存，避免每帧读磁盘
};

#endif // GAMEITEMCAPTUREWIDGET_H
