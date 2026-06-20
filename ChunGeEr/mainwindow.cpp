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
#include "Ocr/ocrmnager.h"
#include "signalslotconnector.h"
#include "slotscheduler.h"
#include "accountdialog.h"
#include "gameutils.h"
#include "XuLog.h"

#include <QSettings>
#include <QDebug>
#include <QLabel>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QTimer>
#include <QPixmap>
#include <QFrame>
#include <QImage>
#include <QScreen>
#include <QGuiApplication>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QLineEdit>
#include <QProcess>
#include <QFileInfo>
#include <QThread>
#include <QMessageBox>
#include <QPushButton>
#include <windows.h>
#include <opencv2/opencv.hpp>

bool isMaster = false;
QString serverIp = "";

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , screenShareUi(new ScreenShareWidget)
    , mScheduler(new SlotScheduler(this))
    , itemCaptureUi(nullptr)
{
    ui->setupUi(this);
    resize(800, 550);
    setMinimumHeight(500);
    qRegisterMetaType<ScreenCaptureManager::ScreenData>("ScreenData");

    QSettings settings(QCoreApplication::applicationDirPath() + "/config.ini", QSettings::IniFormat);
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
    OcrMnager::Instance().setEngine(OcrMnager::EngineTesseract);
    GameUtils::Instance().setTemplateRoot(QCoreApplication::applicationDirPath() + "/images");
    GameUtils::Instance().loadROIs();
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
    }

    ui->ocrEngineCombo->setCurrentIndex(1); // Tesseract

    // ── 配置区：布局化（替代绝对定位）──
    setupAccountTaskUI();

    // 外层 layout 比例 1:3
    auto *rootLayout = static_cast<QVBoxLayout *>(centralWidget()->layout());
    if (rootLayout) {
        rootLayout->setStretch(2, 2);  // configArea
        rootLayout->setStretch(4, 3);  // textEdit
    }

    setupTaskConfigs();
    loadAccountCombo();
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
    std::string serviceName = msg["data"]["ServiceName"].get<std::string>();
    if(serviceName == "DungeonService")
    {
        // 客户端模式：直接停所有 slot 的 service，启动新的
        stopService();
        auto *slot = mScheduler->slot(0); // 默认窗口0
        if (slot) {
            auto *svc = new ClientDungeonService();
            svc->start();
            slot->setService(svc);
            slot->setState(GameSlot::Running);
        }
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
        std::string type = msg["data"]["type"].get<std::string>();
        LeoTask task1 ;
        task1.x = x;
        task1.y = y;
        task1.task = "MousePressSync";
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
    }
    else
    {
        INPUT ip;
        ip.type = INPUT_KEYBOARD;
        ip.ki.wVk = key;
        ip.ki.dwFlags = 0;
        SendInput(1, &ip, sizeof(INPUT));
        ip.ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(1, &ip, sizeof(INPUT));
    }
}

void MainWindow::HandelClientRecKeybordReleaseSync(const json &msg)
{
    int key  = msg["data"]["Key"].get<int>();
    if(MouseKeyboardManager::Instance().isOpen())
    {
        LeoTask task ;
        task.task = "KeybordReleaseSync";
        task.key = key;
        MouseKeyboardManager::Instance().pushbackTask(task);
    }
    else
    {
        INPUT ip;
        ip.type = INPUT_KEYBOARD;
        ip.ki.wVk = key;
        ip.ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(1, &ip, sizeof(INPUT));
    }
}

void MainWindow::HandelClientRecMousewheelSync(const json &msg)
{
    int dis  = msg["data"]["dis"].get<int>();
    INPUT input;
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_WHEEL;
    input.mi.mouseData = dis;
    SendInput(1, &input, sizeof(INPUT));
}



void MainWindow::on_ocrEngineCombo_currentIndexChanged(int index)
{
    OcrMnager::Instance().setEngine(static_cast<OcrMnager::OcrEngine>(index));
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
        if(clientRecHash.contains(cmd))
        {
            clientRecHash[cmd](msg);
        }
    }
}

void MainWindow::on_screenShareButton_clicked()
{
    if(screenShareUi->isVisible())
    {
        screenShareUi->hide();
    }
    else
    {
        screenShareUi->show();
    }
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
}

void MainWindow::ServerRecClientDisConnect(QString ip)
{
}

