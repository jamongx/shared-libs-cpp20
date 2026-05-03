// jthread.cpp – Thread implementation using std::jthread (C++20)

#include <cerrno>
#include <cstdio>
#include "jthread.hpp"

namespace pdk {

// ── Thread::create ───────────────────────────────────────────────────────────
//
// Thread lifecycle is fully serialised by `mutex_`. All state-mutating
// entry points (create / close_pre / close_post / close) acquire it so a
// concurrent start()/close() race cannot leave thread_ or stop_source_ in
// an inconsistent state.
//
// Note on stop tokens: we maintain our own `stop_source_` and ignore the
// std::jthread's built-in token. Two reasons: (1) the lambda we hand to
// jthread doesn't accept the token because subclasses override
// thread_proc() with no parameters; (2) close_pre() needs to be callable
// without a live jthread (e.g., during graceful shutdown to mark
// "stopping" before joining). The header documents this decision.
int Thread::create(int bDetached)
{
    std::unique_lock<std::mutex> lk(mutex_);

    // Wait out any in-progress shutdown (close_post that has moved the
    // jthread out of thread_ and is parked in join()). Without this guard
    // we could see thread_ == nullopt while the previous thread is still
    // executing and would create a second worker that races with it.
    cv_.wait(lk, [this] { return !closing_; });

    if (thread_ && thread_->joinable())
        return 1;   // already running

    if (bDetached != 0) {
        // jthread always joins; detach mode is no longer supported.
        std::fprintf(stderr,
                     "[Thread] detached mode requested but ignored — jthread always joins\n");
    }

    // Refresh the stop_source if it was previously triggered.
    if (stop_source_.stop_requested()) {
        stop_source_ = std::stop_source{};
    }

    // We deliberately drop the jthread-supplied stop_token; the canonical
    // signal is our own stop_source_ which do_exit() polls.
    thread_.emplace([this] {
        thread_proc();
    });

    return 1;
}

// ── Thread::close ────────────────────────────────────────────────────────────
void Thread::close()
{
    close_pre();
    close_post();
}

void Thread::close_pre()
{
    std::unique_lock<std::mutex> lk(mutex_);
    stop_source_.request_stop();
}

void Thread::close_post()
{
    // Race-free shutdown:
    //   1. Under mutex_, mark closing_=true and move the jthread out into
    //      a local. This blocks any concurrent create() at the cv_.wait
    //      gate above until we publish closing_=false.
    //   2. Release the mutex BEFORE join — otherwise a thread_proc() that
    //      called back into a public Thread API would deadlock here.
    //   3. After join completes, re-acquire the mutex, clear thread_ to
    //      nullopt, set closing_=false, and notify any waiters in create().
    std::optional<std::jthread> local;
    {
        std::unique_lock<std::mutex> lk(mutex_);
        if (closing_ || !thread_) return;   // already torn down
        closing_ = true;
        local.emplace(std::move(*thread_));
    }
    if (local && local->joinable())
        local->join();
    {
        std::unique_lock<std::mutex> lk(mutex_);
        thread_.reset();
        closing_ = false;
        cv_.notify_all();
    }
}

// ── Thread::thread_exists ─────────────────────────────────────────────────────
int Thread::thread_exists()
{
    std::unique_lock<std::mutex> lk(mutex_);
    return (thread_ && thread_->joinable()) ? 1 : 0;
}

}  // namespace pdk
