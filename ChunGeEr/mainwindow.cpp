#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "screencapturemanager.h"
#include "StorageVidoeManager.h"
#include "mousekeyboardmanager.h"

#include "wsmanager.h"
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->lineEdit->setText("10.8.0.3");
    qRegisterMetaType<ScreenCaptureManager::ScreenData>("ScreenData");
    StorageVidoeManager::Instance().init();
    ScreenCaptureManager::Instance().init();
    MouseKeyboardManager::Instance().init();
    WsManager::Instance().init();
}

MainWindow::~MainWindow()
{
    delete ui;
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
            WsManager::Instance().startServer();
            clickedButton->setText("结束");
            //ScreenCaptureManager::Instance().startCapture();
           // MouseKeyboardManager::Instance().clickButton("");
           // Keyboardlistener::Instance().startListen();
            // 处理按钮1的逻辑
        }
        else if (clickedButton->text() == "结束")
        {
            qDebug()<<"stop server";
            WsManager::Instance().stopServer();
            clickedButton->setText("开始");
//            ScreenCaptureManager::Instance().stopCapture();
//            StorageVidoeManager::Instance().stopSaveVideo();
        }
    }
    else
    {
        if (clickedButton->text() == "开始")
        {
            qDebug()<<"run server";
            QString ip = ui->lineEdit->text();
            QString url = "ws://"+ip+":8888";
            //WsManager::Instance().startClient(url);
            clickedButton->setText("结束");
//            ScreenCaptureManager::Instance().startCapture();
            // 处理按钮1的逻辑
        }
        else if (clickedButton->text() == "结束")
        {
            qDebug()<<"stop server";
            //WsManager::Instance().stopClient();
            clickedButton->setText("开始");
//            ScreenCaptureManager::Instance().stopCapture();
//            StorageVidoeManager::Instance().stopSaveVideo();
        }
    }
}

