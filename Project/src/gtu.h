#ifndef GTU_H
#define GTU_H

#include <forward_list>
#include <stdexcept>
#include <thread>
#include <string>
#include <mutex>
#include <set>


#if defined(__linux__)
    #include <pthread.h>
#elif defined(_WIN32)
    #include <Windows.h>
    using thread_object = DWORD;
#endif

namespace gtu
{
    enum LockState {
        _LOCKED, _NOT_LOCKED
    };

    class mutex : public std::mutex
    {
        public:
            mutex();
            ~mutex();

            void Register(std::thread& thread, int prio);
            void lock();
            void unlock();

            inline bool operator< (const mutex& Other) const { return (this -> Ceil) < (Other.Ceil); }

        private:
            #if defined(__linux__)
                cpu_set_t cpuset;
            #endif

            static std::forward_list<gtu::mutex *> LockedList;
            
            struct MyThreadInfo
            {
                std::thread::native_handle_type ThisThread;
                int Prio;

                MyThreadInfo(std::thread::native_handle_type ThisThread = 0, int Prio = 0) : ThisThread(ThisThread), Prio(Prio) {}
                MyThreadInfo(const gtu::mutex::MyThreadInfo& OtherThread) : MyThreadInfo(OtherThread.ThisThread, OtherThread.Prio) {}
                
                inline bool operator<(const MyThreadInfo& Other) const { return this -> ThisThread < Other.ThisThread; }
            };
            
            LockState IsLock;
            std::thread::native_handle_type LockMaker;
            std::set<gtu::mutex::MyThreadInfo> Nominees;
             
            int Ceil;

            void SetSchedThread();
            void SetYieldThread();
            std::thread::native_handle_type GetSelf();
    };
}

#endif