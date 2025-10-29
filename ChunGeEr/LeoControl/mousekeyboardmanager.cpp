#include "mousekeyboardmanager.h"
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QDebug>
#include <QThread>
#include <QCursor>
#include <random>
#include <windows.h>
#include <cmath>
#include <QDateTime>
#include "Commons/Log/XuLog.h"
#pragma comment(lib, "User32.lib")
#pragma comment(lib, "imm32.lib")
static QHash<int,int>keyHash ={
    {192,189},
    {121,0xCB},
    {122,0xCC},
};
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
    : QThread{parent}
{}

MouseKeyboardManager::~MouseKeyboardManager()
{
    isStart = false;
}

MouseKeyboardManager &MouseKeyboardManager::Instance()
{
    static MouseKeyboardManager mMouseKeyboardManager;
    return mMouseKeyboardManager;
}
static HKL s_previousLayout;
void MouseKeyboardManager::init()
{
    // timer.start(50);
    mSerialManager.start();
    isStart = true;
    start();
}

void MouseKeyboardManager::run()
{
    while (isStart)
    {
        taskSem.acquire();
        taskMutex.lock();
        if(taskList.isEmpty())
        {
            taskMutex.unlock();
            continue;
        }
        LeoTask task = taskList[0];
        taskList.pop_front();
        taskMutex.unlock();
        if(task.task == "MouseMoveSync")
        {
            mouseMoveDirect(task.x,task.y);
        }
        if(task.task == "MousePressSync")
        {
            mousePress(task.mouseType);
        }
        if(task.task == "MouseReleaseSync")
        {
            mouseRelease(task.mouseType);
        }
        if(task.task == "KeybordPressSync")
        {
            keyPress(task.key);
        }
        if(task.task == "KeybordReleaseSync")
        {
            keyRelease(task.key);
        }
    }
}

void MouseKeyboardManager::pushbackTask(const LeoTask &task)
{
    if(isSoleOperate)
    {
        return;
    }
    taskMutex.lock();
    taskList.push_back(task);
    taskMutex.unlock();
    taskSem.release();
}

void MouseKeyboardManager::startSoleOperate()
{
    isSoleOperate = true;
}

bool MouseKeyboardManager::waitForSoleOperate()
{
    return taskList.isEmpty();
}

bool MouseKeyboardManager::isOpen()
{
    return mSerialManager.isOpen();
}

void MouseKeyboardManager::clickButton(const QString &button)
{
    if(!mSerialManager.isOpen() || button.isEmpty())
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
    mSerialManager.sendData(QByteArray((const char *)key,5+button.size() + 4));
    //serial.write((const char *)key,5+button.size() + 4);
}

void MouseKeyboardManager::clickButton(int button)
{
    keyPress(button);
    QThread::msleep(200);
    keyRelease(button);
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
    QDateTime start = QDateTime::currentDateTime();
    infof("moveMouse enter:x:{},y{}",x,y);
    unsigned char tmp[100] = {0} ;
    char data[100] = {0};
    data[0] =  0x02;
    data[1] =  1;
    memcpy(&data[2],&x,sizeof(int));
    memcpy(&data[6],&y,sizeof(int));
    createPacket((char*)tmp,data,10);
    mSerialManager.sendData(QByteArray((const char *)tmp,10 + 3 + 4));
    //serial.write((const char *)tmp,10 + 3 + 4);
    QDateTime end = QDateTime::currentDateTime();
    infof("moveMouse leave:x:{},y{},cost:{}",x,y,start.msecsTo(end));
}

void MouseKeyboardManager::mouseMoveDirect(int x, int y)
{
    infof("mouseMoveDirect enter,x:{},y:{}",x,y);
    QDateTime start = QDateTime::currentDateTime();
    QPoint current = QCursor::pos();
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
        QPoint tmpbefor = QCursor::pos();
        moveMouse(movex,movey);
        int index = 0;
        while (QCursor::pos().x() != tmpx || QCursor::pos().y() != tmpy)
        {
            QThread::msleep(1);
            index++;
            if(index >= 500)
            {
                errorf("moveMouse movex:{},movey:{},timeout,before pos,x:{},y:{},later pos,x:{},y:{}",movex,movey,tmpbefor.x(),tmpbefor.y(),QCursor::pos().x(),QCursor::pos().y());
                return;
            }
        }
    }
    current = QCursor::pos();
    QDateTime end = QDateTime::currentDateTime();
    infof("mouseMoveDirect leave,cost:{},cursor pos,x:{},y:{}",start.msecsTo(end),current.x(),current.y());
}

