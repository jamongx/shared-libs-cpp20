
namespace pdk {
/*

	HURIM Copy Right

	1. Declare Trace Class

	File : trace.h
	Create Date : 2004. by Kang

*/

#ifndef __TRACE_CLASS_H__
#define __TRACE_CLASS_H__

template<class T>
class Trace {
protected:
    T m_Variable;
    static void Event(T& v) { fprintf(stderr, "On Event = %#x:\n", &v); }

public:
    typedef void (*PCallBack)(T&);
    Trace() : OnWrite(Event), OnRead(Event) {}
    Trace(PCallBack OnEvent) : OnWrite(OnEvent), OnRead(OnEvent) {}
    ~Trace() {}
    Trace<T>& operator=(T v)
    {
        m_Variable = v;
        if (OnWrite)
            OnWrite(m_Variable);
        return *this;
    }
    Trace<T>& operator+=(T v)
    {
        m_Variable += v;
        if (OnWrite)
            OnWrite(m_Variable);
        return *this;
    }
    PCallBack OnWrite;
    PCallBack OnRead;
};

#endif  // __TRACE_CLASS_H__

}  // namespace pdk
