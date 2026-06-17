#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include "nlohmann/json.hpp"
#include "service/baseservice.h"
#include "Ui/screensharewidget.h"
#include "Ui/gameitemcapturewidget.h"
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
    void startLauncherHandler();
    void handleLauncherStep();

    Ui::MainWindow *ui;
    ScreenShareWidget *screenShareUi;
    GameItemCaptureWidget *itemCaptureUi;
    BaseService *mService;
    SlotScheduler *mScheduler;
    QCheckBox *m_slotChecks[3] = {};
    QWidget *m_activeTaskWidget = nullptr;
    QVBoxLayout *m_activeTaskLayout = nullptr;
    QHash<QString,HandelClientRecMegFun>clientRecHash;
    QHash<int, QWidget*> m_activeTaskRows;
    QTimer *m_launcherTimer = nullptr;
    int m_launcherStep = 0;
    int m_launcherTicks = 0;
};

#endif // MAINWINDOW_H
