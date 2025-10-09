#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "screencapturemanager.h"
#include "StorageVidoeManager.h"
#include "mousekeyboardmanager.h"
static WebSocketService ws;
static websocket_server_t server;
static hv::WebSocketClient wsClient;
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    server.port = 8888;
    server.ws = &ws;
    ws.onopen = std::bind(&MainWindow::connectFromClient, this, std::placeholders::_1,std::placeholders::_2);
    ws.onmessage = std::bind(&MainWindow::receiveFromClient, this, std::placeholders::_1,std::placeholders::_2);
    ws.onclose = std::bind(&MainWindow::closeFromClient, this, std::placeholders::_1);

    wsClient.onopen = std::bind(&MainWindow::connectFromServer, this);
    wsClient.onmessage = std::bind(&MainWindow::receiveFromServer , this,std::placeholders::_1);
    wsClient.onclose = std::bind(&MainWindow::closeFromServer, this);

    qRegisterMetaType<ScreenCaptureManager::ScreenData>("ScreenData");
    StorageVidoeManager::Instance().init();
    ScreenCaptureManager::Instance().init();
    MouseKeyboardManager::Instance().init();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::receiveFromClient(const WebSocketChannelPtr &channel, const std::string &msg)
{
    qDebug()<<QString::fromStdString(msg);
}

void MainWindow::connectFromClient(const WebSocketChannelPtr& channel, const HttpRequestPtr& req)
{
    clientList.push_back(channel);
}

void MainWindow::closeFromClient(const WebSocketChannelPtr&channel)
{
    for (int i = 0; i < clientList.size(); ++i)
    {
        if(clientList[i].get() == channel.get())
        {
            clientList.removeAt(i);
            break;
        }
    }
}

void MainWindow::receiveFromServer(const std::string &msg)
{
    qDebug()<<QString::fromStdString(msg);
}

void MainWindow::connectFromServer()
{
        //qDebug()<<QString::fromStdString(msg);
}

void MainWindow::closeFromServer()
{

}




void MainWindow::on_clientRadioButton_clicked()
{
    isMaster = false;
}


void MainWindow::on_serverRadioButton_clicked()
{
    isMaster = true;
}


void MainWindow::on_clickPushButton_clicked()
{
    qDebug()<<"on_clickPushButton_clicked";
    QObject *senderObj = sender();
    // 2. 安全地转换为具体的按钮类型
    QPushButton *clickedButton = qobject_cast<QPushButton*>(senderObj);

    if (!clickedButton)
    {
        qDebug() << "Button clicked:" << clickedButton->text();
        return;// 转换成功，确认发送者是 QPushButton
        // 根据不同的按钮进行不同的处理
    }
    if(isMaster)
    {
        if (clickedButton->text() == "开始")
        {
            qDebug()<<"run server";
            websocket_server_run(&server, 0);
            clickedButton->setText("结束");
            ScreenCaptureManager::Instance().startCapture();
            // 处理按钮1的逻辑
        }
        else if (clickedButton->text() == "结束")
        {
            qDebug()<<"stop server";
            websocket_server_stop(&server);
            clickedButton->setText("开始");
            ScreenCaptureManager::Instance().stopCapture();
            StorageVidoeManager::Instance().stopSaveVideo();
        }
    }
    else
    {
        if (clickedButton->text() == "开始")
        {
            qDebug()<<"run server";
            QString ip = ui->lineEdit->text();
            QString url = "ws://"+ip+":8888";
            wsClient.open(url.toStdString().data());
            clickedButton->setText("结束");
            ScreenCaptureManager::Instance().startCapture();
            // 处理按钮1的逻辑
        }
        else if (clickedButton->text() == "结束")
        {
            qDebug()<<"stop server";
            wsClient.close();
            clickedButton->setText("开始");
            ScreenCaptureManager::Instance().stopCapture();
            StorageVidoeManager::Instance().stopSaveVideo();
        }
    }
}

