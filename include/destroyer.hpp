
namespace pdk {

#ifndef _SINGLETON_DESTROYER_H_
#define _SINGLETON_DESTROYER_H_

template<class T>
class Singleton_Destroyer {
public:
    Singleton_Destroyer() : m_pT(NULL) {}
    ~Singleton_Destroyer() { delete m_pT; }
    void Register(T* pT) { m_pT = pT; }

private:
    T* m_pT;
};

#endif

}  // namespace pdk
