#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "screencapturemanager.h"
#include "StorageVidoeManager.h"
#include "mousekeyboardmanager.h"

#include "wsmanager.h"
#include "service/dungeon/dungeonservice.h"
#include "encodingmanager.h"
#include "StorageVidoeManager.h"
#include "screenshare.h"
#include "Detector/detectormanager.h"
#include <QSettings>
#include <QDebug>
bool isMaster;
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    qRegisterMetaType<ScreenCaptureManager::ScreenData>("ScreenData");

    ScreenCaptureManager::Instance().init();
    MouseKeyboardManager::Instance().init();
    EncodingManager::Instance().init();
    StorageVidoeManager::Instance().init();
    ScreenShare::Instance().init();
    DetectorManager::Instance().init("best.onnx","data.yaml");

    connect(&WsManager::Instance(),&WsManager::clientRecMeg,this,&MainWindow::clientRecMegSlot,Qt::QueuedConnection);
    connect(&WsManager::Instance(),&WsManager::clientConnectToServer,this,&MainWindow::clientConnectToServer,Qt::QueuedConnection);
    connect(&WsManager::Instance(),&WsManager::clientDisConnectToServer,this,&MainWindow::clientDisConnectToServer,Qt::QueuedConnection);
    connect(&WsManager::Instance(),&WsManager::ServerRecClientConnect,this,&MainWindow::ServerRecClientConnect,Qt::QueuedConnection);
    connect(&WsManager::Instance(),&WsManager::ServerRecClientDisConnect,this,&MainWindow::ServerRecClientDisConnect,Qt::QueuedConnection);
    connect(&ScreenShare::Instance(),&ScreenShare::showScreen,this,&MainWindow::screenShowSlot,Qt::QueuedConnection);

    QSettings settings("config.ini", QSettings::IniFormat);
    // 2. 读取配置项
    QString Type = settings.value("Basic/Type").toString();
    isMaster = Type == "Server";
    WsManager::Instance().init();
    if(isMaster)
    {
        WsManager::Instance().startServer();
    }
    else
    {
        QString serverip = settings.value("Client/ServerIp").toString();
        QString url = "ws://"+serverip+":7777";
        WsManager::Instance().startClient(url);
        ScreenCaptureManager::Instance().startCapture();
    }
    // 3. 打印读取的值
    qDebug() << "AppName:" << Type;

    //MouseKeyboardManager::Instance().clickButton("abcdef");
    //MouseKeyboardManager::Instance().clickButton(" ");
    //MouseKeyboardManager::Instance().mouseClick();
    //MouseKeyboardManager::Instance().humanMouseMove(10,10);
   // QThread::sleep(5);
    //MouseKeyboardManager::Instance().moveMouse(-25,-25);
    //MouseKeyboardManager::Instance().mouseDoubleClick();
    //MouseKeyboardManager::Instance().mouseRightClick();

    //MouseKeyboardManager::Instance().clickButton(KEY_BACKSPACE);

    //EncodingManager::Instance().startEncodeing();
    //ScreenCaptureManager::Instance().startCapture();
    //StorageVidoeManager::Instance().startSaveVideo("D:\\asdfasdf.mp4");


}

MainWindow::~MainWindow()
{
    delete ui;
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


void MainWindow::on_screenShareButton_clicked()
{
    QObject *senderObj = sender();
    // 2. 安全地转换为具体的按钮类型
    QPushButton *clickedButton = qobject_cast<QPushButton*>(senderObj);
    if (clickedButton->text() == "画面共享")
    {
        clickedButton->setText("结束");
        EncodingManager::Instance().startEncodeing();
        //ScreenShare::Instance().startShare(ui->lineEdit->text());
    }
    else if (clickedButton->text() == "结束")
    {
        clickedButton->setText("画面共享");
        EncodingManager::Instance().stopEncodeing();
        ScreenShare::Instance().stopShare();
    }
}

void MainWindow::screenShowSlot(QImage pic)
{
    ui->label->setPixmap(QPixmap::fromImage(pic));
}

void MainWindow::clientConnectToServer()
{
    ui->statuslabel->setText("已连接");
}

void MainWindow::clientDisConnectToServer()
{
    ui->statuslabel->setText("已断开");
}

void MainWindow::ServerRecClientConnect(QString ip)
{
    ui->statuslabel->setText("已连接");
}

void MainWindow::ServerRecClientDisConnect(QString ip)
{
    ui->statuslabel->setText("已断开");
}

