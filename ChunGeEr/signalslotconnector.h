#ifndef SIGNALSLOTCONNECTOR_H
#define SIGNALSLOTCONNECTOR_H

#include <QObject>

class SignalSlotConnector : public QObject
{
    Q_OBJECT
public:
    explicit SignalSlotConnector(QObject *parent = nullptr);

    static SignalSlotConnector &Instance();
signals:
    void log(const QString&log);
};

#endif // SIGNALSLOTCONNECTOR_H