// ════════════════════════════════════════════════
// 任务配置面板
// ════════════════════════════════════════════════
void MainWindow::setupTaskConfigs()
{
    auto *stack = ui->taskConfigStack;
    if (!stack) return;

    // 页面0：副本
    {
        auto *page = new QWidget();
        auto *lay = new QHBoxLayout(page);
        lay->setContentsMargins(0, 0, 0, 0);
        auto *combo = new QComboBox(page);
        combo->setObjectName("dungeonCombo");
        combo->addItems({QString::fromUtf8("恶人谷"), QString::fromUtf8("屠狼洞"),
            QString::fromUtf8("凤鸣山"), QString::fromUtf8("冰风洞"),
            QString::fromUtf8("长寿宫"), QString::fromUtf8("夜哭山庄")});
        auto *spin = new QSpinBox(page);
        spin->setObjectName("dungeonCount");
        spin->setRange(1, 99);
        spin->setValue(1);
        lay->addWidget(new QLabel(QString::fromUtf8("副本:"), page));
        lay->addWidget(combo);
        lay->addWidget(new QLabel(QString::fromUtf8("次数:"), page));
        lay->addWidget(spin);
        lay->addStretch();
        stack->addWidget(page);
    }

    // 页面1：主线任务
    {
        auto *page = new QWidget();
        auto *lay = new QHBoxLayout(page);
        lay->setContentsMargins(0, 0, 0, 0);
        auto *loopChk = new QCheckBox(QString::fromUtf8("循环"), page);
        loopChk->setObjectName("mainQuestLoop");
        loopChk->setChecked(true);
        lay->addWidget(loopChk);
        lay->addStretch();
        stack->addWidget(page);
    }

    // 页面2：冒险
    {
        auto *page = new QWidget();
        auto *lay = new QHBoxLayout(page);
        lay->setContentsMargins(0, 0, 0, 0);
        auto *advCombo = new QComboBox(page);
        advCombo->setObjectName("adventureCombo");
        advCombo->addItems({QString::fromUtf8("普通冒险"), QString::fromUtf8("平定安邦")});
        lay->addWidget(advCombo);
        lay->addStretch();
        stack->addWidget(page);
    }

    // 页面3：一条龙
    {
        auto *page = new QWidget();
        auto *lay = new QHBoxLayout(page);
        lay->setContentsMargins(0, 0, 0, 0);
        auto *combo = new QComboBox(page);
        combo->setObjectName("yitiaolongCombo");
        combo->addItems({QString::fromUtf8("平定安邦"), QString::fromUtf8("招财殿"),
            QString::fromUtf8("试炼场"), QString::fromUtf8("押镖"),
            QString::fromUtf8("悬赏"), QString::fromUtf8("副本")});
        lay->addWidget(combo);
        lay->addStretch();
        stack->addWidget(page);
    }

    connect(ui->taskCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::on_taskCombo_currentIndexChanged);
}

void MainWindow::on_taskCombo_currentIndexChanged(int index)
{
    ui->taskConfigStack->setCurrentIndex(index);
}

// ════════════════════════════════════════════════
// 启动/停止 —— 串行登录流程
// ════════════════════════════════════════════════
void MainWindow::on_startStopBtn_clicked()
{
    // \u505c\u6b62\u6240\u6709\uff08\u4efb\u52a1\u6216\u767b\u5f55\u4e2d\uff09
    if (m_currentLogin || !m_loginQueue.isEmpty()) {
        m_loginQueue.clear();
        m_currentLoginIdx = -1;
        if (m_currentLogin) {
            QMetaObject::invokeMethod(m_currentLogin, "cancel", Qt::QueuedConnection);
        }
        for (auto it = m_autoLogins.begin(); it != m_autoLogins.end(); ++it) {
            if (it.value()) { QMetaObject::invokeMethod(it.value(), "cancel", Qt::QueuedConnection); }
        }
        // 退出所有登录工作线程
        for (auto it = m_loginThreads.begin(); it != m_loginThreads.end(); ++it) {
            if (it.value()) { it.value()->quit(); it.value()->wait(3000); }
        }
        m_autoLogins.clear();
        m_loginThreads.clear();
        m_currentLogin = nullptr;
        m_loginSuccessCount = 0;
        for (auto it = m_activeTaskRows.begin(); it != m_activeTaskRows.end(); ++it) {
            if (mScheduler->slot(it.key())) mScheduler->slot(it.key())->setState(GameSlot::Idle);
        }
        m_activeTaskRows.clear();
        ui->statuslabel->setText(QString::fromUtf8("\u5df2\u505c\u6b62"));
        ui->textEdit->append(QString::fromUtf8("\u767b\u5f55\u5df2\u53d6\u6d88"));
        return;
    }

    // \u505c\u6b62\u5df2\u542f\u52a8\u7684\u670d\u52a1
    stopService();
    refreshTaskPanel();
}

void MainWindow::beginLoginSequence()
{
    m_loginQueue.clear();
    m_loginSuccessCount = 0;

    // 收集未登录的窗口
    for (int i = 0; i < 3; i++) {
        auto *slot = mScheduler->slot(i);
        if (slot && !slot->isLoggedIn() && slot->state() != GameSlot::Searching) {
            m_loginQueue.append(i);
        }
    }

    if (m_loginQueue.isEmpty()) {
        ui->statuslabel->setText(QString::fromUtf8("没有需要登录的窗口"));
        return;
    }

    loginNextSlot();
}

