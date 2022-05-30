#include <iostream>
#include <cstring>
#include <chrono>
#include <thread>
#include <mutex>

#include "gtu.h"

gtu::mutex m1;
gtu::mutex m2;

std::mutex m1_;
std::mutex m2_;

void Thread1()
{
    try {
        std::cout << "Thread1 locking m1..." << std::endl;
        std::unique_lock<gtu::mutex> lock1(m1);
        std::cout << "Thread1 locked m1..." << std::endl;

        std::this_thread::yield();

        std::cout << "Thread1 locking m2..." << std::endl;
        std::unique_lock<gtu::mutex> lock2(m2);
        std::cout << "Thread1 locked m2..." << std::endl;
    }
    catch (std::exception& ex) {
        std::cerr << ex.what() << std::endl;
    }
}

void Thread2()
{   
    try {
        std::cout << "Thread2 locking m2..." << std::endl;
        std::unique_lock<gtu::mutex> lock2(m2);
        std::cout << "Thread2 locked m2..." << std::endl;

        std::this_thread::yield();

        std::cout << "Thread2 locking m1..." << std::endl;
        std::unique_lock<gtu::mutex> lock1(m1);
        std::cout << "Thread2 locked m1..." << std::endl;
    }  
    catch (std::exception& ex) {
        std::cerr << ex.what() << std::endl;
    }
}

void Thread1_Deadlock()
{
    try {
        std::cout << "Thread1 locking m1..." << std::endl;
        std::unique_lock<std::mutex> lock1(m1_);
        std::cout << "Thread1 locked m1..." << std::endl;

        std::this_thread::yield();

        std::cout << "Thread1 locking m2..." << std::endl;
        std::unique_lock<std::mutex> lock2(m2_);
        std::cout << "Thread1 locked m2..." << std::endl;
    }
    catch (std::exception& ex) {
        std::cerr << ex.what() << std::endl;
    }
}

void Thread2_Deadlock()
{
    try {
        std::cout << "Thread2 locking m2..." << std::endl;
        std::unique_lock<std::mutex> lock2(m2_);
        std::cout << "Thread2 locked m2..." << std::endl;

        std::this_thread::yield();

        std::cout << "Thread2 locking m1..." << std::endl;
        std::unique_lock<std::mutex> lock1(m1_);
        std::cout << "Thread2 locked m1..." << std::endl;
    }  
    catch (std::exception& ex) {
        std::cerr << ex.what() << std::endl;
    }
}

void Set_SchedFIFO(cpu_set_t& set, sched_param& prio, bool opt)
{
    #if defined(__linux__)
        prio.__sched_priority = sched_get_priority_max(SCHED_FIFO);
        if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &prio)) {
            std::cerr << "[ERROR] setschedparam: " << std::strerror(errno) << std::endl;
            exit(EXIT_FAILURE);
        }
    #endif

    if (opt) {
        std::this_thread::yield();
    }
}

void __InitSystem(cpu_set_t& cpuset, sched_param& priority)
{
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);  
}

int main(int argc, char const *argv[])
{
    #if defined(__linux__)
        cpu_set_t cpuset;
        sched_param priority;
        __InitSystem(cpuset, priority);
    #endif

    try {

        for (int i = 0; i < 50000; ++i) {

            Set_SchedFIFO(cpuset, priority, false);
            std::thread thread1(Thread1);
            std::thread thread2(Thread2);

            m1.Register(thread1, 10);
            m1.Register(thread2, 15);
            m2.Register(thread1, 10);
            m2.Register(thread2, 15);

            Set_SchedFIFO(cpuset, priority, true);
            
            thread1.join();
            thread2.join();
        }
    }
    catch (std::exception& ex) {
        std::cerr << ex.what() << std::endl;
    }

    std::string Input;
    std::cout << std::endl;
    std::cout << "If you see not applied Priority Ceiling Protocol this design. (y/n)" << std::endl;
    std::cout << "Input: ";
    std::cin >> Input;
    std::cout << "After the deadlock please kill the process Ctrl + C or any kill signal" << std::endl << std::endl;

    if (Input == "y") {

        for (int i = 0; i < 50000; ++i) {

            std::thread thread1(Thread1_Deadlock);
            std::thread thread2(Thread2_Deadlock);
            
            thread1.join();
            thread2.join();
        }
    }

    return 0;
}