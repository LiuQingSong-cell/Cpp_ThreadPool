#include "./include/threadpool.h"

#include <iostream>
#include <chrono>

int sum(int begin, int end)
{
    int sum = 0; 
    for (int i = begin; i <= end; i++)
    {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        sum += i;
    }
    
    return sum;
}


int main()
{
    {
        ThreadPool threadPool;
        threadPool.setMode(MODE_CACHED);
        threadPool.start(2);
        std::future<int> res1 = threadPool.submitTask(sum, 1, 1000000000);
        std::future<int> res2 = threadPool.submitTask(sum, 1, 1000000000);
        std::future<int> res3 = threadPool.submitTask(sum, 1, 1000000000);
        std::future<int> res4 = threadPool.submitTask(sum, 1, 1000000000);
        std::future<int> res6 = threadPool.submitTask([]() {
            int sum = 0;
            for (int i = 10; i <= 100; i++)
                sum += i;
            std::cout << "end of task" << std::endl;
            return sum;
        });

        std::cout << res6.get() << std::endl;
        // std::this_thread::sleep_for(std::chrono::seconds(10));
    }

    std:: cout << "ThreadPool life is over!" << std::endl;

    // getchar();

    return 0;
}