void MouseKeyboardManager::mousePress(int type)
{
    infof("mousePress enter,type:{}",type);
    QDateTime start = QDateTime::currentDateTime();
    unsigned char tmp[100] = {0} ;
    QByteArray array;
    array.push_back(0x02);
    array.push_back(type == MOUSE_LEFT ? 5 : 6);
    int size = createPacket((char*)tmp,array.data(),array.size());
//    serial.write((const char *)tmp,size);
//    serial.flush();
//    serial.waitForBytesWritten();
    mSerialManager.sendData(QByteArray((const char *)tmp,size));
    QDateTime end = QDateTime::currentDateTime();
    infof("mousePress leave cost:{}",start.msecsTo(end));
}

void MouseKeyboardManager::mouseRelease(int type)
{
    infof("mouseRelease enter,type:{}",type);
    QDateTime start = QDateTime::currentDateTime();
    unsigned char tmp[100] = {0} ;
    QByteArray array;
    array.push_back(0x02);
    array.push_back(type == MOUSE_LEFT ? 7 : 8);
    int size = createPacket((char*)tmp,array.data(),array.size());
    mSerialManager.sendData(QByteArray((const char *)tmp,size));
//    serial.write((const char *)tmp,size);
//    serial.flush();
//    serial.waitForBytesWritten();
    QDateTime end = QDateTime::currentDateTime();
    infof("mouseRelease leave cost:{}",start.msecsTo(end));
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
    int endkey = key;
    if(keyHash.contains(key))
    {
        endkey = keyHash[key];
    }
    if(key>=65 && key<=90)
    {
        endkey = key+32;
    }

    infof("keyPress enter,endkey:{}",endkey);
    QDateTime start = QDateTime::currentDateTime();
    unsigned char tmp[100] = {0} ;
    QByteArray array;
    array.push_back(0x01);
    array.push_back(0x01);
    array.push_back(endkey);
    int size = createPacket((char*)tmp,array.data(),array.size());
    mSerialManager.sendData(QByteArray((const char *)tmp,size));
//    serial.write((const char *)tmp,size);
//    serial.flush();
//    serial.waitForBytesWritten();
    QDateTime end = QDateTime::currentDateTime();
    infof("keyPress leave cost:{}",start.msecsTo(end));
}


void MouseKeyboardManager::keyRelease(int key)
{
    int endkey = key;
    if(keyHash.contains(key))
    {
        endkey =  keyHash[key];
    }
    if(key>=65 && key<=90)
    {
        endkey = key+32;
    }
    infof("keyRelease enter,endkey:{}",endkey);
    QDateTime start = QDateTime::currentDateTime();
    unsigned char tmp[100] = {0} ;
    QByteArray array;
    array.push_back(0x01);
    array.push_back(0x02);
    array.push_back(endkey);
    int size = createPacket((char*)tmp,array.data(),array.size());
    mSerialManager.sendData(QByteArray((const char *)tmp,size));
//    serial.write((const char *)tmp,size);
//    serial.flush();
//    serial.waitForBytesWritten();
    QDateTime end = QDateTime::currentDateTime();
    infof("keyRelease leave cost:{}",start.msecsTo(end));
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
    mousePress(MOUSE_LEFT);
    QThread::msleep(200);
    mouseRelease(MOUSE_LEFT);
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
    mSerialManager.sendData(QByteArray((const char *)key,4+ 4));
    //serial.write((const char *)key,4+ 4);
}

void MouseKeyboardManager::mouseRightClick()
{
    mousePress(MOUSE_RIGHT);
    QThread::msleep(200);
    mouseRelease(MOUSE_RIGHT);
}
