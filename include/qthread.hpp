// qthread.h – Thread with an event queue (C++17)
#pragma once

#include "eventdefs.hpp"

namespace pdk {

class QThread : public Thread {
public:
    QThread();
    explicit QThread(int nBufferLen);
    ~QThread() override = default;

    virtual bool start();
    virtual void stop();
    void close() override;

    [[nodiscard]] bool add(PEVENTINFO pEvent);
    PEVENTINFO remove();
    PEVENTINFO try_remove();
    int length() { return queue_->length(); }

    virtual bool add_event(PDK16U type, PDK16U stype, char* pValue = nullptr, int nLen = 0);
    virtual bool add_event(PDK16U type, PDK16U stype, PDK32U value);

protected:
    virtual bool add_quit_event() { return add_event(CI_QUIT_THREAD, 0); }

    std::unique_ptr<EventQueue> queue_;
};

}  // namespace pdk
