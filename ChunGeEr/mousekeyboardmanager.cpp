#include "mousekeyboardmanager.h"
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QDebug>
#include <QThread>
#include <QCursor>
#include <random>
#include <windows.h>
#pragma comment(lib, "User32.lib")
#pragma comment(lib, "imm32.lib")
static uint16_t crc_16(uint8_t *data, uint16_t len)
{
    uint16_t crc_reg = 0xffff;
    for (uint16_t i = 0; i < len; i++)
    {
        //infof(" crc_16{:x} i {}", data[i],i);
        crc_reg ^= data[i];
        for (uint16_t j = 0; j < 8; j++)
        {
            if (crc_reg & 0x01)
            {
                crc_reg = ((crc_reg >> 1) ^ 0xa001);
            }
            else
            {
                crc_reg = crc_reg >> 1;
            }
        }
    }
    return crc_reg;
}
int getRandomInRange(int min, int max) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(min, max);
    return distrib(gen);
}
MouseKeyboardManager::MouseKeyboardManager(QObject *parent)
    : QObject{parent}
{}

MouseKeyboardManager::~MouseKeyboardManager()
{
    serial.close();
}

MouseKeyboardManager &MouseKeyboardManager::Instance()
{
    static MouseKeyboardManager mMouseKeyboardManager;
    return mMouseKeyboardManager;
}
static HKL s_previousLayout;
void MouseKeyboardManager::init()
{
    QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
    if (ports.isEmpty()) {
        qDebug() << "未找到任何可用串口。";
        return;
    }
    qDebug() << "找到以下串口：";
    QSerialPortInfo choosePort;
    bool isfound = false;
    for (const auto &port : ports)
    {
        if(port.description().contains("Leonardo"))
        {
            isfound = true;
            choosePort = port;
        }
    }
    if(!isfound)
    {
        return;
    }
    // 2. 使用第一个找到的串口（实际应用中应由用户选择或根据条件确定）

    serial.setPortName(choosePort.portName());

    // 3. 配置串口参数
    serial.setBaudRate(QSerialPort::Baud9600);
    serial.setDataBits(QSerialPort::Data8);
    serial.setParity(QSerialPort::NoParity);
    serial.setStopBits(QSerialPort::OneStop);
    serial.setFlowControl(QSerialPort::NoFlowControl);

    // 4. 尝试打开串口
    if (serial.open(QIODevice::ReadWrite))
    {
        qDebug() << "串口" << serial.portName() << "打开成功！";
    }
    else
    {
        qDebug() << "打开串口失败：" << serial.errorString();
    }
    connect(&serial,&QSerialPort::readyRead,[=](){
        QByteArray Read_Buf=serial.readAll();
        qDebug()<<Read_Buf;
    });
}

void MouseKeyboardManager::clickButton(const QString &button)
{
    if(!serial.isOpen() || button.isEmpty())
    {
        return;
    }

    QByteArray ba = button.toLatin1();
    QThread::sleep(5);
    unsigned char key[100] = {0} ;
    key[0] = 0x66;
    key[1] = 0x68;
    key[2] = 0x01;
    key[3] = button.size();
    for (int i = 0; i < ba.size(); ++i)
    {
        key[4+i] =( unsigned char)ba.at(i);
    }
    uint16_t crc= crc_16(&key[2],button.size() + 2);
    memcpy(&key[4+button.size()],&crc,sizeof(uint16_t));
    key[4+button.size() + 2] = 0x5B;
    key[4+button.size() + 3] = 0x81;

    // QByteArray data;
    // data.append(0x66);
    // data.append(0x68);
    // data.append(0x01);
    // data.append(0x61);
    // data.append(0xC1);
    // data.append(0xC8);
    // data.append(0x5B);
    // data.append(0x81);
    qDebug()<<"write";
    serial.write((const char *)key,4+button.size() + 4);
}

void MouseKeyboardManager::clickButton(int button)
{
    QThread::sleep(5);
    unsigned char key[100] = {0} ;
    key[0] = 0x66;
    key[1] = 0x68;
    key[2] = 0x01;
    key[3] = 1;
    key[4] = button;
    uint16_t crc= crc_16(&key[2],3);
    memcpy(&key[5],&crc,sizeof(uint16_t));
    key[7] = 0x5B;
    key[8] = 0x81;
    qDebug()<<"write";
    serial.write((const char *)key,4+1 + 4);
}

