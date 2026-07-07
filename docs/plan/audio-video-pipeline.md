# 音视频责任链统一规划 — docs/plan

> 目标：在现有 XTask 责任链上补齐 **音频解封装 → 解码 → 播放**，并与视频侧 **命名对称、职责清晰、第一版含 PTS 同步**。  
> 行尾 **HIL-xxx** 供 `/plan-sync` 或 Issue 跟踪（HOME 团队前缀）；**Phase → Linear Milestone**，**Task/Test → 普通 Issue**（无 Sub-issue）。  
> 实施顺序：**R → A → B → C → D → E → F → G → H**；每 Phase 可独立验收。

---

## 0. 背景与现状

| 模块 | 现状 | 缺口 |
|------|------|------|
| `XDemuxTask` | 独立线程读包；**仅转发视频包** | 音频包被丢弃 |
| `XDecodeTask` + `VideoDecoder` | 视频解码 → `AVFrame` | 无音频解码器 |
| `XDisplayTask` | 视频渲染 / OpenGL | 无音频输出 Task |
| `XAudioPlay` | SDL 播放 S16 交错 PCM | 未接入责任链 |
| `LocalPlayer` | Demux → Decode → Display | 无音频支路；强制要求视频流 |
| Observer | `RtspClient` 录流等旁路 | 不适合作为主播放链路 |

参考实现（不直接复制，仅作逻辑参考）：

- 音频 decode + swr：`src/ffmepg_open/src/main.cpp`
- 播放队列：`src/XAudioPlayTest/main.cpp`

测试素材：`assert/output.mp4`（仓库内可先由 `v1080.mp4` 复制；运行 CWD 为 `out/bin.x64`）。

---

## 1. 目标架构

### 1.1 总体数据流

```
                    ┌── XVideoDecodeTask ──► XVideoDisplayTask
                    │         (AVPacket)          (AVFrame)
XDemuxTask ─────────┤
  (读包 + 分发)      │
                    └── XAudioDecodeTask ──► XAudioPlayTask ──► XAudioPlay (SDL)
                              (AVPacket)         (AVFrame S16)
```

- **Demux 唯一读文件**；按 `codec_type` 分发，视频/音频 **并行两条链**。
- **解码层**：`VideoDecoder` / `AudioDecoder`（新建）分别封装 FFmpeg。
- **输出层**：`XVideoDisplayTask` 渲染；`XAudioPlayTask` 写声卡。
- **第一版包含 PTS 同步**（见 §5）。

### 1.2 Demux 分叉方案（已定：`setAudioNext`）

**选用：`XDemuxTask::setAudioNext(std::shared_ptr<XTask>)`**

| 方案 | 优点 | 缺点 |
|------|------|------|
| **setAudioNext（采用）** | 主播放支路语义明确；`LocalPlayer::open` 一眼可见 A/V 两链；不与录流 Observer 混淆 | 仅支持单条音频链（当前足够） |
| Observer | 可多订阅 | 主/辅链路混在一起；现有 Observer 仅 clone 视频包，需大改；后期读代码成本高 |

**约定：**

- `setNext()` → **视频**解码链入口（保持现有习惯，逐步改名为 `setVideoNext` 别名，见 §2）。
- `setAudioNext()` → **音频**解码链入口。
- `addObserver()` → **旁路**（录流、统计、调试），不用于主播放。

`process()` 伪逻辑：

```text
readPacket(pkt)
  if VIDEO  → pacing + pushPacket(video_next_) + notifyObservers(optional)
  if AUDIO  → pushPacket(audio_next_)   // 不做视频帧率 sleep
  else      → 丢弃
EOF → notifyEof()  // 视频链、音频链均收到 EOF
```

---

## 2. 命名统一（视频 + 音频对称）

### 2.1 重命名对照表

| 层级 | 现名 | 新名 | 说明 |
|------|------|------|------|
| 解封装 Task | `XDemuxTask` | **不变** | 音视频共用 |
| 视频解码器 | `VideoDecoder` | **不变** | 已广泛使用；与 `AudioDecoder` 对称 |
| 音频解码器 | — | **`AudioDecoder`** | 新建；封装 audio codec + SwrContext |
| 视频解码 Task | `XDecodeTask` | **`XVideoDecodeTask`** | 内部仍用 `VideoDecoder` |
| 视频显示 Task | `XDisplayTask` | **`XVideoDisplayTask`** | 渲染职责不变 |
| 音频解码 Task | — | **`XAudioDecodeTask`** | 新建 |
| 音频播放 Task | — | **`XAudioPlayTask`** | 新建；持有 `XAudioPlay` |
| 播放设备 | `XAudioPlay` | **不变** | 底层 SDL，非 Task |

