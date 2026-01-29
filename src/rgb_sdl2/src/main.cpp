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
    auto texture = SDL_CreateTexture(render, SDL_PIXELFORMAT_ARGB8888,
                                     SDL_TEXTUREACCESS_STREAMING, /// 可加锁
                                     w, h);
    if (!texture)
    {
        std::cout << SDL_GetError() << std::endl;
        return -4;
    }

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

        /// 8 渲染
        SDL_RenderPresent(render);
    }

    getchar();
    return 0;
}
