#include "LocalPlayer.h"
#include <chrono>
#include <thread>
#include <iostream>

int main()
{
    setlocale(LC_ALL, "zh_CN.UTF-8");

    auto start_time = std::chrono::steady_clock::now();

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

        player.play();
        printf("开始播放...\n");

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

        if (actual_duration > duration - 0.5 && actual_duration < duration + 0.5)
        {
            printf("✅ 播放速度正常！\n");
        }
        else
        {
            printf("❌ 播放速度异常！倍率: %.2fx\n", actual_duration / duration);
        }

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
