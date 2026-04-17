// main.cpp – PDK 4.0 demonstration / smoke test (C++17)
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>
#include "base.h"
#include "logger.h"
#include "util.h"
#include "qthread.h"
#include "procinfo.h"
#include "pnr.h"

using namespace pdk;

// ── Timer thread: responds to CI_TIMER_DONE events ───────────────────────────
class CThread1 : public QThread {
public:
    bool start_timer(int msec, PDK8U which = 0) { return ::start_timer(this, msec, which); }
    bool stop_timer(PDK8U which = 0) { return ::stop_timer(this, which); }

protected:
    void* thread_proc() override
    {
        srand(static_cast<unsigned int>(time(nullptr)));
        Logger* pLogger = Logger::get();
        const int nTime[5] = {100, 100, 300, 400, 500};

        while (!do_exit()) {
            PEVENTINFO pEvent = remove();
            assert(pEvent != nullptr);
            if (EVENTINFO_TYPE(pEvent) == CI_TIMER_DONE) {
                pLogger->log(Logger::Info,
                             "TIMER_DONE[%p:%d]",
                             static_cast<void*>(this),
                             EVENTINFO_SUBTYPE(pEvent));
                int nWhich = rand() % 2;
                start_timer(nTime[nWhich], static_cast<PDK8U>(nWhich));
            }
        }
        pLogger->log(Logger::Info, "CThread1 stopped...");
        return nullptr;
    }
};

// ── Timer starter thread ──────────────────────────────────────────────────────
class CTimerStarter : public QThread {
public:
    CTimerStarter(CThread1* obj, int nCount) : m_pObj(obj), m_nCount(nCount) {}

protected:
    CThread1* m_pObj;
    int m_nCount;
    void* thread_proc() override
    {
        const int nTime[5] = {100, 200, 300, 400, 500};
        Logger* pLogger = Logger::get();
        while (!do_exit()) {
            int nWhich = rand() % 2;
            for (int i = 0; i < m_nCount; i++) {
                if (m_pObj[i].start_timer(nTime[nWhich], static_cast<PDK8U>(nWhich)))
                    pLogger->log(Logger::Info,
                                 "start_timer OK[%p:%d]",
                                 static_cast<void*>(&m_pObj[i]),
                                 nWhich);
            }
            milli_sleep(100);
        }
        return nullptr;
    }
};

// ── Timer stopper thread ──────────────────────────────────────────────────────
class CTimerStopper : public QThread {
public:
    CTimerStopper(CThread1* obj, int nCount) : m_pObj(obj), m_nCount(nCount) {}

protected:
    CThread1* m_pObj;
    int m_nCount;
    void* thread_proc() override
    {
        Logger* pLogger = Logger::get();
        while (!do_exit()) {
            int nWhich = rand() % 2;
            for (int i = 0; i < m_nCount; i++) {
                if (!m_pObj[i].stop_timer(static_cast<PDK8U>(nWhich)))
                    pLogger->log(Logger::Error,
                                 "stop_timer Err[%p:%d]",
                                 static_cast<void*>(&m_pObj[i]),
                                 nWhich);
                else
                    pLogger->log(Logger::Info,
                                 "stop_timer OK[%p:%d]",
                                 static_cast<void*>(&m_pObj[i]),
                                 nWhich);
            }
            milli_sleep(100);
        }
        return nullptr;
    }
};

// ── main ──────────────────────────────────────────────────────────────────────
int main(int argc, char* argv[])
{
    Logger* pLogger = Logger::get();
    pLogger->create_file(argv[0], "./");
    pLogger->log(Logger::Info, "PDK 4.0 smoke test started");

    // ── Process tree ──────────────────────────────────────────────────────────
    {
        ProcInfo proc;
        int nCount = proc.update();
        pLogger->log(Logger::Info, "Process count: %d", nCount);
        proc.view_tree(0);

        ProcInfo::PPROC pInit = proc.GetProc(1);
        if (pInit)
            pLogger->log(Logger::Info, "PID 1 cmd: %s", pInit->cmd);
    }

    // ── File open / Archive store+load ──────────────────────────────────────
    {
        File file;
        if (!file.open("main.cpp", File::modeRead)) {
            fprintf(stderr, "Cannot open main.cpp\n");
        } else {
            pLogger->log(Logger::Info, "Opened '%s', length=%ld", file.filename(), file.length());
            file.close();
        }

        // Archive round-trip: store int + string, then load back
        File ar_file("./test_archive.bin", File::modeWrite | File::modeCreate);
        {
            Archive ar_store(&ar_file, Archive::store);
            int32_t val = 0x1234;
            ar_store << val;
            ar_store.close();
        }
        ar_file.close();

        ar_file.open("./test_archive.bin", File::modeRead);
        {
            Archive ar_load(&ar_file, Archive::load);
            int32_t val = 0;
            ar_load >> val;
            ar_load.close();
            pLogger->log(Logger::Info, "Archive round-trip: 0x%x", val);
            assert(val == 0x1234);
        }
        ar_file.close();
    }

    // ── Timer thread test ─────────────────────────────────────────────────────
    {
        constexpr int nCount = 10;
        CThread1 testThread[nCount];
        for (int i = 0; i < nCount; i++) {
            testThread[i].start();
            testThread[i].start_timer(100, 0);
        }

        CTimerStarter starter(testThread, nCount);
        CTimerStopper stopper(testThread, nCount);
        starter.start();
        stopper.start();

        milli_sleep(500);  // run briefly

        starter.stop();
        stopper.stop();
        for (int i = 0; i < nCount; i++)
            testThread[i].stop();
    }

    // ── tokenize ──────────────────────────────────────────────────────────────
    {
        std::vector<std::string> tokens;
        tokenize("hello world foo bar", tokens);
        for (const auto& t : tokens)
            pLogger->log(Logger::Info, "Token: %s", t.c_str());
    }

    pLogger->log(Logger::Info, "PDK 4.0 smoke test finished");
    return 0;
}
