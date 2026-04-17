// qthread.cpp – QThread implementation
#include <cstring>
#include "eventdefs.hpp"
#include "logger.hpp"
#include "qthread.hpp"

namespace pdk {

QThread::QThread() : queue_(std::make_unique<EventQueue>(DEF_EVENT_QUEUE_LENGTH)) {}

QThread::QThread(int nBufferLen) : queue_(std::make_unique<EventQueue>(nBufferLen)) {}

bool QThread::start()
{
    return create() == 1;
}

void QThread::stop()
{
    close();
}

void QThread::close()
{
    close_pre();
    add_quit_event();
    close_post();
}

bool QThread::add(PEVENTINFO pEvent)
{
    if (queue_->put(pEvent, EVENTINFO_LENGTH(pEvent)) != 0) {
        Logger::get()->log(Logger::Warn,
                           "QThread: event queue full, event dropped (type=0x%04x)",
                           EVENTINFO_TYPE(pEvent));
        return false;
    }
    return true;
}

PEVENTINFO QThread::remove()
{
    return queue_->get();
}

PEVENTINFO QThread::try_remove()
{
    PEVENTINFO pEvent = nullptr;
    if (queue_->try_get(&pEvent) == 1)
        return pEvent;
    return nullptr;
}

bool QThread::add_event(PDK16U type, PDK16U stype, char* pValue, int nLen)
{
    EVENTINFO Event;
    PEVENTINFO pEvent = &Event;
    memset(pEvent, 0, sizeof(EVENTINFO));
    EVENTINFO_TYPE(pEvent) = type;
    EVENTINFO_SUBTYPE(pEvent) = stype;
    if (pValue && nLen > 0)
        memcpy(pEvent->Value, pValue, nLen);
    EVENTINFO_LENGTH(pEvent) = static_cast<int>(EVENTINFO_HEADER_LENGTH) + nLen;
    return add(pEvent);
}

bool QThread::add_event(PDK16U type, PDK16U stype, PDK32U value)
{
    EVENTINFO Event;
    PEVENTINFO pEvent = &Event;
    memset(pEvent, 0, sizeof(EVENTINFO));
    EVENTINFO_TYPE(pEvent) = type;
    EVENTINFO_SUBTYPE(pEvent) = stype;
    *reinterpret_cast<PDK32U*>(&pEvent->Value[0]) = value;
    EVENTINFO_LENGTH(pEvent) = static_cast<int>(EVENTINFO_HEADER_LENGTH) + sizeof(PDK32U);
    return add(pEvent);
}

}  // namespace pdk
