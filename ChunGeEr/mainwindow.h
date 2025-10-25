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

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
private slots:

    void on_testButton_clicked();
    void clientRecMegSlot(const json &msg);
    void on_screenShareButton_clicked();

    void clientConnectToServer();
    void clientDisConnectToServer();
    void ServerRecClientConnect(QString ip);
    void ServerRecClientDisConnect(QString ip);
private:
    Ui::MainWindow *ui;
    ScreenShareWidget *screenShareUi;
    std::shared_ptr<BaseService>mService;
};
#endif // MAINWINDOW_H
