#include "TestOpengl.h"
#include "ui_TestQtOpengl.h"

#include "LocalPlayer.h"
#include "XOpenGLVideoWidget.h"
#include "XOverlayUtil.h"

TestOpengl::TestOpengl(QWidget* parent) : QWidget(parent)
{
    ui = new Ui::TestQtOpenglClass();
    ui->setupUi(this);

    player_ = std::make_unique<LocalPlayer>();
    player_->setOpenGLWidget(ui->openGLWidget);
    player_->setRenderBackend(RenderBackend::OpenGL);
    player_->setOverlayStyle(defaultRecOverlayStyle());

    if (player_->open("assert/output.mp4"))
    {
        player_->play();
    }
}

TestOpengl::~TestOpengl()
{
    if (player_)
    {
        player_->stop();
    }
    delete ui;
}
