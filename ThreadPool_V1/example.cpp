#include "./include/threadpool.h"

#include <iostream>
#include <chrono>


class MyTask : public Task
{
    public:
        MyTask(int b, int e) : begin_(b), end_(e){}
        Any run() override
        {
            long long sum = 0;
            for (int i = begin_; i <= end_; i++)
                sum += i;
            // std::this_thread::sleep_for(std::chrono::seconds(3));
            return sum; 
        }
    private:
        int begin_;
        int end_;
};


int main()
{
    {
        ThreadPool threadPool;
        threadPool.setMode(MODE_CACHED);
        threadPool.start(4);

        Result res1 = threadPool.submitTask(std::make_shared<MyTask>(1, 100));
        Result res2 = threadPool.submitTask(std::make_shared<MyTask>(100000, 200000));
        Result res3 = threadPool.submitTask(std::make_shared<MyTask>(100000, 200000));
        Result res4 = threadPool.submitTask(std::make_shared<MyTask>(100000, 200000));
        Result res5 = threadPool.submitTask(std::make_shared<MyTask>(100000, 200000));
        Result res6 = threadPool.submitTask(std::make_shared<MyTask>(100000, 200000));
        Result res7 = threadPool.submitTask(std::make_shared<MyTask>(100000, 200000));
        
        std::cout << "和为: " << res1.get().cast<long long>() + res2.get().cast<long long>() << std::endl;
        // std::this_thread::sleep_for(std::chrono::seconds(10));
    }

    std:: cout << "ThreadPool life is over!" << std::endl;

    // getchar();

    return 0;
}