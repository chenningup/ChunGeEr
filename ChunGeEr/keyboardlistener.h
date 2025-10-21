#ifndef KEYBOARDLISTENER_H
#define KEYBOARDLISTENER_H

#include <QThread>

class Keyboardlistener : public QThread
{
    Q_OBJECT
public:
    explicit Keyboardlistener(QObject *parent = nullptr);

    static Keyboardlistener&Instance();

    void run();

    void startListen();

    void stopListen();
signals:
    void keyPressEvent(int vkCode);
    void keyReleaseEvent(int vkCode);
private:
    bool isListen;
};

#endif // KEYBOARDLISTENER_H
