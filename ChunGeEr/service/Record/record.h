#ifndef RECORD_H
#define RECORD_H

#include <QObject>
#include "../baseservice.h"
#include <QTimer>
class Record : public BaseService
{
    Q_OBJECT
public:
    explicit Record(QObject *parent = nullptr);

    void run();

    void startService();
    void stopService();
public slots:
    void timeOutSlot();
signals:
private:
    QTimer recordTime;
};

#endif // RECORD_H
