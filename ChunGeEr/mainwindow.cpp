#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "screencapturemanager.h"
#include "StorageVidoeManager.h"
#include "mousekeyboardmanager.h"

#include "wsmanager.h"
#include "service/dungeon/dungeonservice.h"
#include "service/CatchMonsters/catchmonstersservice.h"
#include "keyboardlistener.h"
#include "encodingmanager.h"
#include "StorageVidoeManager.h"
#include "screenshare.h"
#include "Detector/detectormanager.h"
#include <QSettings>
#include <QDebug>
#include <windows.h>
#include "Ocr/ocrmnager.h"
bool isMaster;
QString serverIp;
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    qRegisterMetaType<ScreenCaptureManager::ScreenData>("ScreenData");

    QSettings settings("config.ini", QSettings::IniFormat);
    // 2. 读取配置项
    QString Type = settings.value("Basic/Type").toString();
    isMaster = Type == "Server";
    serverIp = settings.value("Client/ServerIp").toString();
    ScreenCaptureManager::Instance().init();
    MouseKeyboardManager::Instance().init();
    EncodingManager::Instance().init();
    StorageVidoeManager::Instance().init();
    ScreenShare::Instance().init();
    DetectorManager::Instance().init("best.onnx","data.yaml");
    OcrMnager::Instance().init();
    connect(&WsManager::Instance(),&WsManager::clientRecMeg,this,&MainWindow::clientRecMegSlot,Qt::QueuedConnection);
    connect(&WsManager::Instance(),&WsManager::clientConnectToServer,this,&MainWindow::clientConnectToServer,Qt::QueuedConnection);
    connect(&WsManager::Instance(),&WsManager::clientDisConnectToServer,this,&MainWindow::clientDisConnectToServer,Qt::QueuedConnection);
    connect(&WsManager::Instance(),&WsManager::ServerRecClientConnect,this,&MainWindow::ServerRecClientConnect,Qt::QueuedConnection);
    connect(&WsManager::Instance(),&WsManager::ServerRecClientDisConnect,this,&MainWindow::ServerRecClientDisConnect,Qt::QueuedConnection);
    connect(&ScreenShare::Instance(),&ScreenShare::showScreen,this,&MainWindow::screenShowSlot,Qt::QueuedConnection);


    WsManager::Instance().init();
    if(isMaster)
    {
        WsManager::Instance().startServer();
    }
    else
    {
        QString url = "ws://"+serverIp+":7777";
        WsManager::Instance().startClient(url);
        ScreenCaptureManager::Instance().startCapture();
        ui->screenShareButton->hide();
    }
    // 3. 打印读取的值
    qDebug() << "AppName:" << Type;

    //MouseKeyboardManager::Instance().clickButton("abcdef");
    //MouseKeyboardManager::Instance().clickButton(" ");
    //MouseKeyboardManager::Instance().mouseClick();
    //MouseKeyboardManager::Instance().humanMouseMove(10,10);
        QPoint current = QCursor::pos();
        qDebug()<<"steps"<<current;
    MouseKeyboardManager::Instance().moveMouse(-30,-30);
    // while(true)
    // {
    //     QPoint current = QCursor::pos();
    //     qDebug()<<"steps"<<current;
         QThread::sleep(1);
    // }
        current = QCursor::pos();
        qDebug()<<"steps"<<current;
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
    // StorageVidoeManager::Instance().stopSaveVideo();
    // EncodingManager::Instance().stopEncodeing();
   CatchMonstersService *service = new CatchMonstersService();
   service->startService();
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
            return;
            //qDebug()<<"click key "<< key;
        }
        if(cmd == "ShareScreen")
        {
            std::string serviceName = msg["data"]["OperateType"].get<std::string>();
            if(serviceName == "Start")
            {
                EncodingManager::Instance().startEncodeing();
                ScreenShare::Instance().startShare();
            }
            else
            {
                EncodingManager::Instance().stopEncodeing();
                ScreenShare::Instance().stopShare();
            }
            return;
        }
        if(cmd == "MouseMoveSync")
        {
            int x = msg["data"]["x"].get<int>();
            int y = msg["data"]["y"].get<int>();
            SetCursorPos(x, y);
            return;
        }
        if(cmd == "MousePressSync")
        {
            int x = msg["data"]["x"].get<int>();
            int y = msg["data"]["y"].get<int>();
            SetCursorPos(x, y);

            std::string type = msg["data"]["type"].get<std::string>();
            DWORD dwtype;
            if(type == "left")
            {
                dwtype = MOUSEEVENTF_LEFTDOWN;
            }
            else
            {
                dwtype = MOUSEEVENTF_RIGHTDOWN;
            }
            INPUT inputs[1] = {0};
            // 按下事件
            inputs[0].type = INPUT_MOUSE;
            inputs[0].mi.dwFlags = dwtype;
            // 释放事件
//            inputs[1].type = INPUT_MOUSE;
//            inputs[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
            // 发送输入事件
            SendInput(1, inputs, sizeof(INPUT));
            return;
        }
        if(cmd == "MouseReleaseSync")
        {
            int x = msg["data"]["x"].get<int>();
            int y = msg["data"]["y"].get<int>();
            SetCursorPos(x, y);
            std::string type = msg["data"]["type"].get<std::string>();
            DWORD dwtype;
            if(type == "left")
            {
                dwtype = MOUSEEVENTF_LEFTUP;
            }
            else
            {
                dwtype = MOUSEEVENTF_RIGHTUP;
            }
            INPUT inputs[1] = {0};
            // 释放事件
            inputs[0].type = INPUT_MOUSE;
            inputs[0].mi.dwFlags = dwtype;
            // 释放事件
            //            inputs[1].type = INPUT_MOUSE;
            //            inputs[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
            // 发送输入事件
            SendInput(1, inputs, sizeof(INPUT));
            return;
        }
        if( cmd == "KeybordPressSync" )
        {
            int key  = msg["data"]["Key"].get<int>();
            INPUT ip;
            ip.type = INPUT_KEYBOARD;
            ip.ki.wVk = key; // 'A' 的虚拟键码
            ip.ki.dwFlags = 0; // 0 表示按下
            SendInput(1, &ip, sizeof(INPUT));

            // 设置键盘释放事件
            ip.ki.dwFlags = KEYEVENTF_KEYUP; // 键释放标志
            SendInput(1, &ip, sizeof(INPUT));
             return;
        }
        if( cmd == "KeybordReleaseSync" )
        {
             int key  = msg["data"]["Key"].get<int>();
             INPUT ip;
             ip.type = INPUT_KEYBOARD;
             ip.ki.wVk = key; // 'A' 的虚拟键码

             // 设置键盘释放事件
             ip.ki.dwFlags = KEYEVENTF_KEYUP; // 键释放标志
             SendInput(1, &ip, sizeof(INPUT));
             return;
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
        ScreenShare::Instance().startShare();
        Keyboardlistener::Instance().startListen();
        json cmd ;
        cmd["cmd"] = "ShareScreen";
        json data;
        data["OperateType"] = "Start";
        cmd["data"] = data;
        WsManager::Instance().sendMsgToClient(cmd.dump());
    }
    else if (clickedButton->text() == "结束")
    {
        clickedButton->setText("画面共享");
        ScreenShare::Instance().stopShare();
        json cmd ;
        cmd["cmd"] = "ShareScreen";
        json data;
        data["OperateType"] = "Stop";
        cmd["data"] = data;
        WsManager::Instance().sendMsgToClient(cmd.dump());
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

