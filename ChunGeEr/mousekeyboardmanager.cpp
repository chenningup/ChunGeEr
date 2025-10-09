#include "mousekeyboardmanager.h"
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QDebug>
#include <QThread>
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
}

void MouseKeyboardManager::clickButton(const QString &button)
{
    if(!serial.isOpen())
    {
        return;
    }
    QThread::sleep(10);
    QByteArray data;
    data.append(0x66);
    data.append(0x68);
    data.append(0x01);
    data.append(0x61);
    data.append(0xC1);
    data.append(0xC8);
    data.append(0x5B);
    data.append(0x81);
    serial.write(data);
}
