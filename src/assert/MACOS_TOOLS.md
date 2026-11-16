# macOS 测试工具

本目录额外放置 macOS Apple Silicon 原生的 `mediamtx`、`ffmpeg`，
与 Windows 的同名 `.exe` 并存。已验证当前 Mac 可执行，Intel Mac 需要另取对应版本。

先启动服务，再在另一个终端推流：

```bash
bash src/assert/mock_rtsp_mediamtx.sh
bash src/assert/mock_rtsp_ffmpeg.sh
```

默认循环推送 `400_300_25.mp4` 到 `rtsp://127.0.0.1:8554/main`。
可传自定义视频与地址，例如：

```bash
bash src/assert/mock_rtsp_ffmpeg.sh /path/to/video.mp4 rtsp://127.0.0.1:8554/test
```

两个入口均在前台运行，Ctrl+C 结束；启动服务不会批量结束其他进程。
沿用已有 `mediamtx.yml`，下载包中的默认配置没有覆盖项目配置。

## 来源与校验

- MediaMTX `v1.21.4`：官方 GitHub 的 `mediamtx_v1.21.4_darwin_arm64.tar.gz`。
  https://github.com/bluenviron/mediamtx/releases/tag/v1.21.4
  压缩包 SHA-256：`20dfa23c1db149b935c2f23439ffbf0820a0ac1d6979a6eb9b21f74fc8376647`。
  授权文本见 `LICENSE.mediamtx`。
- FFmpeg `9.0.2`：OSXExperts 提供的 Apple Silicon 静态构建（第三方构建）。
  https://www.osxexperts.net/ffmpeg902arm.zip
  可执行文件 SHA-256：`6deb2860c3dc0c08e0f96bab119196c024103046fa3cc7ef72e3a77797d8553c`。
  构建来源及编译选项：https://www.osxexperts.net/ 。

程序放在项目本地，Git 忽略这些无扩展名的二进制，与现有 `.exe` 忽略策略一致。
脚本和说明可跟踪；复制本项目到另一台电脑时需一并复制二进制或按上述来源重新下载。

## Clumsy 的替代方案

已下载 Toxiproxy `2.13.0` 的 macOS ARM64 服务端和 CLI 到本目录：
`toxiproxy-server`、`toxiproxy-cli`。二者均已通过版本命令检查，程序在 Git 中忽略。
压缩包来自 https://github.com/Shopify/toxiproxy/releases/tag/v2.13.0 ，
文件名 `toxiproxy_2.13.0_darwin_arm64.tar.gz`，SHA-256 为
`0890e2eef1ed4d269411a8eca591112b7401f61b04c7abc5c7d0cb2da33e2484`。
MIT 授权文本见 `LICENSE.toxiproxy`。

前台启动管理服务（Ctrl+C 停止）：

```bash
./src/assert/toxiproxy-server -host 127.0.0.1 -port 8474
```

另开终端创建 RTSP 代理：

```bash
./src/assert/toxiproxy-cli create -l 127.0.0.1:18554 -u 127.0.0.1:8554 rtsp
```

创建代理后客户端使用 TCP 连接 `rtsp://127.0.0.1:18554/main`。
MediaMTX 和 FFmpeg 推流仍使用原来的 `8554`，避免把发布流也送进弱网代理。
本次仅下载并验证版本，没有后台启动服务或自动添加弱网规则。

- **Toxiproxy**：跨平台的 TCP 代理，可以模拟延迟、限速、超时和断连。
  https://github.com/Shopify/toxiproxy
  适合本机 RTSP-over-TCP；例如代理监听 `18554`，转发到本机 MediaMTX 的 `8554`，
  客户端改连 `rtsp://127.0.0.1:18554/main`，并强制使用 TCP 承载 RTP。
  它不能模拟 RTP-over-UDP 的逐包丢失或乱序，应用层 TCP 故障也不等同于真实 IP 丢包。
- **Network Link Conditioner**：Apple 的网络条件模拟工具，随 Additional Tools for Xcode
  提供，适合需要图形界面的场景。不要假定它必然覆盖 `127.0.0.1` 的 RTSP 流量；
  本机回环场景需实际验证，且它与 Clumsy 的按端口筛选能力不同。
  https://developer.apple.com/download/all/
- **Linux tc/netem**：需要可控的 UDP 丢包、延迟、重复或乱序时，可在 Linux
  虚拟机/测试机的实际网络路径上模拟；先设计好流量经过哪个接口，再应用规则。

当前未启用弱网规则，没有修改系统防火墙、全局网络或其他进程。
