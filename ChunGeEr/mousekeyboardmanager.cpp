#include "mousekeyboardmanager.h"
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QDebug>
MouseKeyboardManager::MouseKeyboardManager(QObject *parent)
    : QObject{parent}
{}

MouseKeyboardManager &MouseKeyboardManager::Instance()
{
    static MouseKeyboardManager mMouseKeyboardManager;
    return mMouseKeyboardManager;
}

void MouseKeyboardManager::init()
{
    auto ports = QSerialPortInfo::availablePorts();
    if (ports.isEmpty()) {
        qDebug() << "未找到任何可用串口。";
        return;
    }
    qDebug() << "找到以下串口：";
    for (const auto &port : ports) {
        qDebug() << "  " << port.portName();
    }

    // 2. 使用第一个找到的串口（实际应用中应由用户选择或根据条件确定）
    QSerialPort serial;
    serial.setPortName(ports.first().portName());

    // 3. 配置串口参数
    serial.setBaudRate(QSerialPort::Baud9600);
    serial.setDataBits(QSerialPort::Data8);
    serial.setParity(QSerialPort::NoParity);
    serial.setStopBits(QSerialPort::OneStop);
    serial.setFlowControl(QSerialPort::NoFlowControl);

    // 4. 尝试打开串口
    if (serial.open(QIODevice::ReadWrite)) {
        qDebug() << "串口" << serial.portName() << "打开成功！";
        // ... 此处可进行数据读写操作 ...
        serial.write(QString("a").toLocal8Bit());
        serial.close(); // 操作完成后关闭串口
        qDebug() << "串口已关闭。";
    } else {
        qDebug() << "打开串口失败：" << serial.errorString();
    }
}
