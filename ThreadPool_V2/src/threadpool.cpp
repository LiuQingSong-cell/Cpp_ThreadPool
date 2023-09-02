#include "../include/threadpool.h"

#include <iostream>
#include <ctime>

const int TASK_MAX_THRESHHOLD = 1024;
const int THREAD_MAX_THRESHHOLE = 20; // cached模式下线程数目的上限
const int THREAD_MAX_IDLE_TIME = 60; // 秒

ThreadPool::ThreadPool():
    initThreadSize_(0),
    taskSize_(0),
    taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD),
    threadSizeThreshHold_(10),
    poolMode_(MODE_FIXED),
    isPoolRunning_(false),
    idleThreadSize_(0),
    curThreadSize_(0)
{

}

ThreadPool::~ThreadPool()
{
    isPoolRunning_ = false;

    std::unique_lock<std::mutex> lk(taskQueMtx_);
    notEmpty_.notify_all();
    exitCond_.wait(lk, [&]() {return threads_.size() == 0;});
}

bool ThreadPool::checkRunningState() const
{
    return isPoolRunning_;
}

void ThreadPool::setMode(PoolMode mode)
{
    if (checkRunningState()) return ;
    poolMode_ = mode;
}

void ThreadPool::setTaskQueThreshHold(int threshhold)
{
    if (checkRunningState()) return ;
    taskQueMaxThreshHold_ = threshhold;
}

void ThreadPool::setCachedModeThreadSizeLimit(int threashHold)
{
    if (checkRunningState() || poolMode_ != MODE_CACHED) return ;
    threadSizeThreshHold_ = threashHold;
}

void ThreadPool::start(int initThreadSize)
{
    isPoolRunning_ = true;
    initThreadSize_ = initThreadSize;
    curThreadSize_ = initThreadSize;

    // 创建线程对象
    for (int i = 0; i < initThreadSize_; i++)
    {
        // emplace_back会直接使用传入的参数在尾部构造std::unique_ptr<Thread>
        // threads_.emplace_back(new Thread(std::bind(&ThreadPool::threadFunc, this)));
        auto threadPtr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
        ulong id = threadPtr->getId();
        threads_[id] = std::move(threadPtr);
    }

    for (int i = 0; i < initThreadSize_; i++)
    {
        threads_[i]->start();
        idleThreadSize_ ++;
    }
}

void ThreadPool::threadFunc(ulong threadId)
{
    auto last_time = std::chrono::high_resolution_clock().now();
    for (;;)
    {
        Task task;
        {
            std::unique_lock<std::mutex> lk(taskQueMtx_);
            while (taskQue_.size() == 0) 
            {
                if (!isPoolRunning_) // 保证threadpool析构的时候所有任务都完成再退出
                {
                    threads_.erase(threadId);
                    std::cout << threadId << " exit because threadpool life is over!" << std::endl;
                    exitCond_.notify_all();
                    return ;
                }

                if (poolMode_ == MODE_CACHED)
                {
                    if (std::cv_status::timeout == notEmpty_.wait_for(lk, std::chrono::seconds(1)))
                    {
                        auto now = std::chrono::high_resolution_clock().now();
                        auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - last_time);
                        if (dur.count() > THREAD_MAX_IDLE_TIME)
                        {
                            std::cout << threadId << " exit because idle time is too long!" << std::endl;
                            threads_.erase(threadId);
                            curThreadSize_ --;
                            idleThreadSize_ --;
                            return ;
                        }
                    }
                }
                else
                {
                    notEmpty_.wait(lk);
                }

            }

            task = taskQue_.front();
            taskQue_.pop();
            idleThreadSize_ --;
            taskSize_ --;
            if (taskQue_.size() > 0)
            {
                notEmpty_.notify_all();
            }
            notFull_.notify_all();
        }
        if (task != nullptr)
        {
            task(); // 执行funtional<void()>
        }

        idleThreadSize_ ++;
        last_time = std::chrono::high_resolution_clock().now();
    }
}

// --------------------Thread类方法实现-------------------------------

ulong Thread::idIdx_ = 0;

Thread::Thread(ThreadFunc func) : func_(func), threadId_(idIdx_ ++)
{}

Thread::~Thread()
{}


void Thread::start()
{
    std::thread t(func_, threadId_);
    t.detach();
}