void MainWindow::loginNextSlot()
{
    if (m_loginQueue.isEmpty()) {
        onAllLoginDone();
        return;
    }

    m_currentLoginIdx = m_loginQueue.takeFirst();
    auto *slot = mScheduler->slot(m_currentLoginIdx);
    if (!slot) {
        loginNextSlot();
        return;
    }

    QSettings settings(QCoreApplication::applicationDirPath() + "/config.ini", QSettings::IniFormat);
    QString gamePath = settings.value("Accounts/GamePath").toString();

    // 创建 AutoLogin 实例 → 移到独立工作线程（避免 msleep 阻塞主线程事件循环）
    QThread *loginThread = new QThread(this);
    m_currentLogin = new AutoLogin(slot);  // 不能传 parent, moveToThread 要求无父对象
    m_currentLogin->moveToThread(loginThread);
    m_autoLogins.insert(m_currentLoginIdx, m_currentLogin);
    m_loginThreads.insert(m_currentLoginIdx, loginThread);

    // 线程清理（注意：不在 finished 时 quit，因为登录完成后还要 startPostInit）
    connect(loginThread, &QThread::finished, loginThread, &QObject::deleteLater);

    // 信号回主线程（Qt::AutoConnection 在跨线程时自动排队）
    connect(m_currentLogin, &AutoLogin::finished, this, &MainWindow::onLoginFinished);
    connect(m_currentLogin, &AutoLogin::postInitDone, this, &MainWindow::onPostInitDone);
    connect(m_currentLogin, &AutoLogin::captchaRequired, this, &MainWindow::onCaptchaRequired);
    connect(m_currentLogin, &AutoLogin::statusMessage, this, [this](const QString &msg) {
        ui->statuslabel->setText(msg);
        ui->textEdit->append(msg);
    });

    slot->setState(GameSlot::Searching);
    addActiveTask(m_currentLoginIdx, QString::fromUtf8("登录中"));
    ui->statuslabel->setText(QString::fromUtf8("窗口%1 登录中...").arg(m_currentLoginIdx + 1));

    loginThread->start();
    // 用 invokeMethod 确保 start() 在登录线程执行（timer 在正确的线程里创建/启动）
    QMetaObject::invokeMethod(m_currentLogin, "start", Qt::QueuedConnection, Q_ARG(QString, gamePath));
}

void MainWindow::onLoginFinished(bool success)
{
    int idx = m_currentLoginIdx;
    auto *slot = (idx >= 0) ? mScheduler->slot(idx) : nullptr;

    if (slot) {
        if (success) {
            m_loginSuccessCount++;
            slot->setLoggedIn(true);
            slot->setState(GameSlot::Running);
            removeActiveTask(idx);
            addActiveTask(idx, slot->taskName());
            if (m_statusLabels[idx]) { m_statusLabels[idx]->setText(QString::fromUtf8("\u5df2\u767b\u5f55")); m_statusLabels[idx]->setStyleSheet("color: #090;"); }
            refreshTaskPanel();
            ui->textEdit->append(QString::fromUtf8("窗口%1 登录成功，开始初始化...").arg(idx + 1));
            QMetaObject::invokeMethod(m_currentLogin, "startPostInit", Qt::QueuedConnection);
            return;
        } else {
            slot->setState(GameSlot::Idle);
            removeActiveTask(idx);
            ui->textEdit->append(QString::fromUtf8("窗口%1 登录失败").arg(idx + 1));
        }
    } else {
        ui->textEdit->append(QString::fromUtf8("登录已取消"));
    }

    // 清理登录线程
    if (m_loginThreads.contains(idx)) {
        QThread *t = m_loginThreads.take(idx);
        t->quit(); t->wait(3000);
    }
    m_autoLogins.remove(idx);
    m_currentLogin = nullptr;
    m_currentLoginIdx = -1;
    loginNextSlot();
}

void MainWindow::onPostInitDone(int slotIndex)
{
    ui->textEdit->append(QString::fromUtf8("窗口%1 初始化完成，继续下一个").arg(slotIndex + 1));
    // 清理登录线程
    if (m_loginThreads.contains(slotIndex)) {
        QThread *t = m_loginThreads.take(slotIndex);
        t->quit(); t->wait(3000);
    }
    m_autoLogins.remove(slotIndex);
    m_currentLogin = nullptr;
    m_currentLoginIdx = -1;
    loginNextSlot();
}


void MainWindow::onCaptchaRequired(int slotIndex)
{
    if (m_captchaBtn) {
        m_captchaBtn->setText(QString::fromUtf8("窗口%1 验证码已填 ▶").arg(slotIndex + 1));
        m_captchaBtn->show();
    }
    ui->statuslabel->setText(QString::fromUtf8("窗口%1 请输入验证码").arg(slotIndex + 1));
}
void MainWindow::onAllLoginDone()
{
    if (m_loginSuccessCount == 0) {
        ui->statuslabel->setText(QString::fromUtf8("所有窗口登录失败"));
        return;
    }

    ui->statuslabel->setText(QString::fromUtf8("登录完成，启动任务调度..."));
    ui->textEdit->append(QString::fromUtf8("%1/%2 窗口登录成功，开始任务").arg(m_loginSuccessCount).arg(m_loginSuccessCount + (3 - m_loginQueue.size() - m_loginSuccessCount)));

    // 启动 SlotScheduler 轮询调度
    refreshTaskPanel();
}


