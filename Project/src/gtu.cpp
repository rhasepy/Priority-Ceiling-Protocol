#include "gtu.h"

std::forward_list<gtu::mutex*> gtu::mutex::LockedList;

gtu::mutex::mutex() : 
    std::mutex(), 
    Ceil(0), 
    IsLock(gtu::LockState::_NOT_LOCKED),
    LockMaker()
{
    #if defined(__linux__)
        CPU_ZERO(&cpuset);
        CPU_SET(0, &cpuset);
    #endif
}

gtu::mutex::~mutex() {}

void gtu::mutex::SetSchedThread()
{
    int Zero = 0;
    #if defined(_WIN32)
        SetThreadPriority(GetCurrentThread(), Nominees.find({GetCurrentThreadId(), Zero}) -> Prio);
    #elif defined(__linux__)
        sched_param Prio;
        Prio.__sched_priority = Nominees.find({pthread_self(), Zero})->Prio;
        if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &Prio)) {
            throw std::system_error(std::error_code(errno, std::system_category()), "Failed to set priority to orj while unlock!");
        }
    #endif
}

void gtu::mutex::SetYieldThread()
{
    #if defined(_WIN32)
        SwitchToThread()
    #elif defined(__linux__)
        int Chk = sched_yield();

        if (Chk) {
            throw std::system_error(std::error_code(errno,std::system_category()), "Undefined Behaviour for Unix!");
        }
    #endif

    std::this_thread::yield();
}

void gtu::mutex::unlock()
{
    int Zero = 0;
    SetSchedThread();

    this -> IsLock = gtu::LockState::_NOT_LOCKED;
    this -> LockMaker = Zero;
    
    gtu::mutex::LockedList.remove(this);
    LockedList.sort([](const gtu::mutex* begin, const gtu::mutex* end) { return *begin < *end; });

    SetYieldThread();
}

std::thread::native_handle_type gtu::mutex::GetSelf()
{
    #if defined(__linux__)
        return pthread_self();
    #elif defined(_WIN32)
        return GetCurrentThreadId();
    #endif
}

void gtu::mutex::lock()
{
    int Zero = 0;
	if(LockMaker == GetSelf()) {
        throw std::system_error(std::error_code(errno, std::system_category()), "Mutex already locked!");
    }
		
	#if defined(__linux__)
        
        std::set<gtu::mutex::MyThreadInfo>::iterator MyThreadInfoIter = this -> Nominees.find({GetSelf(), Zero});
        sched_param sch;

        int SelfPrio = Zero;
		int magic;
		if(pthread_getschedparam(MyThreadInfoIter -> ThisThread, &magic, &sch)) {
            throw std::system_error(std::error_code(errno, std::system_category()), "[ERROR] getschedparam in gtu::mutex::lock");
        }
			
		SelfPrio = sch.__sched_priority;

	#elif defined(_WIN32)
		SelfPrio = GetThreadPriority(OpenThread(THREAD_ALL_ACCESS, TRUE, (MyThreadInfoIter -> the_thread)));
	#endif

	if(!(LockedList.empty()) && (SelfPrio <= (*LockedList.begin()) -> Ceil)) {
        
        std::forward_list<mutex*>::iterator traverser = LockedList.begin();
        while (traverser != LockedList.end()) {

            if((*traverser) -> LockMaker != MyThreadInfoIter -> ThisThread && (*traverser) -> Nominees.count(*MyThreadInfoIter) == 1) {

                #if defined(__linux__)

                    sched_param priority;
                    int magic;
                    if(pthread_getschedparam((*traverser)->LockMaker, &magic, &priority)) {
                        throw std::system_error(std::error_code(errno, std::system_category()), "[ERROR] getschedparam in gtu::mutex::lock");
                    }

                    if (!(priority.__sched_priority < SelfPrio)) {
                        priority.__sched_priority = SelfPrio;
                    }
                    if (pthread_setschedparam((*traverser) -> LockMaker, SCHED_FIFO, &priority)) {
                        throw std::system_error(std::error_code(errno, std::system_category()), "[ERROR] setschedparam in gtu::mutex::lock");
                    }

                    int Chk = sched_yield();
                    if(Chk) {
                        throw std::system_error(std::error_code(errno, std::system_category()), "[ERROR] sched_yield");
                    }
                    
                #elif defined(_WIN32)

                    int priority = GetThreadPriority(OpenThread(THREAD_ALL_ACCESS, TRUE, (*traverser) -> LockMaker));
                    if (priority < SelfPrio) {
                        priority = SelfPrio;
                    }

                    if(!SetThreadPriority(OpenThread(THREAD_ALL_ACCESS, TRUE, (*traverser)->locker), priority)) {
                        throw std::system_error(std::error_code(errno, std::system_category()), "[ERROR] SetThreadPriority");
                    }
                #endif

                std::this_thread::yield();
                traverser = LockedList.begin();
            }
            else
                ++traverser;
        }
    }
		
	this -> IsLock = gtu::LockState::_LOCKED;
	this -> LockMaker = MyThreadInfoIter -> ThisThread;

	LockedList.push_front(this);
	LockedList.sort([](const mutex* begin, const mutex* end) { return *begin < *end; });
}

void gtu::mutex::Register(std::thread& thread, int prio)
{
   	#if defined(__linux__)

		if(pthread_setaffinity_np(thread.native_handle(), sizeof(cpu_set_t), &cpuset)) {
            throw std::system_error(std::error_code(errno, std::system_category()), "[ERROR] pthread_setaffinity_np");
        }
			
		sched_param priority_param({ prio });
		gtu::mutex::MyThreadInfo CurrentThreadInfo(thread.native_handle(), prio);

		if (pthread_setschedparam(thread.native_handle(), SCHED_FIFO, &priority_param)) {
            throw std::system_error(std::error_code(errno, std::system_category()), "[ERROR] setschedparam");
		}

	#elif defined(_WIN32)

		if(SetThreadIdealProcessor(input.native_handle(), 1) == -1) {
            throw std::system_error(std::error_code(errno, std::system_category()), "[ERROR] SetThreadIdealProcessor");
        }

		if(SetThreadPriorityBoost(input.native_handle(), TRUE) == 0) {
            throw std::system_error(std::error_code(errno, std::system_category()), "[ERROR] SetThreadPriorityBoost");
        }

		thread CurrentThreadInfo(GetThreadId(input.native_handle()), priority);
		if(SetThreadPriority(input.native_handle(), priority)) {
            throw std::system_error(std::error_code(errno, std::system_category()), "[ERROR] SetThreadPriority");
        }

	#endif

    if (!(this -> Ceil < prio)) {
        this -> Ceil = prio;
    }

	Nominees.insert({{CurrentThreadInfo.ThisThread, CurrentThreadInfo.Prio}}); 
}