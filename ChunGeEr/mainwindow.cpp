#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "screencapturemanager.h"
#include "./StorageVideo/StorageVidoeManager.h"
#include "./LeoControl/mousekeyboardmanager.h"

#include "./WsManager/wsmanager.h"
#include "service/dungeon/dungeonservice.h"
#include "service/CatchMonsters/catchmonstersservice.h"
#include "service/MainQuest/mainquestservice.h"
#include "service/Record/record.h"
#include "./KeyboardListener/keyboardlistener.h"
#include "./Encode/encodingmanager.h"
#include "./StorageVideo/StorageVidoeManager.h"
#include "./ScreenShare/screenshare.h"
#include "Detector/detectormanager.h"
#include <QSettings>
#include <QDebug>
#include <windows.h>
#include "Ocr/ocrmnager.h"
#include "signalslotconnector.h"
#include <QScreen>
bool isMaster;
QString serverIp;
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , screenShareUi(new ScreenShareWidget)
    , mService(nullptr)
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
    init();
    connect(&WsManager::Instance(),&WsManager::clientRecMeg,this,&MainWindow::clientRecMegSlot,Qt::QueuedConnection);
    connect(&WsManager::Instance(),&WsManager::serverRecMeg,this,&MainWindow::serverRecMegSlot,Qt::QueuedConnection);
    connect(&WsManager::Instance(),&WsManager::clientConnectToServer,this,&MainWindow::clientConnectToServer,Qt::QueuedConnection);
    connect(&WsManager::Instance(),&WsManager::clientDisConnectToServer,this,&MainWindow::clientDisConnectToServer,Qt::QueuedConnection);
    connect(&WsManager::Instance(),&WsManager::ServerRecClientConnect,this,&MainWindow::ServerRecClientConnect,Qt::QueuedConnection);
    connect(&WsManager::Instance(),&WsManager::ServerRecClientDisConnect,this,&MainWindow::ServerRecClientDisConnect,Qt::QueuedConnection);
    connect(&SignalSlotConnector::Instance(),&SignalSlotConnector::log,this,&MainWindow::receiveLog,Qt::QueuedConnection);

    WsManager::Instance().init();
    if(isMaster)
    {
        WsManager::Instance().startServer();
    }
    else
    {
        QString url = "ws://"+serverIp+":7777";
        WsManager::Instance().startClient(url);
        ui->screenShareButton->hide();
        ui->dungeonPushButton->hide();
    }
    // 3. 打印读取的值
    qDebug() << "AppName:" << Type;

    //MouseKeyboardManager::Instance().clickButton("abcdef");
    //MouseKeyboardManager::Instance().clickButton(" ");
    //MouseKeyboardManager::Instance().mouseClick();
    //MouseKeyboardManager::Instance().humanMouseMove(10,10);
    //QPoint current = QCursor::pos();
    //qDebug()<<"steps"<<current;
    // MouseKeyboardManager::Instance().moveMouse(-30,-30);
    // MouseKeyboardManager::Instance().moveMouse(-30,-30);
    // MouseKeyboardManager::Instance().keyPress('1');
    // MouseKeyboardManager::Instance().keyRelease('1');
    //QThread::sleep(5);
    //MouseKeyboardManager::Instance().clickButton('a');
    //qDebug()<<"click button";
    //MouseKeyboardManager::Instance().mouseRelease(2);
    //MouseKeyboardManager::Instance().mouseMoveDirect(1900,500);
    // while(true)
    // {
    //     QPoint current = QCursor::pos();
    //     qDebug()<<"steps"<<current;
    //QThread::sleepa(1);
    // }
    // current = QCursor::pos();
    // qDebug()<<"steps"<<current;
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
    delete screenShareUi;
}

