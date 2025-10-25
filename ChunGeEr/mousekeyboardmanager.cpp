#include "mousekeyboardmanager.h"
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QDebug>
#include <QThread>
#include <QCursor>
#include <random>
#include <windows.h>
#include <cmath>
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
    connect(&timer, &QTimer::timeout, [&]() {
        if (serial.bytesAvailable() > 0) {
            QByteArray data = serial.readAll();
            qDebug() << "接收到数据:" << data;
        }
    });
    timer.start(1);
}

bool MouseKeyboardManager::isOpen()
{
    return serial.isOpen();
}

void MouseKeyboardManager::clickButton(const QString &button)
{
    if(!serial.isOpen() || button.isEmpty())
    {
        return;
    }

    QByteArray ba = button.toLatin1();
    unsigned char key[100] = {0} ;
    key[0] = 0x66;
    key[1] = 0x68;
    key[2] = button.size() + 2 + 2;
    key[3] = 0x01;
    key[4] = button.size();
    for (int i = 0; i < ba.size(); ++i)
    {
        key[5+i] =( unsigned char)ba.at(i);
    }
    uint16_t crc= crc_16(&key[3],button.size() + 2);
    memcpy(&key[4+button.size()],&crc,sizeof(uint16_t));
    key[5+button.size() + 2] = 0x5B;
    key[5+button.size() + 3] = 0x81;
    qDebug()<<"write";
    serial.write((const char *)key,5+button.size() + 4);
}

void MouseKeyboardManager::clickButton(int button)
{
    QThread::sleep(5);
    unsigned char key[100] = {0} ;
    key[0] = 0x66;
    key[1] = 0x68;
    key[2] = 0x05;
    key[3] = 0x01;
    key[4] = 1;
    key[5] = button;
    uint16_t crc= crc_16(&key[3],3);
    memcpy(&key[6],&crc,sizeof(uint16_t));
    key[8] = 0x5B;
    key[9] = 0x81;
    qDebug()<<"write";
    serial.write((const char *)key,10);
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

    unsigned char tmp[100] = {0} ;
    char data[100] = {0};
    data[0] =  0x02;
    data[1] =  1;
    memcpy(&data[2],&x,sizeof(int));
    memcpy(&data[6],&y,sizeof(int));
    createPacket((char*)tmp,data,10);
    serial.write((const char *)tmp,10 + 3 + 4);
    serial.flush();
    serial.waitForBytesWritten();
    qDebug() <<"moveMouse leave";
}

void MouseKeyboardManager::mouseMoveDirect(int x, int y)
{
    QPoint current = QCursor::pos();
    //qDebug()<<"mouseMoveDirect enter: move to:" << x << y<<"cur pos:"<<current.x()<<current.y();
    int moveXpiece = x - current.x() > 0 ? 100 : -100;
    int moveYpiece = y - current.y() > 0 ? 100 : -100;
    int tmpx = current.x();
    int tmpy = current.y();
    while( tmpx != x || tmpy != y)
    {
        int movex;
        int movey;
        if(x - tmpx > 100 || x - tmpx < -100)
        {
            movex = moveXpiece;
        }
        else
        {
            movex = x - tmpx;
        }
        if(y - tmpy > 100 || y - tmpy < -100)
        {
            movey = moveYpiece;
        }
        else
        {
            movey = y - tmpy;
        }

        tmpx+=movex;
        tmpy+=movey;
        moveMouse(movex,movey);
        int index = 0;
        while (QCursor::pos().x() != tmpx || QCursor::pos().y() != tmpy)
        {
            QThread::msleep(1);
            index++;
            if(index >=1000)
            {
                break;
            }
        }
        current = QCursor::pos();
       // qDebug()<<"tmp mouseMoveDirect :" << movex << movey <<"cur x,y:"<<current.x() << current.y();

    }
    current = QCursor::pos();
    //qDebug()<<"mouseMoveDirect leave" <<"cur x,y:"<<current.x() << current.y();

}

void MouseKeyboardManager::mousePress(int type)
{
    unsigned char tmp[100] = {0} ;
    QByteArray array;
    array.push_back(0x02);
    array.push_back(type == MOUSE_LEFT ? 5 : 6);
    int size = createPacket((char*)tmp,array.data(),array.size());
    serial.write((const char *)tmp,size);
    serial.flush();
    serial.waitForBytesWritten();
}

void MouseKeyboardManager::mouseRelease(int type)
{
    unsigned char tmp[100] = {0} ;
    QByteArray array;
    array.push_back(0x02);
    array.push_back(type == MOUSE_LEFT ? 7 : 8);
    int size = createPacket((char*)tmp,array.data(),array.size());
    serial.write((const char *)tmp,size);
    serial.flush();
    serial.waitForBytesWritten();
}

void MouseKeyboardManager::keyPress(int key)
{
//    unsigned char key[100] = {0} ;
//    key[3] = 0x01;
//    key[4] = 1;
//    key[5] = button;
//    uint16_t crc= crc_16(&key[3],3);
//    memcpy(&key[6],&crc,sizeof(uint16_t));
//    key[8] = 0x5B;
//    key[9] = 0x81;
//    qDebug()<<"write";
//    serial.write((const char *)key,10);


    unsigned char tmp[100] = {0} ;
    QByteArray array;
    array.push_back(0x01);
    array.push_back(0x01);
    array.push_back(key);
    int size = createPacket((char*)tmp,array.data(),array.size());
    serial.write((const char *)tmp,size);
    serial.flush();
    serial.waitForBytesWritten();

}


void MouseKeyboardManager::keyRelease(int key)
{
    unsigned char tmp[100] = {0} ;
    QByteArray array;
    array.push_back(0x01);
    array.push_back(0x02);
    array.push_back(key);
    int size = createPacket((char*)tmp,array.data(),array.size());
    serial.write((const char *)tmp,size);
    serial.flush();
    serial.waitForBytesWritten();
}

int MouseKeyboardManager::createPacket(char *dist, char *data, int datasize)
{
    (*dist) = 0x66;
    *(dist + 1) = 0x68;
    *(dist + 2) = datasize+2;
    memcpy(dist + 3,data,datasize);
    uint16_t crc= crc_16((uint8_t *)dist + 3,datasize);
    memcpy(dist + 3 + datasize,&crc,sizeof(uint16_t));
    *(dist + 3+ datasize +2) = 0x5B;
    *(dist + 3+ datasize +3) = 0x81;
    return 3+datasize+4;
}


void MouseKeyboardManager::mouseClick()
{
    unsigned char key[100] = {0} ;
    key[0] = 0x66;
    key[1] = 0x68;
    key[2] = 0x02;
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
