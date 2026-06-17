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
    , mService(nullptr)
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
    connect(ui->startStopBtn, &QPushButton::clicked, this, &MainWindow::on_startStopBtn_clicked);

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
// 启动/停止 & 启动器处理
// ════════════════════════════════════════════════
void MainWindow::on_startStopBtn_clicked()
{
    ui->statuslabel->setText(QString::fromUtf8("启动中..."));

    GameSlot::TaskType taskType = static_cast<GameSlot::TaskType>(ui->taskCombo->currentIndex());
    QString param;
    if (taskType == GameSlot::Dungeon) {
        auto *page = ui->taskConfigStack->widget(0);
        auto *combo = page->findChild<QComboBox *>("dungeonCombo");
        auto *spin = page->findChild<QSpinBox *>("dungeonCount");
        if (combo) param = combo->currentText() + " x" + QString::number(spin ? spin->value() : 1);
    }

    QSettings settings("config.ini", QSettings::IniFormat);
    QString gamePath = settings.value("Accounts/GamePath").toString();
    bool launched = false;

    for (int i = 0; i < 3; i++) {
        if (!m_slotChecks[i] || !m_slotChecks[i]->isChecked()) continue;

        auto *slot = mScheduler->slot(i);
        if (!slot) continue;

        if (m_activeTaskRows.contains(i)) {
            QMessageBox::warning(this, QString::fromUtf8("提示"),
                QString("窗口%1 已在运行中，请先停止").arg(QString::number(i + 1)));
            continue;
        }

        slot->setTask(taskType, param);
        slot->setState(GameSlot::Running);
        addActiveTask(i, slot->taskName());

        if (!launched && !gamePath.isEmpty()) {
            launched = true;
            if (gamePath.endsWith(".lnk", Qt::CaseInsensitive)) {
                ShellExecuteW(nullptr, L"open",
                    reinterpret_cast<LPCWSTR>(gamePath.utf16()),
                    nullptr, nullptr, SW_SHOWNORMAL);
            } else if (QFileInfo::exists(gamePath)) {
                SHELLEXECUTEINFOW sei = {sizeof(sei)};
                sei.lpVerb = L"runas";
                sei.lpFile = reinterpret_cast<LPCWSTR>(gamePath.utf16());
                sei.lpDirectory = reinterpret_cast<LPCWSTR>(
                    QFileInfo(gamePath).absolutePath().utf16());
                sei.nShow = SW_SHOWNORMAL;
                ShellExecuteExW(&sei);
            }
        }
    }

    if (launched) {
        startLauncherHandler();
    }
}

void MainWindow::startLauncherHandler()
{
    m_launcherStep = 0;
    m_launcherTicks = 0;
    if (!m_launcherTimer) {
        m_launcherTimer = new QTimer(this);
        connect(m_launcherTimer, &QTimer::timeout, this, &MainWindow::handleLauncherStep);
    }
    m_launcherTimer->start(2000);
    ui->statuslabel->setText(QString::fromUtf8("等待启动器..."));
}

void MainWindow::handleLauncherStep()
{
    m_launcherTicks++;
    if (m_launcherTicks > 30) {
        m_launcherTimer->stop();
        ui->statuslabel->setText(QString::fromUtf8("启动器超时"));
        return;
    }

    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) return;
    QImage img = screen->grabWindow(0).toImage().convertToFormat(QImage::Format_BGR888);
    cv::Mat frame(img.height(), img.width(), CV_8UC3, const_cast<uchar*>(img.bits()), img.bytesPerLine());
    frame = frame.clone();

    auto &gu = GameUtils::Instance();
    QString loginDir = gu.templateRoot() + "/login";
    auto &km = MouseKeyboardManager::Instance();

    switch (m_launcherStep) {
    case 0:
    {
        HWND hwnd = FindWindowW(nullptr, L"大唐无双");
        if (!hwnd) hwnd = FindWindowW(L"Qt5152QWindowIcon", nullptr);
        if (hwnd) {
            SetForegroundWindow(hwnd);
            m_launcherStep = 1;
            m_launcherTicks = 0;
        }
        break;
    }
    case 1:
    {
        auto match = gu.bestMatch(frame, loginDir, "update_btn");
        if (match.confidence > 0.7) {
            km.humanMouseMove(match.centerX, match.centerY);
            QThread::msleep(100);
            km.mouseClick();
            ui->statuslabel->setText(QString::fromUtf8("正在更新..."));
            m_launcherStep = 2;
            m_launcherTicks = 0;
        } else {
            m_launcherStep = 2;
            m_launcherTicks = 0;
        }
        break;
    }
    case 2:
    {
        auto updateMatch = gu.bestMatch(frame, loginDir, "update_btn");
        if (updateMatch.confidence > 0.7) {
            km.humanMouseMove(updateMatch.centerX, updateMatch.centerY);
            QThread::msleep(100);
            km.mouseClick();
            m_launcherTicks = 0;
            break;
        }
        auto match = gu.bestMatch(frame, loginDir, "enter_game_btn");
        if (match.confidence > 0.7) {
            km.humanMouseMove(match.centerX, match.centerY);
            QThread::msleep(100);
            km.mouseClick();
            ui->statuslabel->setText(QString::fromUtf8("进入游戏..."));
            m_launcherStep = 3;
            m_launcherTicks = 0;
        }
        break;
    }
    case 3:
    {
        HWND hwnd = FindWindowW(nullptr, L"大唐无双");
        if (!hwnd) hwnd = FindWindowW(L"Qt5152QWindowIcon", nullptr);
        if (hwnd) {
            m_launcherTimer->stop();
            startService();
        }
        break;
    }
    }
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

    // 活动任务区（挂在 configArea 下方，也在 rootLayout 中）
    m_activeTaskWidget = new QWidget(ca);
    m_activeTaskWidget->setGeometry(0, 50, ca->width(), 120);
    m_activeTaskLayout = new QVBoxLayout(m_activeTaskWidget);
    m_activeTaskLayout->setContentsMargins(4, 1, 4, 1);
    m_activeTaskLayout->setSpacing(1);
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
        removeActiveTask(idx);
        mScheduler->slot(idx)->setState(GameSlot::Idle);
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
// 服务启动/停止
// ════════════════════════════════════════════════
void MainWindow::startService()
{
    if (mService) {
        mService->stopService();
        mService->deleteLater();
        mService = nullptr;
    }

    int idx = ui->taskCombo->currentIndex();
    switch (idx) {
    case 0: // 副本
        mService = new ClientDungeonService();
        break;
    case 1: // 主线
        mService = new MainQuestService();
        break;
    case 2: // 冒险
    case 3: // 一条龙
        mService = nullptr; // TODO
        break;
    }

    if (mService) {
        mService->start();
        ui->statuslabel->setText(QString::fromUtf8("运行中"));
    }
}

void MainWindow::stopService()
{
    if (mService) {
        mService->stopService();
        mService->deleteLater();
        mService = nullptr;
    }
    ui->statuslabel->setText(QString::fromUtf8("就绪"));
}

void MainWindow::startMainQuest()
{
    mService = new MainQuestService();
    mService->start();
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
