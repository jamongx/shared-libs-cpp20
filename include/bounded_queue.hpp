// bounded_queue.hpp — modern bounded MPMC queue (C++17).
//
// Designed for general-purpose intra-process producer/consumer pipelines.
// Unlike pdk::CircularQueue<T,N> (which uses memcpy and therefore requires
// trivially-copyable T), BoundedQueue accepts any movable T including
// std::string, std::vector, std::unique_ptr, etc. Payloads are stored in an
// std::vector<std::optional<T>> ring with std::condition_variable-based
// blocking + timed waits.
//
// API (all functions thread-safe):
//   bool push(T value)                          — block until space
//   bool try_push(T value)                      — false if full
//   bool try_push_for(T value, ms timeout)      — false on timeout
//   std::optional<T> pop()                      — block until value, nullopt
//                                                 if shutdown() was called
//                                                 and queue drained
//   std::optional<T> try_pop()                  — empty if no value
//   std::optional<T> try_pop_for(ms timeout)    — empty on timeout
//   void shutdown()                             — wake all waiters; new pushes
//                                                 are rejected, queued items
//                                                 still drain
//   std::size_t size() / std::size_t capacity()
//
// Use the deduction guide so callers can write `pdk::BoundedQueue q{64};`.
#pragma once

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace pdk {

template<class T>
class BoundedQueue {
public:
    explicit BoundedQueue(std::size_t capacity)
        : buffer_(capacity), capacity_(capacity) {
        // capacity == 0 would cause modulo-by-zero in head_ / tail_ updates
        // and permanently block any push (size_ < 0 is impossible). Surface
        // the misconfiguration loudly rather than deadlocking later.
        if (capacity == 0) {
            throw std::invalid_argument("BoundedQueue capacity must be > 0");
        }
    }

    BoundedQueue(const BoundedQueue&) = delete;
    BoundedQueue& operator=(const BoundedQueue&) = delete;

    // Block until space is available. Returns false only if shutdown() was
    // called before the value could be enqueued.
    bool push(T value) {
        std::unique_lock<std::mutex> lk(mu_);
        not_full_.wait(lk, [&] { return size_ < capacity_ || closed_; });
        if (closed_) return false;
        buffer_[head_] = std::move(value);
        head_ = (head_ + 1) % capacity_;
        ++size_;
        not_empty_.notify_one();
        return true;
    }

    // Non-blocking. Returns false if the queue is full or shut down.
    bool try_push(T value) {
        std::unique_lock<std::mutex> lk(mu_);
        if (closed_ || size_ >= capacity_) return false;
        buffer_[head_] = std::move(value);
        head_ = (head_ + 1) % capacity_;
        ++size_;
        not_empty_.notify_one();
        return true;
    }

    // Block up to `timeout`. Returns false on timeout / shutdown.
    template<class Rep, class Period>
    bool try_push_for(T value, std::chrono::duration<Rep, Period> timeout) {
        std::unique_lock<std::mutex> lk(mu_);
        if (!not_full_.wait_for(lk, timeout,
                                [&] { return size_ < capacity_ || closed_; })) {
            return false;
        }
        if (closed_) return false;
        buffer_[head_] = std::move(value);
        head_ = (head_ + 1) % capacity_;
        ++size_;
        not_empty_.notify_one();
        return true;
    }

    // Block until a value is available. nullopt only when shutdown() was
    // called and the queue is empty (graceful drain).
    std::optional<T> pop() {
        std::unique_lock<std::mutex> lk(mu_);
        not_empty_.wait(lk, [&] { return size_ > 0 || closed_; });
        if (size_ == 0) return std::nullopt;
        return pop_locked();
    }

    // Non-blocking.
    std::optional<T> try_pop() {
        std::unique_lock<std::mutex> lk(mu_);
        if (size_ == 0) return std::nullopt;
        return pop_locked();
    }

    // Block up to `timeout`.
    template<class Rep, class Period>
    std::optional<T> try_pop_for(std::chrono::duration<Rep, Period> timeout) {
        std::unique_lock<std::mutex> lk(mu_);
        if (!not_empty_.wait_for(lk, timeout,
                                 [&] { return size_ > 0 || closed_; })) {
            return std::nullopt;
        }
        if (size_ == 0) return std::nullopt;
        return pop_locked();
    }

    // Wake all blocked producers/consumers. Subsequent push/try_push return
    // false; queued items still drain via pop / try_pop.
    void shutdown() {
        std::lock_guard<std::mutex> lk(mu_);
        closed_ = true;
        not_full_.notify_all();
        not_empty_.notify_all();
    }

    std::size_t size() const {
        std::lock_guard<std::mutex> lk(mu_);
        return size_;
    }

    std::size_t capacity() const noexcept { return capacity_; }
    bool closed() const {
        std::lock_guard<std::mutex> lk(mu_);
        return closed_;
    }

private:
    std::optional<T> pop_locked() {
        std::optional<T> out = std::move(buffer_[tail_]);
        buffer_[tail_].reset();
        tail_ = (tail_ + 1) % capacity_;
        --size_;
        not_full_.notify_one();
        return out;
    }

    mutable std::mutex mu_;
    std::condition_variable not_full_;
    std::condition_variable not_empty_;
    std::vector<std::optional<T>> buffer_;
    std::size_t capacity_;
    std::size_t head_{0};
    std::size_t tail_{0};
    std::size_t size_{0};
    bool closed_{false};
};

}  // namespace pdk
