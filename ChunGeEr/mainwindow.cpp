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

#include <QSettings>
#include <QDebug>
#include <QLabel>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QTimer>
#include <QPixmap>
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
    qRegisterMetaType<ScreenCaptureManager::ScreenData>("ScreenData");

    QSettings settings("config.ini", QSettings::IniFormat);
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
    }

    ui->ocrEngineCombo->setCurrentIndex(1); // Tesseract

    // ── 配置区：绝对定位在 configArea 内部 ──
    auto *ca = ui->configArea;

    ui->statuslabel->setParent(ca);
    ui->statuslabel->setGeometry(2, 2, 64, 24);

    ui->screenShareButton->setParent(ca);
    ui->screenShareButton->setGeometry(70, 2, 84, 24);
    connect(ui->screenShareButton, &QPushButton::clicked, this, &MainWindow::on_screenShareButton_clicked);

    ui->itemCaptureButton->setParent(ca);
    ui->itemCaptureButton->setGeometry(158, 2, 84, 24);
    connect(ui->itemCaptureButton, &QPushButton::clicked, this, &MainWindow::openItemCapture);

    ui->ocrEngineLabel->setParent(ca);
    ui->ocrEngineLabel->setGeometry(250, 2, 32, 24);

    ui->ocrEngineCombo->setParent(ca);
    ui->ocrEngineCombo->setGeometry(284, 2, 96, 24);

    ui->taskLabel->setParent(ca);
    ui->taskLabel->setGeometry(2, 30, 36, 24);

    ui->taskCombo->setParent(ca);
    ui->taskCombo->setGeometry(40, 30, 90, 24);

    ui->taskConfigStack->setParent(ca);
    ui->taskConfigStack->setGeometry(134, 30, 380, 24);

    ui->startStopBtn->setParent(ca);
    ui->startStopBtn->setGeometry(664, 30, 50, 24);
    // connectSlotsByName 已自动连接 on_startStopBtn_clicked，不需要手动 connect

    // 外层 layout 比例 1:3
    auto *rootLayout = static_cast<QVBoxLayout *>(centralWidget()->layout());
    if (rootLayout) {
        rootLayout->setStretch(0, 1);
        rootLayout->setStretch(1, 3);
    }

    setupTaskConfigs();
    setupAccountUI();
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
            svc->startService();
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
    // ── 如果正在登录，取消 ──
    if (m_currentLogin || !m_loginQueue.isEmpty()) {
        // 先清队列，再 cancel（cancel会触发onLoginFinished→loginNextSlot）
        m_loginQueue.clear();
        m_currentLoginIdx = -1;
        if (m_currentLogin) {
            m_currentLogin->cancel();
        }
        // 清所有 autoLogin（除了已被cancel清理的）
        for (auto it = m_autoLogins.begin(); it != m_autoLogins.end(); ++it) {
            if (it.value()) {
                it.value()->cancel();
                it.value()->deleteLater();
            }
        }
        m_autoLogins.clear();
        m_currentLogin = nullptr;
        m_loginSuccessCount = 0;

        // 清理所有活跃任务行
        for (auto it = m_activeTaskRows.begin(); it != m_activeTaskRows.end(); ++it) {
            int idx = it.key();
            if (mScheduler->slot(idx))
                mScheduler->slot(idx)->setState(GameSlot::Idle);
        }
        m_activeTaskRows.clear();

        ui->statuslabel->setText(QString::fromUtf8("已停止"));
        ui->textEdit->append(QString::fromUtf8("登录已取消"));
        return;
    }

    // 检查是否选了窗口
    bool anyChecked = false;
    for (int i = 0; i < 3; i++) {
        if (m_slotChecks[i] && m_slotChecks[i]->isChecked()) { anyChecked = true; break; }
    }
    if (!anyChecked) {
        QMessageBox::warning(this, QString::fromUtf8("提示"),
            QString::fromUtf8("请先勾选要启动的窗口"));
        return;
    }

    QSettings settings("config.ini", QSettings::IniFormat);
    QString gamePath = settings.value("Accounts/GamePath").toString();
    if (gamePath.isEmpty()) {
        QMessageBox::warning(this, QString::fromUtf8("提示"),
            QString::fromUtf8("请先在账号管理中设置游戏路径"));
        return;
    }

    // 清理残留
    for (int i = 0; i < 3; i++) {
        auto *slot = mScheduler->slot(i);
        if (slot && slot->state() == GameSlot::Idle && m_activeTaskRows.contains(i)) {
            removeActiveTask(i);
        }
    }

    // 记录任务配置（登录完成后使用）
    GameSlot::TaskType taskType = static_cast<GameSlot::TaskType>(ui->taskCombo->currentIndex());
    QString param;
    if (taskType == GameSlot::Dungeon) {
        auto *page = ui->taskConfigStack->widget(0);
        auto *combo = page->findChild<QComboBox *>("dungeonCombo");
        auto *spin = page->findChild<QSpinBox *>("dungeonCount");
        if (combo) param = combo->currentText() + " x" + QString::number(spin ? spin->value() : 1);
    }
    for (int i = 0; i < 3; i++) {
        if (!m_slotChecks[i] || !m_slotChecks[i]->isChecked()) continue;
        auto *slot = mScheduler->slot(i);
        if (slot) slot->setTask(taskType, param);
    }

    ui->statuslabel->setText(QString::fromUtf8("开始登录..."));
    beginLoginSequence();
}

