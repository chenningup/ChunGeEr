#ifndef SERIALPORTMANAGER_H
#define SERIALPORTMANAGER_H

#include <QObject>
#include <QSerialPort>
#include <QMutex>
#include <QSemaphore>
#include <QThread>
class SerialPortManager : public QThread
{
    Q_OBJECT
public:
    explicit SerialPortManager(QObject *parent = nullptr);
    ~SerialPortManager();

    void stop();

    void sendData(const QByteArray &data);
    bool isOpen(){return serial->isOpen();};
protected:
    void run() override;
private slots:
    void handleReadyRead();                     // 串口接收槽
signals:
    void dataReceived(const QByteArray &data);  // 接收信号
    void finished();                            // 线程结束信号
private:
    bool m_running = false;
    QSerialPort *serial = nullptr;
    QMutex mutex;
    QSemaphore sem;
    QList<QByteArray> sendQueue;
};

#endif // SERIALPORTMANAGER_H