### 2.2 兼容策略（已完成）

Phase R 重命名后曾保留 **一个版本周期的 type alias**（`using XDecodeTask = XVideoDecodeTask` 等），**Phase G 已删除**。新代码一律使用 `XVideoDecodeTask` / `XVideoDisplayTask`。

### 2.3 重命名影响范围（按优先级）

1. **必须改（主库）**：`src/XCodec/**`、`LocalPlayer`、`RtspClient`（XCodec 内）
2. **已同步**：`RtspClientTask/` 拷贝版 Task 已重命名为 `XVideoDecodeTask` / `XVideoDisplayTask`
3. **独立 demo 工程**：`ffmepg_demux_play` 等可暂不动

---

## 3. 组件设计

### 3.1 AudioDecoder

**职责**：音频 `AVCodecContext` 生命周期；`send_packet` / `receive_frame`；输出 **S16 交错** PCM（经 SwrContext）。

**接口草案**：

| 方法 | 说明 |
|------|------|
| `open(AVStream* / AVCodecParameters*)` | 创建 decoder + swr |
| `decode_packet(AVPacket&, std::vector<AVFrame*>&)` | 与 `VideoDecoder::decode_packet` 风格一致 |
| `flush()` | EOF 后冲洗 |
| `close()` | 释放 codec + swr |
| `output_sample_rate()` / `output_channels()` | 供 `XAudioPlay::open` |
| `get_stats()` | 可选：解码帧数、错误计数 |

**输出格式（固定，对齐 XAudioPlay）**：

- `AV_SAMPLE_FMT_S16`，packed（交错）
- 声道：默认 stereo（`AV_CHANNEL_LAYOUT_STEREO`）；多声道源 downmix 由 swr 配置
- 采样率：默认与源一致；若与设备不一致，swr 重采样到 `XAudioPlay::Spec.sample_rate`

**不在 AudioDecoder 内**：SDL 设备、线程、队列（交给 Task）。

### 3.2 XAudioDecodeTask

**职责**：线程；`popPacket` → `AudioDecoder` → `pushFrame`（S16 `AVFrame`）。

| 项 | 说明 |
|----|------|
| 输入 | `XDemuxTask` 经 `setAudioNext` 推送的音频 `AVPacket` |
| 输出 | S16 `AVFrame`，`nb_samples`、`pts`、`time_base` 保留 |
| 配置 | `initDecoder(AVStream*)` 或 `initDecoder(AVCodecID, AVStream*)` |
| 背压 | 下游 `getQueueSize() >= max_queue_size_` 时上游 Demux 侧 sleep（与视频链相同模式） |
| EOF | 收到 EOF 后 flush decoder，帧送尽再结束 |

### 3.3 XAudioPlayTask

**职责**：线程；`popFrame` → PTS 同步 → `XAudioPlay::push`；设备生命周期。

| 项 | 说明 |
|----|------|
| 输入 | S16 `AVFrame` |
| 设备 | `XAudioPlay::create()`；`open(Spec)` 参数来自 `AudioDecoder` 输出 |
| 启动 | 队列缓冲达到阈值后 `start()`（沿用 XAudioPlayTest：`>= 4 * 4096` 字节） |
| pause | `shouldPause()` 时不 pop / 不 push；设备 `pause()` |
| seek | `clearQueue()` + 重置时钟（与 Demux seek 联动，Phase 5） |
| 音量/倍速 | `XAudioPlay::setVolume`；倍速由 `AudioAtempoFilter`（atempo）在解码后处理，播放层固定 1.0 |

### 3.4 XDemuxTask 扩展

新增成员：

```cpp
void setAudioNext(std::shared_ptr<XTask> next);
void setVideoNext(std::shared_ptr<XTask> next);  // 可选：setNext 转调，语义更清晰
```

行为变更：

- 音频包：`++audio_packets_`，`audio_next_->pushPacket`
- 视频包：逻辑不变（pacing + `video_next_->pushPacket`）
- `stop()`：除 `next_` 外，**同时** `audio_next_->stop()`（需在 `XTask::stop` 扩展或 Demux 重写）

### 3.5 视频 Task 重命名（仅类名/文件名）

- `XDecodeTask` → `XVideoDecodeTask`（文件同步重命名）
- `XDisplayTask` → `XVideoDisplayTask`
- `DECLARE_CREATE` / `IMPLEMENT_CREATE` 宏同步
- CMake `XCodec` 源文件列表随文件名更新

---

## 4. PTS 同步（第一版必做）

### 4.1 原则

