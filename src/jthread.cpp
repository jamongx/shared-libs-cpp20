// jthread.cpp – Thread implementation using std::thread

#include <cerrno>
#include <cstdio>
#include "jthread.hpp"

namespace pdk {

// ── Thread::Create ───────────────────────────────────────────────────────────
int Thread::create(int bDetached)
{
    std::unique_lock<std::mutex> lk(mutex_);
    if (created_.load())
        return 1;

    stop_.store(false);
    created_.store(true);

    thread_ = std::thread([this, bDetached] {
        thread_proc();
        created_.store(false);
    });

    if (bDetached == 1)
        thread_.detach();

    return 1;
}

// ── Thread::Close ────────────────────────────────────────────────────────────
void Thread::close()
{
    close_pre();
    close_post();
}

void Thread::close_pre()
{
    stop_.store(true, std::memory_order_release);
}

void Thread::close_post()
{
    if (thread_.joinable())
        thread_.join();
}

// ── Thread::thread_exists ─────────────────────────────────────────────────────
int Thread::thread_exists()
{
    return created_.load() ? 1 : 0;
}

}  // namespace pdk
