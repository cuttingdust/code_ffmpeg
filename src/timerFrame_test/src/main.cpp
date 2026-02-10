#include <iostream>
#include <thread>
#include <utility>

#include <Windows.h>


void MSleep(unsigned int ms)
{
    auto beg = clock();
    for (int i = 0; std::cmp_less(i, ms); i++)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if (std::cmp_greater_equal((clock() - beg) / (CLOCKS_PER_SEC / 1000), ms))
            break;
    }
}

int main(int argc, char *argv[])
{
    /// 测试c++11的sleep
    /// 测试 sleep 10毫秒 100fps
    auto beg = clock(); /// 开始时间 ，cpu跳数
    int  fps = 0;       /// 帧率
    for (;;)
    {
        fps++;
        auto tmp = clock();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        std::cout << clock() - tmp << " " << std::flush;
        /// 1秒钟开始统计 CLOCKS_PER_SEC cpu每秒跳数
        if ((clock() - beg) / (CLOCKS_PER_SEC / 1000) > 1000) /// 间隔毫秒数
        {
            std::cout << "sleep for fps:" << fps << std::endl;
            break;
        }
    }

    /// 测试wait 事件超时控制 帧率
    auto handle = ::CreateEvent(NULL, FALSE, FALSE, NULL);
    fps         = 0;
    beg         = clock();
    for (;;)
    {
        fps++;
        ::WaitForSingleObject(handle, 10);                    /// 等待超时
        if ((clock() - beg) / (CLOCKS_PER_SEC / 1000) > 1000) /// 间隔毫秒数
        {
            std::cout << "wait fps:" << fps << std::endl;
            break;
        }
    }

    fps = 0;
    beg = clock();
    for (;;)
    {
        fps++;
        MSleep(10);
        if ((clock() - beg) / (CLOCKS_PER_SEC / 1000) > 1000) /// 间隔毫秒数
        {
            std::cout << "MSleep fps:" << fps << std::endl;
            break;
        }
    }

    getchar();
    return 0;
}
