#ifndef MOUSEKEYBOARDMANAGER_H
#define MOUSEKEYBOARDMANAGER_H

#include <QObject>
#include <QSerialPort>
class MouseKeyboardManager : public QObject
{
    Q_OBJECT
public:
    explicit MouseKeyboardManager(QObject *parent = nullptr);
    ~MouseKeyboardManager();
    static MouseKeyboardManager&Instance();

    void init();

    void clickButton(const QString &button);
signals:

private:
    QSerialPort serial;
};

#endif // MOUSEKEYBOARDMANAGER_H