- **视频**：继续由 `XDemuxTask` 按 packet duration + `speed_` sleep（现有逻辑）。
- **音频**：由 **`XAudioPlayTask`** 按帧 PTS 决定何时 `push`（音频 packet 在 Demux 侧 **不做** 帧率 sleep）。

### 4.2 时钟

| 名称 | 含义 |
|------|------|
| `frame_pts_sec` | `av_q2d(frame->time_base) * frame->pts`（`AV_NOPTS_VALUE` 则按样本时长累加） |
| `playback_start_wall_` | 第一次 push 前记录 `steady_clock::now()` |
| `first_pts_sec_` | 第一帧有效 PTS，作为基准 |
| `speed_` | 与 `XDemuxTask::speed_` 共用或订阅同一 atomic（LocalPlayer 设置一次） |

### 4.3 等待公式

```text
target_wall = playback_start_wall_ + (frame_pts_sec - first_pts_sec_) / speed
now = steady_clock::now()
if now < target_wall:
    sleep_until(target_wall)
push PCM
```

- 若落后超过阈值（如 100ms），**追帧**：不 sleep，必要时 log warn（避免越积越慢）。
- EOF 后：flush 解码 → push 剩余 → 等待 `queuedBytes()==0`。

### 4.4 与视频 A/V 对齐（第一版范围）

- **第一版**：音频 PTS 自洽播放，时长与文件接近即可；**不要求**与视频帧像素级对齐。
- **Phase 5+**：`LocalPlayer` 以 **音频主时钟** 或 **Demux current_time_** 微调视频 pacing（文档预留，不阻塞音频 E2E）。

---

## 5. 分阶段实施计划

> 每 Phase 对应 Linear **Milestone**；下列 `Task:` / `Test:` 为**普通 Issue**（非 Sub-issue）。  
> 每 Phase 结束应可编译、可跑、可验收。

## Phase R: 视频 Task 重命名  MS: Phase R

### Feature: 视频 Task 对称命名

- [x] Task: 视频 Task 重命名（无行为变更）  HIL-62

细节：R.1~R.5 含 `XVideoDecodeTask`/`XVideoDisplayTask` 重命名、type alias（已删）、引用更新、`XCodecLocalPlayer` 回归。

**验收**：现有视频播放/RTSP 行为与重命名前一致。

## Phase A: AudioDecoder  MS: Phase A

### Feature: 音频解码器

- [x] Task: 实现 AudioDecoder（open/decode/flush/swr → S16）  HIL-64

细节：含 `ChannelLayoutWrapper` / `SwrContextWrapper` RAII；对 `assert/output.mp4` 解码验证。

**验收**：解码输出 S16 PCM，ffmpeg/ffplay 可播放。

## Phase B: XDemuxTask 音频分叉  MS: Phase B

### Feature: Demux 双链分发

- [x] Task: XDemuxTask setAudioNext 与音频包转发  HIL-66

细节：`setAudioNext`/`setVideoNext`；`stop()` 递归；`audio_packets_` 统计。

**验收**：仅接音频链时 `pushPacket` 计数与文件音频包数接近。

## Phase C: XAudioDecodeTask  MS: Phase C

### Feature: 音频解码 Task

- [x] Task: XAudioDecodeTask 责任链节点  HIL-63

细节：`initDecoder`；`Demux → XAudioDecodeTask → XAudioPlayTask` 链接。

**验收**：解码帧数/采样率日志正常；EOF flush 完整。

## Phase D: XAudioPlayTask + PTS  MS: Phase D

### Feature: 音频播放与 PTS

- [x] Task: XAudioPlayTask + 第一版 PTS 同步  HIL-61

细节：预缓冲 + `start`；§4 PTS sleep/追帧；完整音频链 E2E。

**验收**：`XAudioDemuxTest` 播放 `assert/output.mp4` 有声，时长接近文件。

## Phase E: E2E 测试工程  MS: Phase E

### Feature: 纯音频 E2E

- [x] Task: XAudioDemuxTest 工程与 CMake 注册  HIL-65
- [x] Test: 正常播完，队列归零  HIL-71
- [x] Test: pause / resume 冒烟（XCodecLocalPlayer）  HIL-73
- [ ] Test: 无音频流文件 graceful 失败  HIL-70
- [x] Test: setVolume(0.5) 听感验证（人工）  HIL-72

**验收**：测试工程可编译运行；用例覆盖主路径与 pause/resume。

## Phase F: LocalPlayer 集成  MS: Phase F

### Feature: 播放器 A/V 集成

