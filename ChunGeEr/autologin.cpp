#include "autologin.h"
#include "gameutils.h"
#include "LeoControl/mousekeyboardmanager.h"
#include <QThread>
#include <QDebug>
#include <QPixmap>
#include <QImage>
#include <QScreen>
#include <QGuiApplication>

AutoLogin::AutoLogin(GameSlot *slot, QObject *parent)
    : QObject(parent), m_slot(slot)
{
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &AutoLogin::onTick);
}

QString AutoLogin::phaseText() const
{
    static const char *texts[] = {
        "空闲","启动游戏","等待窗口","检测登录","输入账号","点击登录","验证进入","完成","失败"
    };
    return texts[m_phase];
}

void AutoLogin::start()
{
    m_tickCount = 0;
    m_timer->start(1000);
    setPhase(Launching);

    if (!ensureWindow()) {
        // 窗口不存在，需要外部先启动游戏
        setPhase(Failed);
        emit finished(false);
        return;
    }
    setPhase(DetectingLogin);
}

void AutoLogin::cancel()
{
    m_timer->stop();
    setPhase(Failed);
    emit finished(false);
}

void AutoLogin::onTick()
{
    m_tickCount++;
    if (m_tickCount > m_maxWait) {
        setPhase(Failed);
        emit finished(false);
        return;
    }

    cv::Mat frame = captureScreen();
    if (frame.empty()) return;

    switch (m_phase) {
    case DetectingLogin:
        if (detectLogin(frame)) {
            setPhase(Typing);
        }
        break;

    case Typing:
        if (typeCredentials()) {
            setPhase(LoggingIn);
        }
        break;

    case LoggingIn:
        if (clickLogin()) {
            setPhase(Verifying);
            m_tickCount = 0;
        }
        break;

    case Verifying:
        if (verifyInGame(frame)) {
            m_slot->setLoggedIn(true);
            setPhase(Done);
            emit finished(true);
        }
        break;

    default:
        break;
    }
}

bool AutoLogin::ensureWindow()
{
    return m_slot->hwnd() != nullptr;
}

bool AutoLogin::launchGame()
{
    return false; // 游戏路径在主窗口级别管理
}

bool AutoLogin::detectLogin(const cv::Mat &frame)
{
    auto &gu = GameUtils::Instance();
    auto match = gu.bestMatch(frame, gu.templateRoot() + "/login");
    return !match.name.isEmpty() && match.confidence > 0.7;
}

bool AutoLogin::typeCredentials()
{
    auto &km = MouseKeyboardManager::Instance();
    km.keyPress(KEY_TAB);
    QThread::msleep(50);
    km.keyRelease(KEY_TAB);
    QThread::msleep(100);

    for (QChar ch : m_slot->account()) {
        km.clickButton(ch.toUpper());
        QThread::msleep(50);
    }
    QThread::msleep(200);

    km.keyPress(KEY_TAB);
    QThread::msleep(50);
    km.keyRelease(KEY_TAB);
    QThread::msleep(100);

    for (QChar ch : m_slot->password()) {
        km.clickButton(ch.toUpper());
        QThread::msleep(50);
    }
    QThread::msleep(200);
    return true;
}

bool AutoLogin::clickLogin()
{
    auto &km = MouseKeyboardManager::Instance();
    km.keyPress(KEY_RETURN);
    QThread::msleep(50);
    km.keyRelease(KEY_RETURN);
    QThread::msleep(500);
    return true;
}

bool AutoLogin::verifyInGame(const cv::Mat &frame)
{
    auto &gu = GameUtils::Instance();
    auto match = gu.bestMatch(frame, gu.templateRoot() + "/ingame");
    if (!match.name.isEmpty() && match.confidence > 0.7)
        return true;

    match = gu.detectLocation(frame);
    return !match.name.isEmpty() && match.confidence > 0.6;
}

cv::Mat AutoLogin::captureScreen()
{
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) return {};
    QPixmap pix = screen->grabWindow(0);
    QImage img = pix.toImage().convertToFormat(QImage::Format_BGR888);
    return cv::Mat(img.height(), img.width(), CV_8UC3,
                   const_cast<uchar *>(img.bits()), img.bytesPerLine()).clone();
}

void AutoLogin::setPhase(Phase p)
{
    if (m_phase != p) {
        m_phase = p;
        emit phaseChanged(p);
        if (p == Done || p == Failed)
            m_timer->stop();
    }
}
