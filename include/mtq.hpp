#ifndef __MSGQ_HD_H__
#define __MSGQ_HD_H__

#include "qthread.hpp"
#include "list.hpp"
#include "eventdefs.hpp"
#include "msgqueue.hpp"
#if 0
#include "mtadef.hpp"
#endif
#include "util.hpp"

namespace pdk {

template<class P, class T>
class CMsgQueueThread : public QThread {
public:
    CMsgQueueThread();
    CMsgQueueThread(int nBufferLen);
    virtual ~CMsgQueueThread();
    void start(const char* path, int id, MSG_QUEUE_TYPE type);
    int queue_id() { return m_pMsgQueue->queue_id(); }
    void set_send_id(MSG_QUEUE_TYPE type) { m_pMsgQueue->set_send_id(type); }
    void set_recv_id(MSG_QUEUE_TYPE type) { m_pMsgQueue->set_recv_id(type); }
    static bool MsgQueueRecv(void* self, void* pBuffer, int nLen);
    P GetQueue();
    bool PutQueue(P pMsg);

protected:
    void* thread_proc();

    MsgQueue<P, T>* m_pMsgQueue;
    Logger* logger_;
};

template<class P, class T>
inline CMsgQueueThread<P, T>::CMsgQueueThread()
    : m_pMsgQueue(new MsgQueue<P, T>),
      logger_(Logger::get())
{}

template<class P, class T>
inline CMsgQueueThread<P, T>::CMsgQueueThread(int nBufferLen)
    : QThread::QThread(nBufferLen),
      m_pMsgQueue(new MsgQueue<P, T>),
      logger_(Logger::get())
{}

template<class P, class T>
inline CMsgQueueThread<P, T>::~CMsgQueueThread()
{}

template<class P, class T>
inline void CMsgQueueThread<P, T>::start(const char* path, int id, MSG_QUEUE_TYPE type)
{
    m_pMsgQueue->set_send_id(MSG_RECV_TYPE);
    m_pMsgQueue->set_recv_id(type);
    m_pMsgQueue->start(path, id, CMsgQueueThread::MsgQueueRecv);
    m_pMsgQueue->set_owner(this);

    create();
}

template<class P, class T>
inline P CMsgQueueThread<P, T>::GetQueue()
{
    P pRet = NULL;
    PEVENTINFO pEvent = NULL;

    pEvent = try_remove();
    if (pEvent)
        pRet = (P)(pEvent->Value);

    return pRet;
}

template<class P, class T>
inline bool CMsgQueueThread<P, T>::PutQueue(P pMsg)
{
    return m_pMsgQueue->send(pMsg) != -1;
}

template<class P, class T>
inline void* CMsgQueueThread<P, T>::thread_proc()
{
    while (!do_exit())
        milli_sleep(100);

    return NULL;
}

template<class P, class T>
inline bool CMsgQueueThread<P, T>::MsgQueueRecv(void* self, void* pBuffer, int nLen)
{
    if (!self)
        return false;
    CMsgQueueThread* pThis = (CMsgQueueThread*)self;

#if 0
	nLen = ((comm_head_t *)pBuffer)->ulLen;
#endif
    return pThis->add_event(0, 0, (char*)pBuffer, nLen);
}

#endif /* __MSGQ_HD_H__ */

}  // namespace pdk