// =============================================
// 账号+任务面板UI（布局化）
// =============================================
void MainWindow::setupAccountTaskUI()
{
    auto *rootLayout = qobject_cast<QVBoxLayout*>(centralWidget()->layout());

    // ── 1. 工具栏（独立于 configArea，插入 rootLayout[0]）──
    auto *toolbar = new QWidget();
    toolbar->setFixedHeight(28);
    auto *tbLayout = new QHBoxLayout(toolbar);
    tbLayout->setContentsMargins(4, 0, 4, 0);
    tbLayout->setSpacing(4);

    tbLayout->addWidget(ui->statuslabel);

    ui->screenShareButton->show(); // 始终显示
    tbLayout->addWidget(ui->screenShareButton);
    connect(ui->screenShareButton, &QPushButton::clicked,
            this, &MainWindow::on_screenShareButton_clicked);

    tbLayout->addWidget(ui->itemCaptureButton);
    connect(ui->itemCaptureButton, &QPushButton::clicked,
            this, &MainWindow::openItemCapture);

    // 验证码按钮（默认隐藏，创建角色时显示）
    m_captchaBtn = new QPushButton(QString::fromUtf8("验证码已填"));
    m_captchaBtn->setFixedHeight(22);
    m_captchaBtn->setStyleSheet("QPushButton { background-color: #e67e22; color: #fff; font-weight: bold; border-radius: 3px; padding: 0 10px; } QPushButton:hover { background-color: #d35400; }");
    m_captchaBtn->hide();
    tbLayout->addWidget(m_captchaBtn);
    connect(m_captchaBtn, &QPushButton::clicked, this, [this]() {
        if (m_currentLogin) {
            QMetaObject::invokeMethod(m_currentLogin, "onCaptchaDone", Qt::QueuedConnection);
            m_captchaBtn->hide();
        }
    });

    tbLayout->addStretch();

    auto *sep1 = new QFrame();
    sep1->setFrameShape(QFrame::VLine);
    sep1->setStyleSheet("color: #555;");
    tbLayout->addWidget(sep1);

    tbLayout->addWidget(ui->ocrEngineLabel);
    tbLayout->addWidget(ui->ocrEngineCombo);

    tbLayout->addSpacing(6);
    tbLayout->addWidget(ui->startStopBtn);

    rootLayout->insertWidget(0, toolbar);

    // ── 2. 模式切换栏（插入 rootLayout[1]）──
    auto *modeBar = new QWidget();
    modeBar->setFixedHeight(26);
    auto *modeLayout = new QHBoxLayout(modeBar);
    modeLayout->setContentsMargins(4, 0, 4, 0);
    modeLayout->setSpacing(0);

    m_accountModeBtn = new QPushButton(QString::fromUtf8("\u8d26\u53f7"), modeBar);
    m_accountModeBtn->setFixedSize(44, 22);
    m_accountModeBtn->setCheckable(true);
    m_accountModeBtn->setChecked(true);
    m_accountModeBtn->setStyleSheet(QString::fromUtf8(
        "QPushButton { border:1px solid #555; background:#3a3a3a; color:#ccc; font-size:11px; }"
        "QPushButton:checked { background:#1a6fb5; color:white; border-color:#1a6fb5; }"));
    modeLayout->addWidget(m_accountModeBtn);

    m_taskModeBtn = new QPushButton(QString::fromUtf8("\u4efb\u52a1"), modeBar);
    m_taskModeBtn->setFixedSize(44, 22);
    m_taskModeBtn->setCheckable(true);
    m_taskModeBtn->setStyleSheet(QString::fromUtf8(
        "QPushButton { border:1px solid #555; background:#3a3a3a; color:#ccc; font-size:11px; }"
        "QPushButton:checked { background:#1a6fb5; color:white; border-color:#1a6fb5; }"));
    modeLayout->addWidget(m_taskModeBtn);
    modeLayout->addStretch();
    rootLayout->insertWidget(1, modeBar);

    // 隐藏 .ui 旧控件（不再需要）
    ui->taskLabel->hide();
    ui->taskCombo->hide();
    ui->taskConfigStack->hide();

    // ── 3. configArea → QStackedWidget 双页 ──
    auto *ca = ui->configArea;
    delete ca->layout();
    m_modeStack = new QStackedWidget();
    m_modeStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto *caLayout = new QVBoxLayout(ca);
    caLayout->setContentsMargins(4, 0, 4, 0);
    caLayout->addWidget(m_modeStack);

    // 页面 0: 账号
    auto *acctPage = new QWidget();
    auto *acctPageLayout = new QVBoxLayout(acctPage);
    acctPageLayout->setContentsMargins(0, 0, 0, 0);
    acctPageLayout->setSpacing(8);

    // 替换 QGroupBox，用轻量 Header + 行布局，避免边框造成的拥挤感
    auto *acctHeader = new QLabel(QString::fromUtf8("\u8d26\u53f7"), acctPage);
    acctHeader->setStyleSheet("font-weight:bold; font-size:13px; color:#ddd; padding:2px 0;");
    acctPageLayout->addWidget(acctHeader);

    auto *acctRowsWidget = new QWidget(acctPage);
    auto *acctLayout = new QVBoxLayout(acctRowsWidget);
    acctLayout->setContentsMargins(0, 4, 0, 4);
    acctLayout->setSpacing(6);

    for (int i = 0; i < 3; i++) {
        auto *row = new QWidget(acctRowsWidget);
        row->setMinimumHeight(28);
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(4);

        auto *nameLabel = new QLabel(QString("\u7a97\u53e3%1").arg(i + 1), row);
        nameLabel->setFixedWidth(38);
        rowLayout->addWidget(nameLabel);

        m_statusLabels[i] = new QLabel(QString::fromUtf8("\u672a\u767b\u5f55"), row);
        m_statusLabels[i]->setStyleSheet(QString::fromUtf8("color: #888;"));
        m_statusLabels[i]->setFixedWidth(48);
        rowLayout->addWidget(m_statusLabels[i]);

        m_accountCombos[i] = new QComboBox(row);
        m_accountCombos[i]->setMinimumWidth(100);
        m_accountCombos[i]->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        int idx = i;
        connect(m_accountCombos[i], QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this, idx]() { on_accountComboChanged(idx); });
        rowLayout->addWidget(m_accountCombos[i]);

        m_loginBtns[i] = new QPushButton(QString::fromUtf8("\u767b\u5f55"), row);
        m_loginBtns[i]->setFixedSize(44, 22);
        connect(m_loginBtns[i], &QPushButton::clicked, this, [this, idx]() {
            on_singleLoginBtn_clicked(idx);
        });
        rowLayout->addWidget(m_loginBtns[i]);

        rowLayout->addStretch();
        acctLayout->addWidget(row);
    }
    acctPageLayout->addWidget(acctRowsWidget);

    auto *acctBtn = new QPushButton(QString::fromUtf8("\u8d26\u53f7\u7ba1\u7406"), acctPage);
    acctBtn->setFixedHeight(26);
    acctBtn->setStyleSheet(QString::fromUtf8(
        "QPushButton { background:#4CAF50; color:white; font-weight:bold; border-radius:2px; }"
        "QPushButton:hover { background:#66BB6A; }"));
    connect(acctBtn, &QPushButton::clicked, this, [this]() {
        AccountDialog dlg(this);
        connect(&dlg, &AccountDialog::accountsChanged,
                this, &MainWindow::loadAccountCombo);
        dlg.exec();
    });
    acctPageLayout->addWidget(acctBtn);
    acctPageLayout->addStretch();
    m_modeStack->addWidget(acctPage); // index 0

    // 页面 1: 任务
    buildTaskPanel();
    m_modeStack->addWidget(m_taskPanel); // index 1

    // 模式切换逻辑
    connect(m_accountModeBtn, &QPushButton::clicked, this, [this]() {
        m_modeStack->setCurrentIndex(0);
        m_accountModeBtn->setChecked(true);
        m_taskModeBtn->setChecked(false);
    });
    connect(m_taskModeBtn, &QPushButton::clicked, this, [this]() {
        m_modeStack->setCurrentIndex(1);
        m_accountModeBtn->setChecked(false);
        m_taskModeBtn->setChecked(true);
    });

    // ── 4. 活动任务栏（插入 rootLayout 在 textEdit 之上）──
    m_activeTaskWidget = new QWidget();
    m_activeTaskWidget->setMinimumHeight(0);
    m_activeTaskWidget->hide();  // 默认隐藏，有任务时再显示
    m_activeTaskLayout = new QVBoxLayout(m_activeTaskWidget);
    m_activeTaskLayout->setContentsMargins(4, 1, 4, 1);
    m_activeTaskLayout->setSpacing(1);

    // rootLayout 当前: [0]toolbar [1]modeBar [2]configArea [3]textEdit
    // 插入活动任务到 textEdit 之前
    int textEditIdx = rootLayout->indexOf(ui->textEdit);
    if (textEditIdx >= 0)
        rootLayout->insertWidget(textEditIdx, m_activeTaskWidget);
}