- [x] Task: LocalPlayer 音频链集成  HIL-67
- [x] Bug: 回放切倍速未调用 XAudioPlay setSpeed  HIL-99
- [x] Task: 回放 atempo 保音调倍速（AudioAtempoFilter）  HIL-100
- [x] Bug: Seek/拖拽后音频 PTS 失步与 pipeline 未暂停  HIL-101

细节：play/pause/stop/seek/setSpeed 同步；seek flush；`XCodecLocalPlayer` 冒烟。HIL-99：`XAudioPlayTask::setSpeed` 同步 PTS 钟。HIL-100：解码链 `AudioAtempoFilter`，播放设备固定 1.0x。HIL-101：Seek flush 传播至 play task、PTS 大偏差重对齐、Seek/拖拽全 pipeline 暂停。

**验收**：`output.mp4` 音画同播；pause/resume 音画均停/续。

## Phase G: 清理与文档  MS: Phase G

### Feature: 收尾

- [x] Task: 清理 type alias 与 RtspClientTask 同步  HIL-69

细节：删除 alias；`RtspClientTask` 类名/文件名同步；更新本规划文档。

## Phase H: XView 预览/录制音频集成  MS: Phase H

### Feature: RecordClient 录制音频

- [x] Task: RecordClient 录制音频 remux 支路（XAudioRemuxTask + XMuxerTask）  HIL-91
- [x] Bug: XMuxerTask 关键帧前音频积压与非单调 DTS  HIL-92

细节：主码流视频 decode→encode，音频 AAC remux 直通；关键帧前丢弃音频、DTS 单调、队列上限 128。

**验收**：新录制 MP4 含 AAC 轨（见 HIL-95）。

### Feature: XPlayVideo 回放 UI

- [x] Task: 音量/进度条布局修复  HIL-96

**验收**：进度条占满剩余宽度；无音频时音量禁用；回放窗口缩放时视频区域保持比例适配，窄窗口下控件不挤压。

### Feature: XView 预览音频

- [x] Task: RtspClient 预览按需音频与 XViewer 互斥  HIL-94
- [x] Task: 预览拖拽默认开启声音  HIL-97
- [x] Bug: 关闭软件时预览 cameraReleased 析构崩溃  HIL-98
- [ ] Test: 预览默认有声无 PTS 追帧  HIL-93

细节：`enableAudio()` lazy 挂载；预览/回放互斥；首帧就绪自动开声 + 多窗声音独占。

**验收**：拖拽预览即有声音，无长期「PTS 落后」追帧日志。

### Feature: 录制回放 E2E

- [ ] Test: 录制 MP4 含 AAC 回放有声  HIL-95

---

## 6. 测试与资产

| 资产 | 路径 | 说明 |
|------|------|------|
| 主测试文件 | `assert/output.mp4` | 本地可从 `v1080.mp4` 复制 |
| 无音频对照 | `400_300_25.mp4` 等 | 测 graceful 失败 |
| 运行目录 | `out/bin.x64` | VS `VS_DEBUGGER_WORKING_DIRECTORY` |

---

## 7. 风险与约束

| 风险 | 缓解 |
|------|------|
| 重命名波及多工程 | Phase R 限 XCodec + alias；Phase G 再清 alias |
| 双链 EOF 时序 | Demux EOF 后两链分别 flush；AudioPlayTask 等 `queuedBytes==0` |
| PTS 缺失 | 用样本累计伪 PTS；日志标记 |
| planar 格式 | 只在 `AudioDecoder` swr 输出 S16 packed |
| 播放层 `setSpeed` 与 Demux speed 叠加 | Demux 控包速；`AudioAtempoFilter` 保音调变速；`XAudioPlayTask` 仅 PTS 墙钟调度 |

---

## 8. 不在本规划内（后续）

- 视频 pacing 与音频主时钟硬同步
- `AudioDecoder` 硬件加速
- 多音轨 / 字幕

---

## 9. 决策记录

| 日期 | 决策 |
|------|------|
| 2026-06-04 | 音频/视频解码 **Task 分离**；新增 `AudioDecoder` |
| 2026-06-04 | Demux 主音频支路用 **`setAudioNext`**，Observer 仅旁路 |
| 2026-06-04 | 视频 Task 重命名为 **`XVideoDecodeTask` / `XVideoDisplayTask`** |
| 2026-06-04 | **第一版包含音频 PTS 同步**（在 `XAudioPlayTask`） |
| 2026-06-04 | 实施顺序：**R → A → B → C → D → E → F → G** |

---

## 10. 下一步（当前）

**Phase H 开发项已完成（本地未提交）**；待验收：

1. **HIL-95** — 新录制 MP4 回放有声
2. **HIL-93** — 预览默认有声、无 PTS 追帧
3. **HIL-70**（Phase E）— 无音频流 graceful 失败
