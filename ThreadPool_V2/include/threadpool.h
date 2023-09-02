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
#include <future>
#include <iostream>

// 线程模式
enum PoolMode
{
    MODE_FIXED,  // 固定线程数
    MODE_CACHED  // 动态线程数
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
        std::packaged_task<int()> task;
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

        using Task = std::function<void()>;


        // 提交任务
        // 使用可变参数模板接收任意任务函数和任意数量参数 引用折叠 + 后置返回值类型推导
        // std::invoke_result/std::result_of
        template <typename Func, typename... Args>
        auto submitTask(Func&& func, Args&&...args) -> std::future<decltype(func(args...))>
        {
            // 打包用户提交的任务
            using RTtype = decltype(func(args...));
            auto task = std::make_shared<std::packaged_task<RTtype()>>(
                std::bind(std::forward<Func>(func), std::forward<Args>(args)...)  //--> void operator()(){ // 绑定的函数  }
            );
            std::future<RTtype> result = task->get_future();

            std::unique_lock<std::mutex> lk(taskQueMtx_);
            if (!notFull_.wait_for(lk, std::chrono::seconds(1), [&]() {return taskQue_.size() < taskQueMaxThreshHold_;}))
            {
                std::cerr << "task queue is full, submit task fail, retry later." << std::endl;
                auto task = std::make_shared<std::packaged_task<RTtype()>>(
                    []() ->RTtype {return RTtype();}
                );
                (*task)();
                return task->get_future();
            }

            // taskQue_.push(task); 
            taskQue_.emplace([task]() {
                (*task)(); // 线程池能接收的task是void() 所以需要封装一层
            });

            taskSize_++;

            notEmpty_.notify_all();

            // cached模式下 当前任务数大于空闲线程数并且当前已经创建的线程总数没有超过设定的阈值 就创建一个新的线程
            if (poolMode_ == MODE_CACHED && taskQue_.size() > idleThreadSize_ && curThreadSize_ < threadSizeThreshHold_)
            {   
                auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
                ulong id = ptr->getId();
                std::cout << "create new thread, id = " << id << std::endl;
                threads_[id] = std::move(ptr);
                threads_[id]->start();
                idleThreadSize_ ++;
                curThreadSize_ ++;
            }
            return result;
        }



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

        std::queue<Task> taskQue_; // 任务队列
        std::atomic_uint taskSize_;  // 任务数量
        int taskQueMaxThreshHold_;      // 任务数量上限

        std::mutex taskQueMtx_; 
        std::condition_variable notFull_;  // 任务队列未满
        std::condition_variable notEmpty_; // 任务队列不空
        std::atomic_bool isPoolRunning_;   // 线程启动状态

        std::condition_variable exitCond_; // 等待线程池中所有资源回收

};

#endif