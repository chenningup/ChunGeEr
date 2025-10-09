#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "hv/WebSocketServer.h"
#include "hv/EventLoop.h"
#include "hv/htime.h"
#include "hv/hssl.h"
#include "hv/WebSocketClient.h"
#include "nlohmann/json.hpp"
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

    void receiveFromClient(const WebSocketChannelPtr& channel, const std::string& msg);
    void connectFromClient(const WebSocketChannelPtr& channel, const HttpRequestPtr& req);
    void closeFromClient(const WebSocketChannelPtr& channel);

    void receiveFromServer(const std::string& msg);
    void connectFromServer();
    void closeFromServer();
private slots:
    void on_clientRadioButton_clicked();
    void on_serverRadioButton_clicked();
    void on_clickPushButton_clicked();

private:
    Ui::MainWindow *ui;
    bool isMaster;
    QList<WebSocketChannelPtr>clientList;
};
#endif // MAINWINDOW_H
