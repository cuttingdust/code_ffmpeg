#include "LocalPlayer.h"
#include <AVLog.h>
#include <chrono>
#include <thread>
#include <iostream>

int main()
{
    avLogInit();
    setlocale(LC_ALL, "zh_CN.UTF-8");

    try
    {
        LocalPlayer player;
        std::string filepath = "assert/output.mp4";

        if (!player.open(filepath, nullptr))
        {
            printf("打开文件失败: %s\n", filepath.c_str());
            return -1;
        }

        double duration = player.getDuration();
        printf("视频时长: %.2f 秒\n", duration);

        player.setSpeed(PlaybackSpeed::SPEED_1_0X);
        player.play();
        printf("开始播放...\n");

        auto start_time = std::chrono::steady_clock::now();

        /// 播放 20 秒后暂停
        std::this_thread::sleep_for(std::chrono::seconds(20));

        printf("暂停播放...\n");
        player.pause();

        // 暂停 5 秒
        std::this_thread::sleep_for(std::chrono::seconds(5));

        printf("恢复播放...\n");
        player.resume();

        // 等待播放结束
        while (player.isPlaying() && !player.isFinished())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        auto end_time = std::chrono::steady_clock::now();
        auto actual_duration =
                std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count() / 1000.0;

        printf("\n========== 结果 ==========\n");
        printf("视频实际时长: %.2f 秒\n", duration);
        printf("播放实际耗时: %.2f 秒\n", actual_duration);

        player.stop();
    }
    catch (const std::exception& e)
    {
        printf("错误: %s\n", e.what());
    }

    printf("按回车键退出...\n");
    getchar();
    return 0;
}
