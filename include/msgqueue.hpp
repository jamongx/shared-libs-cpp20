// msgqueue.h – System V message queue wrapper (C++17)
#pragma once

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <cerrno>
#include <cstring>
#include <functional>
#include <memory>
#include "jthread.hpp"
#include "logger.hpp"

namespace pdk {

#define PERMIT 0660

enum MSG_QUEUE_TYPE { MSG_SEND_TYPE = 1, MSG_RECV_TYPE, MSG_NONE_TYPE = 0xfffe };

class QThread;

template<class P, class T>
class MsgQueue : public Thread {
protected:
    struct MSGELEM {
        long mtype;
        T mbody;
    };

    std::unique_ptr<MSGELEM> send_elem_;
    std::unique_ptr<MSGELEM> recv_elem_;
    int queue_id_{-1};
    MSG_QUEUE_TYPE send_id_{MSG_SEND_TYPE};
    MSG_QUEUE_TYPE recv_id_{MSG_RECV_TYPE};
    std::function<bool(void*, void*, int)> recv_callback_;
    QThread* owner_{nullptr};
    Logger* logger_;

public:
    MsgQueue()
        : send_elem_(std::make_unique<MSGELEM>()),
          recv_elem_(std::make_unique<MSGELEM>()),
          logger_(Logger::get())
    {}
    ~MsgQueue() = default;

    int queue_id() { return queue_id_; }
    void set_send_id(MSG_QUEUE_TYPE nId) { send_id_ = nId; }
    MSG_QUEUE_TYPE send_id() { return send_id_; }
    void set_recv_id(MSG_QUEUE_TYPE nId) { recv_id_ = nId; }
    MSG_QUEUE_TYPE recv_id() { return recv_id_; }
    void set_owner(QThread* pOwner) { owner_ = pOwner; }
    QThread* owner() { return owner_; }
    void remove() { msgctl(queue_id_, IPC_RMID, nullptr); }

    bool start(const char* path, int id, std::function<bool(void*, void*, int)> callback = nullptr)
    {
        recv_callback_ = std::move(callback);
        key_t key = ftok(path, id);
        queue_id_ = msgget(key, IPC_CREAT | PERMIT);
        if (queue_id_ == -1) {
            logger_->log(
                __FILE__, __LINE__, Logger::Error, "msgget error[%d:%s]", errno, strerror(errno));
            exit(-1);
        }
        return Thread::create();
    }

    int send(P elem, int type = -1)
    {
        if (type == -1)
            type = send_id_;
        send_elem_->mtype = type;
        memcpy(&send_elem_->mbody, elem, sizeof(T));
        int nLen = msgsnd(queue_id_, static_cast<const void*>(send_elem_.get()), sizeof(T), 0);
        if (nLen == -1)
            logger_->log(Logger::Error, "msgsnd error[%d:%s]", errno, strerror(errno));
        return nLen;
    }

    void* thread_proc() override
    {
        while (!do_exit()) {
            recv_elem_->mtype = recv_id_;
            int nLen = msgrcv(
                queue_id_, static_cast<void*>(recv_elem_.get()), sizeof(T), recv_elem_->mtype, 0);
            if (nLen == -1) {
                if (errno == EINTR)
                    continue;
                logger_->log(__FILE__,
                             __LINE__,
                             Logger::Error,
                             "msgrcv error[%d:%s]",
                             errno,
                             strerror(errno));
                if (errno == E2BIG)
                    logger_->log(
                        Logger::Error, "Recv Size[%zu:%zu]", sizeof(recv_elem_->mbody), sizeof(T));
                continue;
            }
            if (recv_callback_)
                recv_callback_(static_cast<void*>(owner_),
                               static_cast<void*>(&recv_elem_->mbody),
                               static_cast<int>(sizeof(T)));
        }
        return nullptr;
    }
};

}  // namespace pdk