void MainWindow::beginLoginSequence()
{
    m_loginQueue.clear();
    m_loginSuccessCount = 0;

    // 收集勾选的窗口，按顺序排队
    for (int i = 0; i < 3; i++) {
        if (m_slotChecks[i] && m_slotChecks[i]->isChecked()) {
            auto *slot = mScheduler->slot(i);
            if (slot && slot->state() != GameSlot::Running && slot->state() != GameSlot::Searching) {
                m_loginQueue.append(i);
            }
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

    QSettings settings("config.ini", QSettings::IniFormat);
    QString gamePath = settings.value("Accounts/GamePath").toString();

    // 创建 AutoLogin 实例
    m_currentLogin = new AutoLogin(slot, this);
    m_autoLogins.insert(m_currentLoginIdx, m_currentLogin);

    connect(m_currentLogin, &AutoLogin::finished, this, &MainWindow::onLoginFinished);
    connect(m_currentLogin, &AutoLogin::statusMessage, this, [this](const QString &msg) {
        ui->statuslabel->setText(msg);
        ui->textEdit->append(msg);
    });

    slot->setState(GameSlot::Searching);
    addActiveTask(m_currentLoginIdx, QString::fromUtf8("登录中"));
    ui->statuslabel->setText(QString::fromUtf8("窗口%1 登录中...").arg(m_currentLoginIdx + 1));

    m_currentLogin->start(gamePath);
}

void MainWindow::onLoginFinished(bool success)
{
    int idx = m_currentLoginIdx;
    auto *slot = (idx >= 0) ? mScheduler->slot(idx) : nullptr;

    if (slot) {
        if (success) {
            m_loginSuccessCount++;
            slot->setState(GameSlot::Running);
            removeActiveTask(idx);
            addActiveTask(idx, slot->taskName());
            ui->textEdit->append(QString::fromUtf8("窗口%1 登录成功").arg(idx + 1));
        } else {
            slot->setState(GameSlot::Idle);
            removeActiveTask(idx);
            ui->textEdit->append(QString::fromUtf8("窗口%1 登录失败").arg(idx + 1));
        }
    } else {
        ui->textEdit->append(QString::fromUtf8("登录已取消"));
    }

    // 继续登录下一个
    m_currentLogin = nullptr;
    m_currentLoginIdx = -1;
    loginNextSlot();
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
    startService();
}

// ════════════════════════════════════════════════
// 账号UI - 多选框
// ════════════════════════════════════════════════
void MainWindow::setupAccountUI()
{
    auto *ca = ui->configArea;

    // 三个窗口复选框 + 齿轮（绝对定位在 configArea 内）
    for (int i = 0; i < 3; i++) {
        m_slotChecks[i] = new QCheckBox(QString("窗口%1").arg(QString::number(i + 1)), ca);
        m_slotChecks[i]->setGeometry(460 + i * 54, 30, 52, 24);
        m_slotChecks[i]->setChecked(false);
    }
    auto *acctBtn = new QPushButton(QString::fromUtf8("\u2699"), ca);
    acctBtn->setGeometry(628, 30, 24, 24);
    acctBtn->setToolTip(QString::fromUtf8("账号管理"));
    connect(acctBtn, &QPushButton::clicked, this, [this]() {
        AccountDialog dlg(this);
        connect(&dlg, &AccountDialog::accountsChanged, this, &MainWindow::loadAccountCombo);
        dlg.exec();
    });

    // 活动任务区（插入到 rootLayout 中 configArea 和 textEdit 之间）
    m_activeTaskWidget = new QWidget(this->centralWidget());
    m_activeTaskWidget->setMinimumHeight(80);
    m_activeTaskLayout = new QVBoxLayout(m_activeTaskWidget);
    m_activeTaskLayout->setContentsMargins(4, 1, 4, 1);
    m_activeTaskLayout->setSpacing(1);

    auto *rootLayout = qobject_cast<QVBoxLayout*>(this->centralWidget()->layout());
    if (rootLayout) {
        rootLayout->insertWidget(1, m_activeTaskWidget);
    }
}

void MainWindow::loadAccountCombo()
{
    QSettings settings("config.ini", QSettings::IniFormat);
    for (int i = 0; i < 3; i++) {
        QString prefix = QString("Accounts/Slot%1/").arg(QString::number(i));
        QString name = settings.value(prefix + "CharName").toString();
        if (name.isEmpty()) name = QString("窗口%1").arg(QString::number(i + 1));

        if (m_slotChecks[i])
            m_slotChecks[i]->setText(name);

        auto *slot = mScheduler->slot(i);
        slot->setCharName(settings.value(prefix + "CharName").toString());
        slot->setAccount(
            settings.value(prefix + "Account").toString(),
            settings.value(prefix + "Password").toString());
    }
}

void MainWindow::addActiveTask(int slotIdx, const QString &taskName)
{
    if (m_activeTaskRows.contains(slotIdx)) return;

    auto *row = new QWidget();
    auto *lay = new QHBoxLayout(row);
    lay->setContentsMargins(4, 1, 4, 1);
    lay->setSpacing(6);

    QString name = m_slotChecks[slotIdx] ? m_slotChecks[slotIdx]->text() : QString("窗口%1").arg(QString::number(slotIdx + 1));
    auto *label = new QLabel(QString("%1  %2").arg(name, taskName));
    lay->addWidget(label);
    lay->addStretch();

    auto *stopBtn = new QPushButton(QString::fromUtf8("停止"));
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
}

void MainWindow::removeActiveTask(int slotIdx)
{
    auto it = m_activeTaskRows.find(slotIdx);
    if (it != m_activeTaskRows.end()) {
        m_activeTaskLayout->removeWidget(it.value());
        it.value()->deleteLater();
        m_activeTaskRows.erase(it);
    }
}

// ════════════════════════════════════════════════
// 服务启动/停止（每个 slot 独立 service）
// ════════════════════════════════════════════════
void MainWindow::startService()
{
    for (int i = 0; i < 3; i++) {
        auto *slot = mScheduler->slot(i);
        if (!slot || slot->state() != GameSlot::Running) continue;
        if (!slot->isLoggedIn()) continue;

        // 每个窗口根据任务类型创建独立 Service
        BaseService *svc = nullptr;
        switch (slot->taskType()) {
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
    }

    mScheduler->start();
    ui->statuslabel->setText(QString::fromUtf8("运行中"));
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
    ui->statuslabel->setText(QString::fromUtf8("就绪"));
}

void MainWindow::startMainQuest()
{
    auto *slot = mScheduler->slot(0);
    if (slot) {
        auto *svc = new MainQuestService();
        svc->start();
        slot->setService(svc);
        slot->setState(GameSlot::Running);
    }
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
