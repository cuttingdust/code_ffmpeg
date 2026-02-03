#include "SdlQtRGB.h"

#include "ui_sdlqtrgb.h"

#include <SDL2/SDL.h>

#include <QtGui/QImage>
#include <QtWidgets/QMessageBox>

#include <algorithm>
#include <fstream>

static int sdl_width  = 0;
static int sdl_height = 0;

static SDL_Window   *sdl_win     = NULL;
static SDL_Renderer *sdl_render  = NULL;
static SDL_Texture  *sdl_texture = NULL;

static unsigned char *rgb      = NULL;
static unsigned char *yuv      = NULL;
static int            pix_size = 2;

static std::ifstream yuv_file;

SdlQtRGB::SdlQtRGB(QWidget *parent) : QWidget(parent)
{
    /// 打开yuv文件
    yuv_file.open(R"(.\assert\400_300_25.yuv)", std::ios::binary);
    if (!yuv_file)
    {
        QMessageBox::information(this, "", "open yuv failed!");
        return;
    }


    ui_ = new Ui::SdlQtRGBClass();
    ui_->setupUi(this);

    sdl_width  = 400;
    sdl_height = 300;
    ui_->label->resize(sdl_width, sdl_height);


    /// 初始化SDL
    SDL_Init(SDL_INIT_VIDEO);

    /// 创建窗口
    sdl_win = SDL_CreateWindowFrom((void *)ui_->label->winId());

    /// 创建渲染器
    sdl_render = SDL_CreateRenderer(sdl_win, -1, SDL_RENDERER_ACCELERATED);

    /// 创建材质
    sdl_texture =
            SDL_CreateTexture(sdl_render, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, sdl_width, sdl_height);

    yuv = new unsigned char[sdl_width * sdl_height * pix_size];

    /// 默认设置为透明
    memset(yuv, 0, sdl_width * sdl_height * pix_size);


    startTimer(10);
}

SdlQtRGB::~SdlQtRGB()
{
}

void SdlQtRGB::timerEvent(QTimerEvent *event)
{
    yuv_file.read((char *)yuv, sdl_width * sdl_height * 1.5);
    /// yuv 平面存储存储
    /// yyyyyyyy uu vv


    SDL_UpdateTexture(sdl_texture, NULL, yuv,
                      sdl_width /// 一行 y的字节数
    );
    SDL_RenderClear(sdl_render);
    SDL_Rect rect;
    rect.x = 0;
    rect.y = 0;
    rect.w = sdl_width;
    rect.h = sdl_height;
    SDL_RenderCopy(sdl_render, sdl_texture, NULL, &rect);
    SDL_RenderPresent(sdl_render);

    QWidget::timerEvent(event);
}
