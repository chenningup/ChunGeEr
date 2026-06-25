#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include <QThread>
#include <QList>
#include "nlohmann/json.hpp"
#include "service/baseservice.h"
#include "Ui/screensharewidget.h"
#include "Ui/gameitemcapturewidget.h"
#include "autologin.h"
using json =  nlohmann::json;
QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class SlotScheduler;
class QComboBox;
class QTimer;
class QCheckBox;
class QLabel;
class QLineEdit;
class QVBoxLayout;
class QWidget;
class QGroupBox;
class QSpinBox;
class QHBoxLayout;

typedef std::function<void(const json &data)> HandelClientRecMegFun;
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void init();

    void HandelClientRecStartService(const json &msg);
    void HandelClientRecShareScreen(const json &msg);
    void HandelClientRecMouseMoveSync(const json &msg);
    void HandelClientRecMousePressSync(const json &msg);
    void HandelClientRecMouseReleaseSync(const json &msg);
    void HandelClientRecKeybordPressSync(const json &msg);
    void HandelClientRecKeybordReleaseSync(const json &msg);
    void HandelClientRecMousewheelSync(const json &msg);

    void startMainQuest();
    void openItemCapture();

private slots:
    void on_ocrEngineCombo_currentIndexChanged(int index);
    void clientRecMegSlot(const json &msg);
    void serverRecMegSlot(const json &msg);
    void on_screenShareButton_clicked();

    void clientConnectToServer();
    void clientDisConnectToServer();
    void ServerRecClientConnect(QString ip);
    void ServerRecClientDisConnect(QString ip);

    void on_startStopBtn_clicked();
    void on_taskCombo_currentIndexChanged(int index);

    void receiveLog(const QString &str);

    void on_loginBtn_clicked();
    void on_singleLoginBtn_clicked(int idx);
    void on_startAllBtn_clicked();
    void on_unifiedTaskChanged(int index);
    void on_accountComboChanged(int idx);
    void updateDungeonVisibility(int taskIndex);

private:
    void setupTaskConfigs();
    void setupAccountTaskUI();
    void buildTaskPanel();
    void refreshTaskPanel();
    void loadAccountCombo();
    void addActiveTask(int slotIndex, const QString &taskName);
    void removeActiveTask(int slotIndex);
    void startService();
    void stopService();
    void startSingleTask(int slotIdx);

    // ── 串行登录流程 ──
    bool checkSavedHwnd(int idx);            // 检测已存hwnd是否有效
    void beginLoginSequence();          // 开始依次登录
    void loginNextSlot();               // 登录下一个窗口
    void onLoginFinished(bool success);  // 单个窗口登录完成回调
    void onPostInitDone(int slotIndex);  // 初始化完成后继续登录队列
    void onAllLoginDone();              // 所有窗口登录完成，启动任务调度
    void onCaptchaRequired(int slotIndex); // 需要人工输入验证码

    Ui::MainWindow *ui;
    ScreenShareWidget *screenShareUi;
    GameItemCaptureWidget *itemCaptureUi;
    SlotScheduler *mScheduler;
    QWidget *m_activeTaskWidget = nullptr;
    QVBoxLayout *m_activeTaskLayout = nullptr;
    QHash<QString,HandelClientRecMegFun>clientRecHash;
    QHash<int, QWidget*> m_activeTaskRows;

    // ── 串行登录状态 ──
    QList<int> m_loginQueue;            // 待登录窗口索引队列
    int m_currentLoginIdx = -1;         // 当前正在登录的窗口
    AutoLogin *m_currentLogin = nullptr; // 当前登录实例
    QHash<int, AutoLogin*> m_autoLogins; // 每个窗口的 AutoLogin 实例
    QHash<int, QThread*> m_loginThreads; // 每个窗口的登录工作线程
    int m_loginSuccessCount = 0;

    // ── 验证码 ──
    QPushButton *m_captchaBtn = nullptr;  // "验证码已填，继续"按钮

    // ── 模式切换 ──
    QStackedWidget *m_modeStack = nullptr;
    QPushButton *m_accountModeBtn = nullptr;
    QPushButton *m_taskModeBtn = nullptr;

    // ── 账号面板 ──
    QLabel *m_statusLabels[3] = {};      // 登录状态标签
    QComboBox *m_accountCombos[3] = {};   // 账号选择下拉
    QPushButton *m_loginBtns[3] = {};    // 单个账号[登录]按钮

    // ── 任务面板 ──
    QGroupBox *m_taskPanel = nullptr;
    QVBoxLayout *m_taskPanelLayout = nullptr;
    QWidget *m_taskRowsWidget = nullptr;
    QHBoxLayout *m_taskRowsLayout = nullptr;
    QComboBox *m_unifiedTaskCombo = nullptr;
    QComboBox *m_taskDungeonCombo = nullptr;
    QSpinBox *m_dungeonSpin = nullptr;
    QLabel *m_dungeonLabel = nullptr;
    QLabel *m_dungeonTypeLabel = nullptr;
    QPushButton *m_startAllBtn = nullptr;
    QComboBox *m_windowCombo = nullptr;
    QPushButton *m_addWindowBtn = nullptr;
    QList<int> m_selectedWindows;
};

#endif // MAINWINDOW_H