// =============================================
// 任务面板（创建一次，登录后 show）
// =============================================
void MainWindow::buildTaskPanel()
{
    m_taskPanel = new QGroupBox(QString::fromUtf8("\u4efb\u52a1"));
    m_taskPanelLayout = new QVBoxLayout(m_taskPanel);
    m_taskPanelLayout->setContentsMargins(6, 6, 6, 6);
    m_taskPanelLayout->setSpacing(2);

    // 顶部统一配置栏
    auto *taskBar = new QWidget(m_taskPanel);
    auto *taskBarLayout = new QHBoxLayout(taskBar);
    taskBarLayout->setContentsMargins(0, 0, 0, 0);
    taskBarLayout->setSpacing(4);

    taskBarLayout->addWidget(new QLabel(QString::fromUtf8("\u7edf\u4e00\u8bbe\u4e3a:"), taskBar));
    m_unifiedTaskCombo = new QComboBox(taskBar);
    m_unifiedTaskCombo->addItems({QString::fromUtf8("\u526f\u672c"), QString::fromUtf8("\u4e3b\u7ebf\u4efb\u52a1"),
                                  QString::fromUtf8("\u5192\u9669"), QString::fromUtf8("\u4e00\u6761\u9f99")});
    m_unifiedTaskCombo->setFixedWidth(80);
    connect(m_unifiedTaskCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::on_unifiedTaskChanged);
    taskBarLayout->addWidget(m_unifiedTaskCombo);

    taskBarLayout->addSpacing(4);
    taskBarLayout->addWidget(new QLabel(QString::fromUtf8("\u6b21\u6570:"), taskBar));
    m_dungeonSpin = new QSpinBox(taskBar);
    m_dungeonSpin->setRange(1, 99);
    m_dungeonSpin->setFixedWidth(44);
    taskBarLayout->addWidget(m_dungeonSpin);

    m_taskDungeonCombo = new QComboBox(taskBar);
    m_taskDungeonCombo->addItems({QString::fromUtf8("\u6076\u4eba\u8c37"), QString::fromUtf8("\u5c60\u72fc\u6d1e"),
                                  QString::fromUtf8("\u51e4\u9e23\u5c71"), QString::fromUtf8("\u51b0\u98ce\u6d1e"),
                                  QString::fromUtf8("\u957f\u5bff\u5bab"), QString::fromUtf8("\u591c\u54ed\u5c71\u5e84")});
    m_taskDungeonCombo->setFixedWidth(72);
    taskBarLayout->addWidget(m_taskDungeonCombo);

    taskBarLayout->addStretch();
    m_startAllBtn = new QPushButton(QString::fromUtf8("\u5168\u90e8\u542f\u52a8"), taskBar);
    m_startAllBtn->setFixedSize(72, 24);
    connect(m_startAllBtn, &QPushButton::clicked, this, &MainWindow::on_startAllBtn_clicked);
    taskBarLayout->addWidget(m_startAllBtn);

    m_taskPanelLayout->addWidget(taskBar);

    // 分隔线
    auto *sep = new QFrame(m_taskPanel);
    sep->setFrameShape(QFrame::HLine);
    sep->setFrameShadow(QFrame::Sunken);
    m_taskPanelLayout->addWidget(sep);

    // 每个窗口的任务行（动态创建）
    m_taskRowsWidget = new QWidget(m_taskPanel);
    m_taskRowsLayout = new QVBoxLayout(m_taskRowsWidget);
    m_taskRowsLayout->setContentsMargins(0, 0, 0, 0);
    m_taskRowsLayout->setSpacing(2);
    m_taskPanelLayout->addWidget(m_taskRowsWidget);
}

