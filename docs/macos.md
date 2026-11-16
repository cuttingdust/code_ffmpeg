# macOS 开发说明

## 环境与依赖

需要 Xcode Command Line Tools 或完整 Xcode、CMake、Git、vcpkg，以及 Qt 6。
基线更新脚本还需要 Python 3。使用 Xcode 生成器时需要完整 Xcode。

```bash
export VCPKG_ROOT="$HOME/vcpkg" # 改成自己的 vcpkg 仓库路径
bash src/vcpkg_install.sh
# Qt 不在默认搜索路径时，配置时传入对应的安装前缀。
bash src/update.sh -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x/macos
cmake --build src/build --parallel 4
```

安装脚本按当前系统及进程架构选择 `arm64-osx` 或 `x64-osx`，
依赖安装到 `src/build/vcpkg_installed`。Rosetta、vcpkg、Qt 和 CMake 的目标
架构应保持一致。Qt 的实际安装位置取决于安装方式，不要照抄示例路径。

## IDE 与清理

已有构建目录沿用缓存中的生成器；Unix Makefiles/Ninja 不能用 `cmake --open`
打开 IDE。需要 Xcode 工程时，从空构建目录配置：

```bash
# 会删除整个 src/build，包括 vcpkg_installed，需要重新恢复依赖。
bash src/clean.sh -G Xcode -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x/macos
bash src/open.sh
```

`update.sh` 和 `clean.sh` 只配置工程，不执行项目编译。
CMake 的 vcpkg 工具链默认可能自动恢复依赖；临时传入
`-DVCPKG_MANIFEST_INSTALL=OFF` 可在依赖已齐全时跳过恢复，
需要重新启用时传 `-DVCPKG_MANIFEST_INSTALL=ON`。这个选项会保存在本地缓存。

## 平台差异与验证边界

- Direct3D9 解码示例和使用 Win32 事件的 `timerFrame_test` 只在 Windows 构建。
- MSVC 链接参数与 `windeployqt.exe` 部署脚本只用于对应 Windows 环境。
- Apple Silicon 原生构建的可执行文件输出到 `out/bin.arm64`；库仍在 `out/lib`。
  不同系统/架构请使用独立工作目录，避免共享 `build`、`out/lib`。
- FFmpeg 使用查找模块提供的完整库列表，以携带静态依赖、系统框架以及
  Debug/Release 对应路径。
- 部分示例仍优先尝试 D3D11VA，自动选择/软件回退路径和 VideoToolbox
  需要实际媒体与设备测试；编译通过不代表硬件解码已经验证。
- 部分示例用 `clock()` 计算帧率或超时，它测量 CPU 时间，不能可靠代替播放
  所需的实际经过时间；后续应改用 `std::chrono::steady_clock` 并验证播放节奏。
- 多个示例按相对路径读取测试媒体，启动时需确认工作目录及媒体文件。
- Bash 脚本是开发入口，不能保证项目所有示例在 macOS 上的运行行为一致。

稀疏检出只负责隐藏另一平台的脚本，不负责 C++/CMake 兼容性：
`bash sparse-checkout.sh` 启用，`bash sparse-checkout.sh --disable` 恢复。

## 本次验证

在 Apple Silicon macOS、Apple Clang、Unix Makefiles 和现有 `arm64-osx`
依赖环境中，CMake 配置和默认全部目标构建通过（不包含上述 Windows 专用示例）。
验证时关闭 vcpkg 自动安装，复用已有依赖；仍有编译警告及重复库链接警告。
另外已验证 MediaMTX/FFmpeg 的本机 RTSP 推流与解码，以及 XView 修复后的实际预览。
XView 在 QApplication 创建前显式请求 OpenGL 4.1 Core，并适配 Core 着色器语法，
解决默认 2.1 上下文造成的黑屏；固定测试帧的 framebuffer 像素检查通过。
录像完整流程、硬件解码和应用打包尚未验证，也未在 Windows/Linux 上回归。
