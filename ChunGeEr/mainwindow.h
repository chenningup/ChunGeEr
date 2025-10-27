#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "nlohmann/json.hpp"
#include "service/baseservice.h"
#include "Ui/screensharewidget.h"
using json =  nlohmann::json;
QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

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
private slots:

    void on_testButton_clicked();
    void clientRecMegSlot(const json &msg);
    void on_screenShareButton_clicked();

    void clientConnectToServer();
    void clientDisConnectToServer();
    void ServerRecClientConnect(QString ip);
    void ServerRecClientDisConnect(QString ip);
    void on_dungeonPushButton_clicked();

    void receiveLog(const QString &str);
private:
    Ui::MainWindow *ui;
    ScreenShareWidget *screenShareUi;
    std::shared_ptr<BaseService>mService;
    std::shared_ptr<BaseService>mLastService;
    QHash<QString,HandelClientRecMegFun>clientRecHash;
};
#endif // MAINWINDOW_H