// =============================================
// 刷新任务面板
// =============================================
void MainWindow::refreshTaskPanel()
{
    // 清除旧行
    QLayoutItem *child;
    while ((child = m_taskRowsLayout->takeAt(0)) != nullptr) {
        if (child->widget()) child->widget()->deleteLater();
        delete child;
    }
    for (int i = 0; i < 3; i++) {
        m_accountTaskCombos[i] = nullptr;
        m_accountStartBtns[i] = nullptr;
    }

    // 检查是否有已登录账号
    bool anyLoggedIn = false;
    for (int i = 0; i < 3; i++) {
        auto *slot = mScheduler->slot(i);
        if (slot && slot->isLoggedIn()) {
            anyLoggedIn = true;
            break;
        }
    }

    if (!anyLoggedIn) {
        m_taskPanel->hide();
        return;
    }

    // 为每个已登录账号创建任务行
    for (int i = 0; i < 3; i++) {
        auto *slot = mScheduler->slot(i);
        if (!slot || !slot->isLoggedIn()) continue;

        auto *row = new QWidget(m_taskRowsWidget);
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(4);

        auto *nameLabel = new QLabel(QString("\u7a97\u53e3%1").arg(i + 1), row);
        nameLabel->setFixedWidth(42);
        rowLayout->addWidget(nameLabel);

        m_accountTaskCombos[i] = new QComboBox(row);
        m_accountTaskCombos[i]->addItems({QString::fromUtf8("\u526f\u672c"), QString::fromUtf8("\u4e3b\u7ebf\u4efb\u52a1"),
                                          QString::fromUtf8("\u5192\u9669"), QString::fromUtf8("\u4e00\u6761\u9f99")});
        m_accountTaskCombos[i]->setCurrentIndex(slot->taskType());
        m_accountTaskCombos[i]->setFixedWidth(80);
        rowLayout->addWidget(m_accountTaskCombos[i]);

        if (slot->state() == GameSlot::Running && slot->service()) {
            // 已在运行中
            auto *runningLabel = new QLabel(QString::fromUtf8("\u25b6 \u8fd0\u884c\u4e2d"), row);
            runningLabel->setStyleSheet("color: #090;");
            rowLayout->addWidget(runningLabel);
        } else {
            m_accountStartBtns[i] = new QPushButton(QString::fromUtf8("\u25b6"), row);
            m_accountStartBtns[i]->setFixedSize(28, 24);
            m_accountStartBtns[i]->setToolTip(QString("\u542f\u52a8\u7a97\u53e3%1").arg(i + 1));
            int idx = i;
            connect(m_accountStartBtns[i], &QPushButton::clicked, this, [this, idx]() {
                on_accountStartBtn_clicked(idx);
            });
            rowLayout->addWidget(m_accountStartBtns[i]);
        }

        rowLayout->addStretch();
        m_taskRowsLayout->addWidget(row);
    }

    m_taskPanel->show();
}

// =============================================
// 加载账号配置
// =============================================
void MainWindow::loadAccountCombo()
{
    QSettings settings(QCoreApplication::applicationDirPath() + "/config.ini", QSettings::IniFormat);

    // 诊断：输出 allKeys 看 QSettings 实际解析结果
    infof("loadAccountCombo allKeys count={}", settings.allKeys().size());
    for (const QString& k : settings.allKeys()) {
        infof("  key='{}' value='{}'", k.toStdString(), settings.value(k).toString().toStdString());
    }

    // 收集所有账号供下拉框
    // config.ini 用 Slot0/Account 扁平 key 格式（非嵌套 section）
    QStringList accountNames;
    accountNames << QString::fromUtf8("\u672a\u914d\u7f6e");
    for (int i = 0; i < 3; i++) {
        QString sk = QString("Slot%1").arg(i);
        QString name = settings.value("Accounts/" + sk + "/CharName").toString();
        QString account = settings.value("Accounts/" + sk + "/Account").toString();
        // CharName 优先，否则用 Account 作为显示名
        QString display = name.isEmpty() ? account : name;
        if (!display.isEmpty())
            accountNames << display;
    }

    for (int i = 0; i < 3; i++) {
        QString sk = QString("Slot%1").arg(i);
        QString name = settings.value("Accounts/" + sk + "/CharName").toString();
        QString account = settings.value("Accounts/" + sk + "/Account").toString();
        QString display = name.isEmpty() ? account : name;

        auto *slot = mScheduler->slot(i);
        slot->setCharName(name);
        slot->setCharFaction(settings.value("Accounts/" + sk + "/CharFaction").toString());
        slot->setAccount(
            account,
            settings.value("Accounts/" + sk + "/Password").toString());

        // 更新账号下拉框
        if (m_accountCombos[i]) {
            m_accountCombos[i]->blockSignals(true);
            m_accountCombos[i]->clear();
            m_accountCombos[i]->addItems(accountNames);
            int selIdx = accountNames.indexOf(display);
            if (selIdx >= 0) m_accountCombos[i]->setCurrentIndex(selIdx);
            m_accountCombos[i]->blockSignals(false);
        }

        // 更新状态标签
        if (m_statusLabels[i]) {
            bool loggedIn = slot->isLoggedIn();
            if (loggedIn) {
                m_statusLabels[i]->setText(QString::fromUtf8("\u5df2\u767b\u5f55"));
                m_statusLabels[i]->setStyleSheet("color: #090;");
            } else {
                m_statusLabels[i]->setText(QString::fromUtf8("\u672a\u767b\u5f55"));
                m_statusLabels[i]->setStyleSheet("color: #888;");
            }
        }
    }
}

