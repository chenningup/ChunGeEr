#include "serialportmanager.h"
#include <QMutexLocker>
#include <QDebug>
#include <QSerialPortInfo>
SerialPortManager::SerialPortManager(QObject *parent)
    : QThread{parent}
{

}

SerialPortManager::~SerialPortManager()
{
    stop();
    wait();
}

void SerialPortManager::stop()
{
    mutex.lock();
    m_running = false;
    mutex.unlock();
    sem.release();
}

void SerialPortManager::sendData(const QByteArray &data)
{
    mutex.lock();
    sendQueue.push_back(data);
    mutex.unlock();
    sem.release();
}

void SerialPortManager::run()
{
    QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
    if (ports.isEmpty())
    {
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
    serial = new QSerialPort;
    serial->setPortName(choosePort.portName());               // 修改为你的端口
    serial->setBaudRate(QSerialPort::Baud9600);
    serial->setDataBits(QSerialPort::Data8);
    serial->setParity(QSerialPort::NoParity);
    serial->setStopBits(QSerialPort::OneStop);
    serial->setFlowControl(QSerialPort::HardwareControl);

    if (!serial->open(QIODevice::ReadWrite))
    {
        qWarning() << "Open serial failed:" << serial->errorString();
        delete serial;
        serial = nullptr;
        return;
    }

    connect(serial, &QSerialPort::readyRead, this, &SerialPortManager::handleReadyRead, Qt::DirectConnection);

    qDebug() << "Serial thread started in:" << QThread::currentThread();
    m_running = true;

    while (m_running)
    {
        sem.acquire();
        mutex.lock();
        QByteArray data = sendQueue.isEmpty() ? QByteArray() : sendQueue[0];
        sendQueue.pop_front();
        mutex.unlock();

        if (!data.isEmpty())
        {
            qint64 w = serial->write(data);
            if (w > 0)
            {
                serial->flush();
                serial->waitForBytesWritten(10); // 非必须，可降低缓冲延迟
            }
        }
    }

    serial->close();
    delete serial;
    serial = nullptr;
    emit finished();

    qDebug() << "Serial thread stopped.";
}

void SerialPortManager::handleReadyRead()
{
    if (!serial) return;
    QByteArray data = serial->readAll();
    if (!data.isEmpty())
    {
        qDebug()<<data;
        emit dataReceived(data);
    }
}
