#ifndef MOUSEKEYBOARDMANAGER_H
#define MOUSEKEYBOARDMANAGER_H

#include <QObject>

class MouseKeyboardManager : public QObject
{
    Q_OBJECT
public:
    explicit MouseKeyboardManager(QObject *parent = nullptr);

    static MouseKeyboardManager&Instance();

    void init();

signals:
};

#endif // MOUSEKEYBOARDMANAGER_H
