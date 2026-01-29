#include <SDL2/SDL.h>

#include <iostream>

int main(int argc, char *argv[])
{
    int w = 800;
    int h = 600;
    /// 1 初始化SDL video库
    if (SDL_Init(SDL_INIT_VIDEO))
    {
        std::cout << SDL_GetError() << std::endl;
        return -1;
    }

    /// 2 生成SDL 窗口
    auto screen = SDL_CreateWindow("test sdl ffmpeg",
                                   SDL_WINDOWPOS_CENTERED, /// 窗口位置
                                   SDL_WINDOWPOS_CENTERED, w, h, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!screen)
    {
        std::cout << SDL_GetError() << std::endl;
        return -2;
    }

    /// 3 生成渲染器
    auto render = SDL_CreateRenderer(screen, -1, SDL_RENDERER_ACCELERATED);
    if (!render)
    {
        std::cout << SDL_GetError() << std::endl;
        return -3;
    }

    /// 4 生成材质
    // auto texture = SDL_CreateTexture(render, SDL_PIXELFORMAT_ARGB8888,
    //                                  SDL_TEXTUREACCESS_STREAMING, /// 可加锁
    //                                  w, h);

    auto texture = SDL_CreateTexture(render, SDL_PIXELFORMAT_RGBA8888,
                                     SDL_TEXTUREACCESS_STREAMING, /// 可加锁
                                     w, h);
    if (!texture)
    {
        std::cout << SDL_GetError() << std::endl;
        return -4;
    }

    /// 存放图像的数据
    std::shared_ptr<unsigned char> rgb(new unsigned char[w * h * 4]);
    auto                           r = rgb.get();
    // unsigned char                  tmp = 255;

    for (;;)
    {
        /// 判断退出
        SDL_Event ev;
        SDL_WaitEventTimeout(&ev, 10);
        if (ev.type == SDL_QUIT)
        {
            SDL_DestroyWindow(screen);
            break;
        }

        {
            // for (int j = 0; j < h; j++)
            // {
            //     int b = j * w * 4;
            //     for (int i = 0; i < w * 4; i += 4)
            //     {
            //         r[b + i]     = 0;   ///< B
            //         r[b + i + 1] = 0;   ///< G
            //         r[b + i + 2] = 255; ///< R
            //         r[b + i + 3] = 0;   ///< A
            //     }
            // }

            for (int j = 0; j < h; j++)
            {
                int b = j * w * 4;
                for (int i = 0; i < w * 4; i += 4)
                {
                    r[b + i]     = 255; ///< R
                    r[b + i + 1] = 0;   ///< G
                    r[b + i + 2] = 0;   ///< B
                    r[b + i + 3] = 255; ///< A
                }
            }

            /// 5 内存数据写入材质
            SDL_UpdateTexture(texture, nullptr, r, w * 4);

            /// 6 清理屏幕
            SDL_RenderClear(render);
            SDL_Rect sdl_rect;
            sdl_rect.x = 0;
            sdl_rect.y = 0;
            sdl_rect.w = w;
            sdl_rect.h = h;

            /// 7 复制材质到渲染器
            SDL_RenderCopy(render, texture,
                           nullptr,  /// 原图位置和尺寸
                           &sdl_rect /// 目标位置和尺寸
            );
        }

        /// 8 渲染
        SDL_RenderPresent(render);
    }

    getchar();
    return 0;
}