void MainWindow::init()
{
    clientRecHash.insert("StartService",std::bind(&MainWindow::HandelClientRecStartService,this,std::placeholders::_1));
    clientRecHash.insert("ShareScreen",std::bind(&MainWindow::HandelClientRecShareScreen,this,std::placeholders::_1));
    clientRecHash.insert("MouseMoveSync",std::bind(&MainWindow::HandelClientRecMouseMoveSync,this,std::placeholders::_1));
    clientRecHash.insert("MousePressSync",std::bind(&MainWindow::HandelClientRecMousePressSync,this,std::placeholders::_1));
    clientRecHash.insert("MouseReleaseSync",std::bind(&MainWindow::HandelClientRecMouseReleaseSync,this,std::placeholders::_1));
    clientRecHash.insert("KeybordPressSync",std::bind(&MainWindow::HandelClientRecKeybordPressSync,this,std::placeholders::_1));
    clientRecHash.insert("KeybordReleaseSync",std::bind(&MainWindow::HandelClientRecKeybordReleaseSync,this,std::placeholders::_1));
    clientRecHash.insert("MousewheelSync",std::bind(&MainWindow::HandelClientRecMousewheelSync,this,std::placeholders::_1));
}

void MainWindow::HandelClientRecStartService(const json &msg)
{
    if(mService)
    {
        mService->stopService();
    }
    std::string serviceName = msg["data"]["ServiceName"].get<std::string>();
    if(serviceName == "DungeonService")
    {
        if(mService)
        {
            mService->stopService();
            mService->deleteLater();
        }
        ClientDungeonService *service = new ClientDungeonService();
        service->startService();
        mService = mService;
    }
}

void MainWindow::HandelClientRecShareScreen(const json &msg)
{
    std::string serviceName = msg["data"]["OperateType"].get<std::string>();
    if(serviceName == "Start")
    {
        EncodingManager::Instance().startEncodeing();
        ScreenCaptureManager::Instance().startCapture();
        ScreenShare::Instance().startShare();
    }
    else
    {
        EncodingManager::Instance().stopEncodeing();
        ScreenCaptureManager::Instance().stopCapture();
        ScreenShare::Instance().stopShare();
    }
}

void MainWindow::HandelClientRecMouseMoveSync(const json &msg)
{
    int x = msg["data"]["x"].get<int>();
    int y = msg["data"]["y"].get<int>();
    if(MouseKeyboardManager::Instance().isOpen())
    {
        LeoTask task ;
        task.x = x;
        task.y = y;
        task.task = "MouseMoveSync";
        MouseKeyboardManager::Instance().pushbackTask(task);
       // MouseKeyboardManager::Instance().mouseMoveDirect(x,y);
    }
    else
    {
        SetCursorPos(x, y);
    }
}

void MainWindow::HandelClientRecMousePressSync(const json &msg)
{
    int x = msg["data"]["x"].get<int>();
    int y = msg["data"]["y"].get<int>();
    if(MouseKeyboardManager::Instance().isOpen())
    {
        LeoTask task ;
        task.x = x;
        task.y = y;
        task.task = "MouseMoveSync";
        MouseKeyboardManager::Instance().pushbackTask(task);
        //MouseKeyboardManager::Instance().mouseMoveDirect(x,y);
        std::string type = msg["data"]["type"].get<std::string>();

        LeoTask task1 ;
        task1.x = x;
        task1.y = y;
        task1.task = "MousePressSync";
        if(type == "left")
        {
            task1.mouseType  = MOUSE_LEFT;
            //MouseKeyboardManager::Instance().mousePress(MOUSE_LEFT);
        }
        else
        {
            task1.mouseType  = MOUSE_RIGHT;
            //MouseKeyboardManager::Instance().mousePress(MOUSE_RIGHT);
        }
        MouseKeyboardManager::Instance().pushbackTask(task1);
    }
    else
    {
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
        inputs[0].type = INPUT_MOUSE;
        inputs[0].mi.dwFlags = dwtype;
        SendInput(1, inputs, sizeof(INPUT));
    }
}

