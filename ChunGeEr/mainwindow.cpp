#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "screencapturemanager.h"
#include "StorageVidoeManager.h"
#include "mousekeyboardmanager.h"

#include "wsmanager.h"
#include "service/dungeon/dungeonservice.h"
#include "encodingmanager.h"
#include "StorageVidoeManager.h"
bool isMaster;
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->lineEdit->setText("10.8.0.3");
    qRegisterMetaType<ScreenCaptureManager::ScreenData>("ScreenData");
    //StorageVidoeManager::Instance().init();
    ScreenCaptureManager::Instance().init();
    MouseKeyboardManager::Instance().init();
    EncodingManager::Instance().init();
    StorageVidoeManager::Instance().init();
    WsManager::Instance().init();
    connect(&WsManager::Instance(),&WsManager::clientRecMeg,this,&MainWindow::clientRecMegSlot,Qt::QueuedConnection);
    //MouseKeyboardManager::Instance().clickButton("abcdef");
    //MouseKeyboardManager::Instance().clickButton(" ");
    //MouseKeyboardManager::Instance().mouseClick();
    //MouseKeyboardManager::Instance().humanMouseMove(10,10);
   // QThread::sleep(5);
    //MouseKeyboardManager::Instance().moveMouse(-25,-25);
    //MouseKeyboardManager::Instance().mouseDoubleClick();
    //MouseKeyboardManager::Instance().mouseRightClick();

    //MouseKeyboardManager::Instance().clickButton(KEY_BACKSPACE);

    EncodingManager::Instance().startEncodeing();
    ScreenCaptureManager::Instance().startCapture();
    StorageVidoeManager::Instance().startSaveVideo("D:\\asdfasdf.mp4");
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_clientRadioButton_clicked()
{
    isMaster = false;
    ui->clickPushButton->setText("连接");
}


void MainWindow::on_serverRadioButton_clicked()
{
    isMaster = true;
    ui->clickPushButton->setText("监听");
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
        if (clickedButton->text() == "监听")
        {
            qDebug()<<"run server";
            clickedButton->setText("结束");
            WsManager::Instance().startServer();
        }
        else if (clickedButton->text() == "结束")
        {
            qDebug()<<"stop server";
            WsManager::Instance().stopServer();
            clickedButton->setText("监听");
        }
    }
    else
    {
        if (clickedButton->text() == "连接")
        {
            qDebug()<<"run server";
            QString ip = ui->lineEdit->text();
            QString url = "ws://"+ip+":7777";
            WsManager::Instance().startClient(url);
            clickedButton->setText("结束");
        }
        else if (clickedButton->text() == "结束")
        {
            qDebug()<<"stop server";
            WsManager::Instance().stopClient();
            clickedButton->setText("连接");
        }
    }
}


void MainWindow::on_testButton_clicked()
{
//    ServerDungeonService *serivce = new ServerDungeonService();
//    serivce->startService();
    StorageVidoeManager::Instance().stopSaveVideo();
    EncodingManager::Instance().stopEncodeing();
}

void MainWindow::clientRecMegSlot(const json &msg)
{
    if(msg.contains("cmd"))
    {
        std::string cmd = msg["cmd"].get<std::string>();

        if(cmd == "StartService")
        {
            if(mService)
            {
                mService->stopService();
            }
            std::string serviceName = msg["data"]["ServiceName"].get<std::string>();
            if(serviceName == "DungeonService")
            {
                std::shared_ptr<ClientDungeonService>service = std::make_shared<ClientDungeonService>();
                service->startService();
                mService = service;
            }
            //qDebug()<<"click key "<< key;
        }
    }
}

