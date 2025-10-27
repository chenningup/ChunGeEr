#include "signalslotconnector.h"

SignalSlotConnector::SignalSlotConnector(QObject *parent)
    : QObject{parent}
{

}

SignalSlotConnector &SignalSlotConnector::Instance()
{
    static SignalSlotConnector mSignalSlotConnector;
    return mSignalSlotConnector;
}