// =============================================
// 账号下拉框切换
// =============================================
void MainWindow::on_accountComboChanged(int idx)
{
    if (!m_accountCombos[idx]) return;
    QString selected = m_accountCombos[idx]->currentText();
    if (selected == QString::fromUtf8("\u672a\u914d\u7f6e")) return;

    // 查找实际 slot 配置
    QSettings settings(QCoreApplication::applicationDirPath() + "/config.ini", QSettings::IniFormat);
    for (int i = 0; i < 3; i++) {
        QString sk = QString("Slot%1").arg(i);
        if (settings.value("Accounts/" + sk + "/CharName").toString() == selected ||
            settings.value("Accounts/" + sk + "/Account").toString() == selected) {
            auto *slot = mScheduler->slot(idx);
            slot->setAccount(
                settings.value("Accounts/" + sk + "/Account").toString(),
                settings.value("Accounts/" + sk + "/Password").toString());
            slot->setCharName(settings.value("Accounts/" + sk + "/CharName").toString());
            slot->setCharFaction(settings.value("Accounts/" + sk + "/CharFaction").toString());
            break;
        }
    }
}

// =============================================
// 单个账号登录
// =============================================
void MainWindow::on_singleLoginBtn_clicked(int idx)
{
    infof("[MainWindow] 单击登录按钮 idx=%d", idx);
    auto *slot = mScheduler->slot(idx);
    if (!slot) { infof("[MainWindow] slot=%d 不存在", idx); return; }
    if (slot->isLoggedIn()) { infof("[MainWindow] slot=%d 已登录，跳过", idx); return; }

    QSettings settings(QCoreApplication::applicationDirPath() + "/config.ini", QSettings::IniFormat);
    QString gamePath = settings.value("Accounts/GamePath").toString();
    if (gamePath.isEmpty()) {
        QMessageBox::warning(this, QString::fromUtf8("\u63d0\u793a"),
            QString::fromUtf8("\u8bf7\u5148\u5728\u8d26\u53f7\u7ba1\u7406\u4e2d\u8bbe\u7f6e\u6e38\u620f\u8def\u5f84"));
        return;
    }
    QString sk = QString("Slot%1").arg(idx);
    if (settings.value("Accounts/" + sk + "/Account").toString().isEmpty()) {
        QMessageBox::warning(this, QString::fromUtf8("\u63d0\u793a"),
            QString::fromUtf8("\u8bf7\u5148\u5728\u8d26\u53f7\u7ba1\u7406\u4e2d\u914d\u7f6e\u7a97\u53e3%1\u7684\u8d26\u53f7\u5bc6\u7801").arg(idx + 1));
        return;
    }

    // 加入登录队列
    if (!m_loginQueue.contains(idx)) {
        m_loginQueue.append(idx);
    }

    // 如果当前没有正在登录的，立即开始
    if (!m_currentLogin && m_loginQueue.size() == 1) {
        loginNextSlot();
    } else {
        ui->statuslabel->setText(QString::fromUtf8("\u6392\u961f\u4e2d..."));
    }
}

// =============================================
// 批量登录按钮
// =============================================
void MainWindow::on_loginBtn_clicked()
{
    QSettings settings(QCoreApplication::applicationDirPath() + "/config.ini", QSettings::IniFormat);
    QString gamePath = settings.value("Accounts/GamePath").toString();
    if (gamePath.isEmpty()) {
        QMessageBox::warning(this, QString::fromUtf8("\u63d0\u793a"),
            QString::fromUtf8("\u8bf7\u5148\u5728\u8d26\u53f7\u7ba1\u7406\u4e2d\u8bbe\u7f6e\u6e38\u620f\u8def\u5f84"));
        return;
    }

    int queued = 0;
    for (int i = 0; i < 3; i++) {
        auto *slot = mScheduler->slot(i);
        if (!slot || slot->isLoggedIn()) continue;
        QString ski = QString("Slot%1").arg(i);
        if (settings.value("Accounts/" + ski + "/Account").toString().isEmpty())
            continue;
        if (!m_loginQueue.contains(i)) {
            m_loginQueue.append(i);
            queued++;
        }
    }
    if (queued == 0) {
        QMessageBox::information(this, QString::fromUtf8("\u63d0\u793a"),
            QString::fromUtf8("\u6ca1\u6709\u9700\u8981\u767b\u5f55\u7684\u8d26\u53f7"));
        return;
    }
    if (!m_currentLogin) loginNextSlot();
}

