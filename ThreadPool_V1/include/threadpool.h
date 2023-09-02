#ifndef THREADPOOL_H__
#define THREADPOOL_H__

#include <vector>
#include <queue>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <functional>
#include <unordered_map>


// Any类型(c++17已支持) 用于接收任务的不同返回值
class Any
{
    public:
        Any() = default;
        template <typename T>
        Any(T data) : base_(std::make_unique<Derive<T>>(data)) 
        {}
        Any(Any&&) = default;
        Any& operator=(Any&&) = default;
        Any(const Any&) = delete;
        Any& operator=(const Any&) = delete;
        ~Any() = default;

        template <typename T>
        T cast()
        {
            Derive<T>* pd = dynamic_cast<Derive<T>*>(base_.get());
            if (pd == nullptr)
            {
                throw "type is unright, please check it!";
            }
            return pd->data_;
        }

    private:
        // 基类
        class Base
        {
            public:
                virtual ~Base() = default;
        };
        // 派生类
        template <typename T>
        class Derive : public Base
        {
            public:
                Derive(T data) : data_(data) {}
                T data_;
        };

    private:
        std::unique_ptr<Base> base_;   
};

// c++20 
class Semaphore
{
    public:
        Semaphore(int resLimit = 0) : resLimit_(resLimit) {}
        ~Semaphore() = default;

        void wait()
        {
           std::unique_lock<std::mutex> lk(mtx_); 
           cond_.wait(lk, [&]() {return resLimit_ > 0;});
           resLimit_ --;
        }

        void post()
        {
            std::unique_lock<std::mutex> lk(mtx_);
            resLimit_ ++;
            cond_.notify_all();
        }
    private:
        int resLimit_;
        std::mutex mtx_;
        std::condition_variable cond_;
};


// 封装任务的返回值类
class Task;
class Result
{
    public:
        Result(std::shared_ptr<Task> task = nullptr, bool isValid = true);
        ~Result() = default;
        Any get();
        void setVal(Any any);
    private:
        Any res_;
        Semaphore sem_;
        std::shared_ptr<Task> task_;
        std::atomic_bool isValid_;
};

// 线程模式
enum PoolMode
{
    MODE_FIXED,  // 固定线程数
    MODE_CACHED  // 动态线程数
};


// 任务抽象基类
// 用户需重写run方法
class Task
{
    public:
        Task() : result_(nullptr){}
        void exec();
        virtual Any run() = 0; // 对外接口
        virtual ~Task() = default;
        void setResult(Result* result_);
    
    private:
        Result* result_;
};

// 线程类型 
class Thread
{
    public:
        using ThreadFunc = std::function<void(ulong)>;
        Thread(ThreadFunc);
        ~Thread();
        void start();
        ulong getId() const {return threadId_;}
    private:
        ThreadFunc func_;
        static ulong idIdx_;
        ulong threadId_;
};



class ThreadPool
{
    public:
        ThreadPool();
        ~ThreadPool();

        void start(int initThreadSize = std::thread::hardware_concurrency());
        void setMode(PoolMode mode);

        // 设置任务队列上限
        void setTaskQueThreshHold(int threshhold);

        // 设置cached模式下的线程数目上限
        void setCachedModeThreadSizeLimit(int threashHold);

        // 提交任务
        Result submitTask(std::shared_ptr<Task> task);

        ThreadPool(const ThreadPool&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;

    private:
        // 定义每个线程的任务函数 std::bind绑定到Thread中
        void threadFunc(ulong threadId);
        // 检查线程池的运行状态
        bool checkRunningState() const;


    private:
        PoolMode poolMode_;   // 当前线程池的工作模式

       
        std::unordered_map<ulong, std::unique_ptr<Thread>> threads_; // 线程列表
        int initThreadSize_;         // 初始的线程数量
        int threadSizeThreshHold_;   // cached模式下线程数量上限
        std::atomic_int idleThreadSize_;   // 空闲线程的个数
        std::atomic_int curThreadSize_; // 当前线程数

        // 使用shared_ptr延长用户所提交任务的生命周期 确保执行该任务时Task没有被销毁
        std::queue<std::shared_ptr<Task>> taskQue_; // 任务队列
        std::atomic_uint taskSize_;  // 任务数量
        int taskQueMaxThreshHold_;      // 任务数量上限

        std::mutex taskQueMtx_; 
        std::condition_variable notFull_;  // 任务队列未满
        std::condition_variable notEmpty_; // 任务队列不空
        std::atomic_bool isPoolRunning_;   // 线程启动状态

        std::condition_variable exitCond_; // 等待线程池中所有资源回收

};

#endif