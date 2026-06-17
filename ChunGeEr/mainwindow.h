#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
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

private:
    void setupTaskConfigs();
    void setupAccountUI();
    void loadAccountCombo();
    void addActiveTask(int slotIndex, const QString &taskName);
    void removeActiveTask(int slotIndex);
    void startService();
    void stopService();

    // ── 串行登录流程 ──
    void beginLoginSequence();          // 开始依次登录
    void loginNextSlot();               // 登录下一个窗口
    void onLoginFinished(bool success);  // 单个窗口登录完成回调
    void onAllLoginDone();              // 所有窗口登录完成，启动任务调度

    Ui::MainWindow *ui;
    ScreenShareWidget *screenShareUi;
    GameItemCaptureWidget *itemCaptureUi;
    SlotScheduler *mScheduler;
    QCheckBox *m_slotChecks[3] = {};
    QWidget *m_activeTaskWidget = nullptr;
    QVBoxLayout *m_activeTaskLayout = nullptr;
    QHash<QString,HandelClientRecMegFun>clientRecHash;
    QHash<int, QWidget*> m_activeTaskRows;

    // ── 串行登录状态 ──
    QList<int> m_loginQueue;            // 待登录窗口索引队列
    int m_currentLoginIdx = -1;         // 当前正在登录的窗口
    AutoLogin *m_currentLogin = nullptr; // 当前登录实例
    QHash<int, AutoLogin*> m_autoLogins; // 每个窗口的 AutoLogin 实例
    int m_loginSuccessCount = 0;
};

#endif // MAINWINDOW_H
