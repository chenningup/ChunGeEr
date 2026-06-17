#include "slotscheduler.h"

SlotScheduler::SlotScheduler(QObject *parent)
    : QObject(parent)
{
    for (int i = 0; i < 3; i++) {
        auto *gs = new GameSlot(i, this);
        m_slots.append(gs);
    }
}

void SlotScheduler::start()
{
    if (m_running) return;
    m_running = true;
    m_current = 0;
    if (auto *s = current()) {
        s->bringToFront();
        s->setState(GameSlot::Running);
    }
    emit started();
}

void SlotScheduler::stop()
{
    m_running = false;
    for (auto *s : m_slots)
        if (s->state() == GameSlot::Running)
            s->setState(GameSlot::Idle);
    emit stopped();
}

void SlotScheduler::next()
{
    if (!m_running) return;

    auto *old = current();
    if (old) old->setState(GameSlot::Idle);

    m_current = (m_current + 1) % 3;
    auto *s = current();
    if (!s) return;

    s->bringToFront();
    s->setState(GameSlot::Running);
    emit switched(m_current, s);
}

void SlotScheduler::switchTo(int idx)
{
    if (idx < 0 || idx >= 3) return;
    m_current = idx;
    if (m_running) {
        if (auto *s = current()) {
            s->bringToFront();
            s->setState(GameSlot::Running);
            emit switched(m_current, s);
        }
    }
}
