#ifndef SLOTSCHEDULER_H
#define SLOTSCHEDULER_H

#include <QObject>
#include <QTimer>
#include <QList>
#include "gameslot.h"

// ════════════════════════════════════════════
// 轮询调度：三个窗口轮流拉到前台干活
// ════════════════════════════════════════════
class SlotScheduler : public QObject
{
    Q_OBJECT
public:
    explicit SlotScheduler(QObject *parent = nullptr);

    GameSlot *slot(int i) { return i >= 0 && i < m_slots.size() ? m_slots[i] : nullptr; }
    GameSlot *current() { return slot(m_current); }
    int currentIndex() const { return m_current; }
    int slotCount() const { return m_slots.size(); }

    bool running() const { return m_running; }

    // 轮询间隔（ms），一个 slot 完成一轮工作后停留多久再切下一个
    void setSlotDurationMs(int ms) { m_slotDuration = ms; }

public slots:
    void start();          // 从当前 slot 开始轮询
    void stop();
    void next();           // 立即切到下一个 slot
    void switchTo(int idx);

signals:
    void switched(int index, GameSlot *slot);
    void started();
    void stopped();

private:
    QList<GameSlot *> m_slots;
    int m_current = 0;
    bool m_running = false;
    int m_slotDuration = 5000;   // 每个 slot 最小停留 5s
};

#endif // SLOTSCHEDULER_H