void MouseKeyboardManager::humanMouseMove(int endX, int endY)
{
    QThread::sleep(2);
    QPoint current = QCursor::pos();
    double distance = sqrt(pow(endX - current.x(), 2) + pow(endY - current.y(), 2));
    int steps = static_cast<int>(distance * 0.30); // 减少步数比例提高速度
    steps = (std::max)(5, (std::min)(steps, 300));  // 限制在5-50步之间

    // 生成轻微曲线控制点
    int controlX = (current.x() + endX) / 2 + getRandomInRange(-20, 20);
    int controlY = (current.y() + endY) / 2 + getRandomInRange(-20, 20);
    int preX = current.x();
    int preY = current.y();
    for (int i = 0; i <= steps; i++) {
        double t = (double)i / steps;
        // 二次贝塞尔曲线
        int x = (int)(pow(1-t,2)*current.x() + 2*(1-t)*t*controlX + pow(t,2)*endX);
        int y = (int)(pow(1-t,2)*current.y() + 2*(1-t)*t*controlY + pow(t,2)*endY);

        moveMouse(x - preX , y - preY);
        preX = x;
        preY = y;
        // 动态延迟：非线性变化，但总体更快
        int delay = 3 + (int)(2* fabs(sin(t * 3.14159))); // 减少基础延迟
        if (i < steps)
        {
            QThread::msleep(delay);
        }
    }
    qDebug()<<"steps"<<steps;
}

void MouseKeyboardManager::moveMouse(int x, int y)
{
    if(x == 0 && y ==0)
    {
        return ;
    }
    // 转换为当前窗口坐标
    //QPoint windowPos = this->mapFromGlobal(globalPos);
    //qDebug() << "Current Window Position:" << windowPos;
    qDebug() <<"move x:"<<x<<",move y:"<<y;
    unsigned char key[100] = {0} ;
    key[0] = 0x66;
    key[1] = 0x68;
    key[2] = 0x02;
    key[3] = 1;
    memcpy(&key[4],&x,sizeof(int));
    memcpy(&key[8],&y,sizeof(int));
    uint16_t crc= crc_16(&key[2],2+8);
    memcpy(&key[12],&crc,sizeof(uint16_t));
    key[14] = 0x5B;
    key[15] = 0x81;
    qDebug()<<"write";
    serial.write((const char *)key,4+10 +4);
    serial.flush();
    serial.waitForBytesWritten();
}


void MouseKeyboardManager::mouseClick()
{
    QThread::sleep(5);
    unsigned char key[100] = {0} ;
    key[0] = 0x66;
    key[1] = 0x68;
    key[2] = 0x02;
    key[3] = 2;
    uint16_t crc= crc_16(&key[2],2);
    memcpy(&key[4],&crc,sizeof(uint16_t));
    key[6] = 0x5B;
    key[7] = 0x81;
    qDebug()<<"write";
    serial.write((const char *)key,4+ 4);
}

void MouseKeyboardManager::mouseDoubleClick()
{
    QThread::sleep(5);
    unsigned char key[100] = {0} ;
    key[0] = 0x66;
    key[1] = 0x68;
    key[2] = 0x02;
    key[3] = 3;
    uint16_t crc= crc_16(&key[2],2);
    memcpy(&key[4],&crc,sizeof(uint16_t));
    key[6] = 0x5B;
    key[7] = 0x81;
    qDebug()<<"write";
    serial.write((const char *)key,4+ 4);
}

void MouseKeyboardManager::mouseRightClick()
{
    QThread::sleep(5);
    unsigned char key[100] = {0} ;
    key[0] = 0x66;
    key[1] = 0x68;
    key[2] = 0x02;
    key[3] = 4;
    uint16_t crc= crc_16(&key[2],2);
    memcpy(&key[4],&crc,sizeof(uint16_t));
    key[6] = 0x5B;
    key[7] = 0x81;
    qDebug()<<"write";
    serial.write((const char *)key,4+ 4);
}