// =============================================
// 统一任务下拉改变
// =============================================
void MainWindow::on_unifiedTaskChanged(int index)
{
    for (int i = 0; i < 3; i++) {
        if (m_accountTaskCombos[i]) {
            m_accountTaskCombos[i]->setCurrentIndex(index);
        }
    }
}

// =============================================
// 全部启动
// =============================================
void MainWindow::on_startAllBtn_clicked()
{
    for (int i = 0; i < 3; i++) {
        auto *slot = mScheduler->slot(i);
        if (slot && slot->isLoggedIn() && m_accountTaskCombos[i]) {
            startSingleTask(i);
        }
    }
}

// =============================================
// 单个启动
// =============================================
void MainWindow::on_accountStartBtn_clicked(int idx)
{
    auto *slot = mScheduler->slot(idx);
    if (!slot || !slot->isLoggedIn()) return;
    startSingleTask(idx);
}

// =============================================
// 启动单个任务
// =============================================
void MainWindow::startSingleTask(int slotIdx)
{
    auto *slot = mScheduler->slot(slotIdx);
    if (!slot || !slot->isLoggedIn()) return;

    GameSlot::TaskType taskType = (m_accountTaskCombos[slotIdx])
        ? static_cast<GameSlot::TaskType>(m_accountTaskCombos[slotIdx]->currentIndex())
        : GameSlot::Dungeon;

    QString param;
    if (taskType == GameSlot::Dungeon && m_taskDungeonCombo && m_dungeonSpin) {
        param = m_taskDungeonCombo->currentText() + " x" + QString::number(m_dungeonSpin->value());
    }
    slot->setTask(taskType, param);

    BaseService *svc = nullptr;
    switch (taskType) {
    case GameSlot::Dungeon:
        svc = new ClientDungeonService();
        break;
    case GameSlot::MainQuest:
        svc = new MainQuestService();
        break;
    default:
        break;
    }
    if (svc) {
        slot->setService(svc);
        svc->start();
    }

    addActiveTask(slotIdx, slot->taskName());

    if (!mScheduler->running()) {
        mScheduler->start();
    }

    ui->statuslabel->setText(QString::fromUtf8("\u8fd0\u884c\u4e2d"));
    ui->textEdit->append(QString::fromUtf8("\u7a97\u53e3%1 \u5df2\u542f\u52a8: %2").arg(slotIdx + 1).arg(slot->taskName()));

    refreshTaskPanel();
}



void MainWindow::addActiveTask(int slotIdx, const QString &taskName)
{
    if (m_activeTaskRows.contains(slotIdx)) return;

    auto *row = new QWidget();
    auto *lay = new QHBoxLayout(row);
    lay->setContentsMargins(4, 1, 4, 1);
    lay->setSpacing(6);

    QString name = QString("\u7a97\u53e3%1").arg(slotIdx + 1);
    auto *label = new QLabel(QString("%1  %2").arg(name, taskName));
    lay->addWidget(label);
    lay->addStretch();

    auto *stopBtn = new QPushButton(QString::fromUtf8("\u505c\u6b62"));
    stopBtn->setFixedSize(40, 22);
    int idx = slotIdx;
    connect(stopBtn, &QPushButton::clicked, this, [this, idx]() {
        auto *slot = mScheduler->slot(idx);
        if (slot) {
            slot->stopService();
            slot->setState(GameSlot::Idle);
        }
        removeActiveTask(idx);
    });
    lay->addWidget(stopBtn);

    m_activeTaskLayout->addWidget(row);
    m_activeTaskRows.insert(slotIdx, row);
    m_activeTaskWidget->setMinimumHeight(80);
    m_activeTaskWidget->show();
}

void MainWindow::removeActiveTask(int slotIdx)
{
    auto it = m_activeTaskRows.find(slotIdx);
    if (it != m_activeTaskRows.end()) {
        it.value()->deleteLater();
        m_activeTaskRows.erase(it);
    }
    if (m_activeTaskRows.isEmpty()) {
        m_activeTaskWidget->setMinimumHeight(0);
        m_activeTaskWidget->hide();
    }
}

void MainWindow::stopService()
{
    for (int i = 0; i < 3; i++) {
        auto *slot = mScheduler->slot(i);
        if (slot) {
            slot->stopService();
            slot->setState(GameSlot::Idle);
        }
    }
    mScheduler->stop();
    ui->statuslabel->setText(QString::fromUtf8("\u5c31\u7eea"));

    // 重置所有登录状态
    for (int i = 0; i < 3; i++) {
        auto *s = mScheduler->slot(i);
        if (s) { s->setLoggedIn(false); }
    }
    for (auto it = m_activeTaskRows.begin(); it != m_activeTaskRows.end(); ++it) {
        it.value()->deleteLater();
    }
    m_activeTaskRows.clear();
    m_loginSuccessCount = 0;
    refreshTaskPanel();
}

void MainWindow::openItemCapture()
{
    if (!itemCaptureUi) {
        itemCaptureUi = new GameItemCaptureWidget();
    }
    itemCaptureUi->show();
    itemCaptureUi->raise();
}

void MainWindow::receiveLog(const QString &str)
{
    ui->textEdit->append(str);
}

