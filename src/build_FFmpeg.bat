@echo off
chcp 65001 > nul
REM MSVC 工具链 怎么编译FFMepg
::（一） 环境准备
%comspec% /k "D:\MyApp\VS2022IDE\Common7\Tools\VsDevCmd.bat -arch=x64 -host_arch=x64"
cmd.exe //c "call \"D:\MyApp\VS2022IDE\VC\Auxiliary\Build\vcvars64.bat\" && bash"
echo %LIB% %INCLUDE%

where cl link
set MSYS2_PATH_TYPE=inherit
msys2_shell.cmd -mingw64 -here -defterm -no-start -shell bash :: "-c 直接执行命令 类似 xarg"

cd ~ && mkdir MyFile && cd MyFile

export HTTP_PROXY=http://127.0.0.1:7890
export HTTPS_PROXY=http://127.0.0.1:7890
export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig
export PKG_CONFIG_SYSTEM_INCLUDE_PATH=/usr/local/include
export PKG_CONFIG_SYSTEM_LIBRARY_PATH=/usr/local/lib

::########################################################################################
where nasm yasm
where automake autoconf diffutils
where pkg-config
REM pacman -Syu
REM pacman -S nasm
REM pacman -S yasm
REM pacman -S make
REM pacman -S cmake
REM pacman -S automake
REM pacman -S autoconf
REM pacman -S diffutils
REM pacman -S pkg-config
REM pacman -S libtool


# 1. 首先，查看pkg-config是否能找到x264
pkg-config --list-all | grep x264

# 2. 如果能找到，获取其详细的编译和链接指令
pkg-config --cflags --libs x264
# 预期输出示例：-I/usr/local/include -L/usr/local/lib -lx264

# 3. 同样方法验证其他库
pkg-config --cflags --libs fdk-aac
pkg-config --cflags --libs SDL2

pkg-config --modversion sdl2

::#########################################################################################

which cl link yasm cpp  ::确认使用cl link

::（二）编译SDL2
git clone --branch SDL2 "https://github.com/libsdl-org/SDL.git" --progress --recursive
cd SDL
cmake -S . -B build
cmake --build build
cmake --install build --config=Deubug --prefix /usr/local
::Debug 版本注意改名
pkg-config --list-all
cd ..


::（三）编译x264
git clone "https://code.videolan.org/videolan/x264.git" --progress --recursive
cd x264
CC=cl ./configure --enable-shared --enable-debug
make clean
make -j$(nproc)
make install
cp  /usr/local/lib/libx264.dll.lib /usr/local/lib/libx264.lib 
::修改x264.pc的编码位UTF8 windows 兼容
cd ..


::（四）编译x265
git clone "https://github.com/videolan/x265.git" --progress --recursive
cd x265/source
cmake -S . -B build -DENABLE_SHARED=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build
cmake --install build --config=Deubug --prefix /usr/local
cp /usr/local/lib/libx265.lib /usr/local/lib/x265.lib
cd ../../


::（五）编译fdk-acc
git clone "https://github.com/mstorsjo/fdk-aac.git" --progress --recursive
cd  fdk-aac
cmake -S . -B build -DBUILD_SHARED_LIBS=ON -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build build
cmake --install build --config=Deubug
cd ..

::（六）编译ffmepg
git clone --branch "release/8.0" "https://github.com/FFmpeg/FFmpeg.git" --progress --recursive
cd FFmpeg

$ 
CC=cl ./configure --toolchain=msvc --arch=x86_64 --enable-shared --enable-gpl --enable-nonfree --enable-sdl2 --pkg-config=/usr/bin/pkg-config

CC=cl ./configure --toolchain=msvc --arch=x86_64 --enable-shared --enable-libx264 --enable-gpl --enable-libfdk-aac --enable-nonfree --enable-libx265  --enable-sdl2 

###############################################
ffmpeg编译
• CC=cl.exe./configure --prefix=./install --toolchain=msvc --enable-shared --disable-programs --disable-ffplay --disable-ffmpeg --disable-ffprobe
--enable-libx264 --enable-gpl --enable-libfdk-aac --enable-nonfree --enable-libx265  --enable-sdl3

• -prefix=./install --toolchain=msvs#指定安装路径和工具链vS
• --enable-shared#编译为动态链接库
•＃不编译工具
• --disable-programs --disable-ffplay --disable-ffmpeg--disable-ffprobe
• --enable-libx264 --enable-libx265 #**#x264 *Д x265
• --enable-gpl #支持x264协议，x264和×265必备
• --enable-libfdk-aac --enable-nonfree # aac音频编码 aac必须包含-enable-nonfree
##################################################
cd ../SDL ::编译SDL3
# 1. 切换到 main 分支（如果存在）
git checkout main

# 如果上一步失败（提示分支不存在），则直接创建并切换到跟踪远程 main 的本地分支
git checkout -b main --track origin/main

# 2. 强制将本地 main 分支重置到和远程 origin/main 一模一样的状态
# 这会丢弃所有本地修改和提交，确保和远程完全同步
git fetch origin
git reset --hard origin/main
::===> next  和SDL2一样
###################################################
CC=cl ./configure --toolchain=msvc --enable-shared \
  --enable-libx264 --enable-gpl \
  --enable-libfdk-aac --enable-nonfree \
  --enable-libx265 --enable-sdl2 \
  --extra-cflags="-I/usr/local/include" \
  --extra-ldflags="-LIBPATH:/usr/local/lib fdk-aac.lib libx264.dll.lib libx265.lib SDL2.lib"
  
or
CC=cl ./configure --prefix=./install --toolchain=msvc --enable-shared --disable-programs --disable-ffplay --disable-ffmpeg --disable-ffprobe \
--enable-libx264 --enable-gpl --enable-libfdk-aac --enable-nonfree --enable-libx265  --enable-sdl2
###################################################


::########################################################################################
REM mingw 工具链 怎么编译FFMepg
REM pacman -S mingw-w64-x86_64-toolchain 
REM pacman -S mingw-w64-x86_64-yasm 
REM pacman -S mingw-w64-x86_64-SDL2 
REM pacman -S mingw-w64-x86_64-fdk-aac 
REM pacman -S mingw-w64-x86_64-x264 
REM pacman -S mingw-w64-x86_64-x265
REM pacman -S mingw-w64-x86_64-gcc
REM pacman -S make diffutils pkg-config git nasm