void MainWindow::HandelClientRecMouseReleaseSync(const json &msg)
{
    int x = msg["data"]["x"].get<int>();
    int y = msg["data"]["y"].get<int>();
    if(MouseKeyboardManager::Instance().isOpen())
    {
        LeoTask task ;
        task.x = x;
        task.y = y;
        task.task = "MouseMoveSync";
        MouseKeyboardManager::Instance().pushbackTask(task);
        //MouseKeyboardManager::Instance().mouseMoveDirect(x,y);
        std::string type = msg["data"]["type"].get<std::string>();
        LeoTask task1 ;
        task1.task = "MouseReleaseSync";
        if(type == "left")
        {
            task1.mouseType  = MOUSE_LEFT;
        }
        else
        {
            task1.mouseType  = MOUSE_RIGHT;
        }
        MouseKeyboardManager::Instance().pushbackTask(task1);
    }
    else
    {
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
        inputs[0].type = INPUT_MOUSE;
        inputs[0].mi.dwFlags = dwtype;
        SendInput(1, inputs, sizeof(INPUT));
    }
}

void MainWindow::HandelClientRecKeybordPressSync(const json &msg)
{
    int key  = msg["data"]["Key"].get<int>();
    if(MouseKeyboardManager::Instance().isOpen())
    {
        LeoTask task ;
        task.task = "KeybordPressSync";
        task.key = key;
        MouseKeyboardManager::Instance().pushbackTask(task);
        //MouseKeyboardManager::Instance().keyPress(key);
    }
    else
    {
        INPUT ip;
        ip.type = INPUT_KEYBOARD;
        ip.ki.wVk = key; // 'A' 的虚拟键码
        ip.ki.dwFlags = 0; // 0 表示按下
        SendInput(1, &ip, sizeof(INPUT));

        // 设置键盘释放事件
        ip.ki.dwFlags = KEYEVENTF_KEYUP; // 键释放标志
        SendInput(1, &ip, sizeof(INPUT));
    }
}

void MainWindow::HandelClientRecKeybordReleaseSync(const json &msg)
{
    int key  = msg["data"]["Key"].get<int>();
    if(MouseKeyboardManager::Instance().isOpen())
    {
        //MouseKeyboardManager::Instance().keyRelease(key);
        LeoTask task ;
        task.task = "KeybordReleaseSync";
        task.key = key;
        MouseKeyboardManager::Instance().pushbackTask(task);
    }
    else
    {

        INPUT ip;
        ip.type = INPUT_KEYBOARD;
        ip.ki.wVk = key; // 'A' 的虚拟键码

        // 设置键盘释放事件
        ip.ki.dwFlags = KEYEVENTF_KEYUP; // 键释放标志
        SendInput(1, &ip, sizeof(INPUT));
    }
}

void MainWindow::HandelClientRecMousewheelSync(const json &msg)
{
    int dis  = msg["data"]["dis"].get<int>();
    INPUT input;
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_WHEEL;
    input.mi.mouseData = dis;  // 正数向上滚动，负数向下滚动

    SendInput(1, &input, sizeof(INPUT));
}

void MainWindow::on_testButton_clicked()
{
    //    ServerDungeonService *serivce = new ServerDungeonService();
    //    serivce->startService();
    // StorageVidoeManager::Instance().stopSaveVideo();
    // EncodingManager::Instance().stopEncodeing();
   CatchMonstersService *service = new CatchMonstersService();
   service->setDatangWindowPos();
   service->startService();
   ScreenCaptureManager::Instance().startCapture();


   // QTimer::singleShot(5000, this, [this,service]() {
   //     service->test();
   //     qDebug() << "Single shot timer triggered after 1 second";
   // });
//   cv::Mat img = cv::imread("Snipaste_2025-11-04_21-46-24.bmp");

//   cv::Rect ocr_rect(100, 40, 80, 20); // 从 (100,50) 开始，截取 200x150 的区域
//   // 截取 ROI
//   cv::Mat cormat = img(ocr_rect).clone();
//   cv::imshow("asdfasdf", cormat);
//   cv::waitKey();
//   NameColor color = service->detectNameColor(cormat);

//   switch (color) {
//   case NAME_RED: qDebug()<<"检测到红名\n"; break;
//   case NAME_WHITE:  qDebug() << "检测到白名\n"; break;
//   default:  qDebug() << "颜色不确定\n"; break;
//   }
    // Record *rec = new Record();
    // rec->startService();
    // ScreenCaptureManager::Instance().startCapture();
}

