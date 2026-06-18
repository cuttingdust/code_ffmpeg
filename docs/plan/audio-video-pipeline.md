# 音视频责任链统一规划 — docs/plan

> 目标：在现有 XTask 责任链上补齐 **音频解封装 → 解码 → 播放**，并与视频侧 **命名对称、职责清晰、第一版含 PTS 同步**。  
> 行尾 **HIL-xxx** 供 `/plan-sync` 或 Issue 跟踪（HOME 团队前缀）；实施时按 Phase 顺序推进，**每 Phase 可独立验收**。  
> Epic：**HIL-54** [音视频责任链统一规划](https://linear.app/hildness/issue/HIL-54)

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

### 3.1 AudioDecoder  HIL-59

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

### 3.2 XAudioDecodeTask  HIL-56

**职责**：线程；`popPacket` → `AudioDecoder` → `pushFrame`（S16 `AVFrame`）。

| 项 | 说明 |
|----|------|
| 输入 | `XDemuxTask` 经 `setAudioNext` 推送的音频 `AVPacket` |
| 输出 | S16 `AVFrame`，`nb_samples`、`pts`、`time_base` 保留 |
| 配置 | `initDecoder(AVStream*)` 或 `initDecoder(AVCodecID, AVStream*)` |
| 背压 | 下游 `getQueueSize() >= max_queue_size_` 时上游 Demux 侧 sleep（与视频链相同模式） |
| EOF | 收到 EOF 后 flush decoder，帧送尽再结束 |

### 3.3 XAudioPlayTask  HIL-55

**职责**：线程；`popFrame` → PTS 同步 → `XAudioPlay::push`；设备生命周期。

| 项 | 说明 |
|----|------|
| 输入 | S16 `AVFrame` |
| 设备 | `XAudioPlay::create()`；`open(Spec)` 参数来自 `AudioDecoder` 输出 |
| 启动 | 队列缓冲达到阈值后 `start()`（沿用 XAudioPlayTest：`>= 4 * 4096` 字节） |
| pause | `shouldPause()` 时不 pop / 不 push；设备 `pause()` |
| seek | `clearQueue()` + 重置时钟（与 Demux seek 联动，Phase 5） |
| 音量/倍速 | 转发 `XAudioPlay::setVolume`；**播放层 setSpeed 仍视为临时**；长期由 Demux/atempo 驱动 |

### 3.4 XDemuxTask 扩展  HIL-58

新增成员：

```cpp
void setAudioNext(std::shared_ptr<XTask> next);
void setVideoNext(std::shared_ptr<XTask> next);  // 可选：setNext 转调，语义更清晰
```

行为变更：

- 音频包：`++audio_packets_`，`audio_next_->pushPacket`
- 视频包：逻辑不变（pacing + `video_next_->pushPacket`）
- `stop()`：除 `next_` 外，**同时** `audio_next_->stop()`（需在 `XTask::stop` 扩展或 Demux 重写）

### 3.5 视频 Task 重命名（仅类名/文件名）  HIL-57

- `XDecodeTask` → `XVideoDecodeTask`（文件同步重命名）
- `XDisplayTask` → `XVideoDisplayTask`
- `DECLARE_CREATE` / `IMPLEMENT_CREATE` 宏同步
- CMake `XCodec` 源文件列表随文件名更新

---

## 4. PTS 同步（第一版必做）  HIL-60

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

> 每 Phase 结束应可编译、可跑、可验收；避免跨 Phase 大块未测代码。

### Phase R — 视频 Task 重命名（无行为变更）  HIL-62

- [x] R.1 `XDecodeTask` → `XVideoDecodeTask`（类名、文件名、CREATE 宏）
- [x] R.2 `XDisplayTask` → `XVideoDisplayTask`
- [x] R.3 添加 type alias `XDecodeTask` / `XDisplayTask`（过渡期）
- [x] R.4 更新 `LocalPlayer`、`RtspClient`、`RecordClient`、`XMediaClient` 引用
- [x] R.5 编译通过；`XCodecLocalPlayer` 回归（视频仍正常）

**验收**：现有视频播放/RTSP 行为与重命名前一致。

---

### Phase A — AudioDecoder  HIL-64

- [x] A.1 新增 `AudioDecoder.h/.cpp`（XCodec）
- [x] A.2 `open` + `decode_packet` + `flush` + swr → S16 stereo
- [x] A.3 单元级验证：对 `assert/output.mp4` 解码（Phase E `XAudioDemuxTest` 验收）

**验收**：对 `assert/output.mp4` 解码输出 S16 PCM 文件，ffmpeg/ffplay 可播放。

---

### Phase B — XDemuxTask 音频分叉  HIL-66

- [x] B.1 `setAudioNext` / `audio_next_`；`stop()` 递归停止音频链
- [x] B.2 `process()` 转发音频包；统计 `audio_packets_`
- [x] B.3 `setVideoNext` 别名（`setNext` 语义别名）
- [x] B.4 编译 + 音频包计数验证（`XAudioDemuxTest` demux_stats.audio_packets）

**验收**：临时测试代码仅接音频链，`pushPacket` 计数与文件音频包数一致（approx）。

---

### Phase C — XAudioDecodeTask  HIL-63

- [x] C.1 新建 Task；`initDecoder`；`process` 循环
- [x] C.2 链：`demux->setAudioNext(audio_decode)`；`audio_decode->setNext(audio_play)`（见 XAudioDemuxTest）

**验收**：日志打印解码帧数、采样率；无 crash；EOF flush 完整。

---

### Phase D — XAudioPlayTask + PTS  HIL-61

- [x] D.1 新建 Task；持有 `XAudioPlay`；预缓冲 + `start`
- [x] D.2 实现 §4 PTS sleep / 追帧
- [x] D.3 完整链：`Demux → XAudioDecodeTask → XAudioPlayTask`（XAudioDemuxTest）

**验收**：`XAudioDemuxTest` 播放 `assert/output.mp4` **有声**；播放时长与文件接近。

---

### Phase E — E2E 测试工程  HIL-65

- [x] E.1 新建 `src/XAudioDemuxTest/`（CMake：`XCodec`）
- [x] E.2 `main`：组装责任链；fallback `v1080.mp4`
- [x] E.3 `src/CMakeLists.txt` 注册

#### TC: 纯音频 E2E  HIL-68

- [x] TC.1 正常播完，队列归零  HIL-71
- [x] TC.2 pause / resume（Task + 设备）— `XCodecLocalPlayer` 冒烟  HIL-73
- [ ] TC.3 无音频流文件 graceful 失败  HIL-70
- [ ] TC.4 `setVolume(0.5)` 可听感验证（人工）  HIL-72

---

### Phase F — LocalPlayer 集成  HIL-67

- [x] F.1 有音频流时创建并启动音频链；**无视频流时不强制失败**（可选：纯音频文件）
- [x] F.2 `play/pause/stop/seek/setSpeed` 同步到音频 Task + `XAudioPlay`
- [x] F.3 seek：`clearQueue`、flush decoder、重置 PTS 时钟（`XDemuxTask::seek` 已含音频链）
- [x] F.4 `XCodecLocalPlayer` 冒烟测试通过

**验收**：`output.mp4` 同时有画面和声音；pause/resume 音画均停/续。

---

### Phase G — 清理与文档  HIL-69

- [x] G.1 删除 type alias（确认无引用）
- [x] G.2 同步 `RtspClientTask` 拷贝模块（类名 + 文件名）
- [x] G.3 更新本规划文档勾选状态

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
| 播放层 `setSpeed` 与 Demux speed 叠加 | LocalPlayer 统一只调 Demux speed；AudioPlay speed 固定 1.0 直至 atempo 接入 |

---

## 8. 不在本规划内（后续）

- FFmpeg `atempo` 滤镜（保音调倍速）
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

**下一步**：**HIL-70** 无音频流 graceful 失败；**HIL-72** setVolume 听感验证（人工）。