void MainWindow::on_ocrEngineCombo_currentIndexChanged(int index)
{
    if (index == 0) {
        OcrMnager::Instance().setEngine(OcrMnager::EnginePaddleOCR);
        ui->textEdit->append("[OCR] 切换到 PaddleOCR");
    } else {
        OcrMnager::Instance().setEngine(OcrMnager::EngineTesseract);
        ui->textEdit->append("[OCR] 切换到 Tesseract");
    }
}

void MainWindow::clientRecMegSlot(const json &msg)
{
    if(msg.contains("cmd"))
    {
        QString cmd = QString::fromStdString(msg["cmd"].get<std::string>());
        if(clientRecHash.contains(cmd))
        {
            clientRecHash[cmd](msg);
        }
    }
}

void MainWindow::serverRecMegSlot(const json &msg)
{
    if(msg.contains("cmd"))
    {
        QString cmd = QString::fromStdString(msg["cmd"].get<std::string>());
        if(cmd == "ClientScreenAnys")
        {
            int width = msg["data"]["width"].get<int>();
            int height = msg["data"]["height"].get<int>();
            screenShareUi->setGeometry(0,0,width,height);
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
        screenShareUi->setWindowFlags(Qt::Window);
        screenShareUi->setWindowFlags(Qt::FramelessWindowHint);

        QScreen *primaryScreen = QGuiApplication::primaryScreen();
        if (primaryScreen)
        {
            if(screenShareUi->width() == primaryScreen->size().width() &&
                screenShareUi->height() == primaryScreen->size().height())
            {
                screenShareUi->showFullScreen();
            }
            else
            {
                screenShareUi->showNormal();
            }
        }
        //
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
        screenShareUi->setWindowFlags(Qt::SubWindow);
        screenShareUi->hide();
    }
}


void MainWindow::clientConnectToServer()
{
    QScreen *primaryScreen = QGuiApplication::primaryScreen();
    if (primaryScreen)
    {
        qDebug() << "主屏幕分辨率:"
                 << primaryScreen->size().width() << "x"
                 << primaryScreen->size().height();
    }
    ui->statuslabel->setText("已连接");
    json cmd ;
    cmd["cmd"] = "ClientScreenAnys";
    json data;
    data["width"] = primaryScreen->size().width();
    data["height"] = primaryScreen->size().height();
    cmd["data"] = data;
    WsManager::Instance().sendMsgToServer(cmd.dump());
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


void MainWindow::on_dungeonPushButton_clicked()
{
    QObject *senderObj = sender();
    // 2. 安全地转换为具体的按钮类型
    QPushButton *clickedButton = qobject_cast<QPushButton*>(senderObj);
    if (clickedButton->text() == "副本")
    {
        clickedButton->setText("结束");
        if(mService)
        {
            mService->stopService();
            mService->deleteLater();
        }
        ServerDungeonService *serivce = new ServerDungeonService() ;
        serivce->startService();
        mService = serivce;
    }
    else if (clickedButton->text() == "结束")
    {
        clickedButton->setText("副本");
        if(mService)
        {
            mService->stopService();
        }
    }
}

void MainWindow::on_mainQuestButton_clicked()
{
    QObject *senderObj = sender();
    QPushButton *clickedButton = qobject_cast<QPushButton*>(senderObj);
    if (clickedButton->text() == "主线任务")
    {
        clickedButton->setText("结束");
        if(mService)
        {
            mService->stopService();
            mService->deleteLater();
        }
        MainQuestService *service = new MainQuestService();
        service->startService();
        ScreenCaptureManager::Instance().startCapture();
        mService = service;
    }
    else if (clickedButton->text() == "结束")
    {
        clickedButton->setText("主线任务");
        if(mService)
        {
            mService->stopService();
        }
        ScreenCaptureManager::Instance().stopCapture();
    }
}

void MainWindow::startMainQuest()
{
    if(mService)
    {
        mService->stopService();
        mService->deleteLater();
    }
    MainQuestService *service = new MainQuestService();
    service->startService();
    ScreenCaptureManager::Instance().startCapture();
    mService = service;
    ui->mainQuestButton->setText(QString::fromUtf8("结束"));
}

void MainWindow::receiveLog(const QString &str)
{
    ui->textEdit->append(str);
}

