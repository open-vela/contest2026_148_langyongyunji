# VelaGuard

本目录映射到 openvela `packages/demos/contest2026_148_hello_app`。外层目录名暂保留
`hello_app` 是为了兼容比赛仓的 linkfile 和 openvela 构建映射；实际作品名和
NSH 命令名均为 VelaGuard。

当前实现是黄山派 SF32LB52 上的 VelaGuard 原生 LVGL 应用原型，NSH 命令名为：

```sh
velaguard
velaguard --imu-scan
velaguard --imu-test 50
velaguard --fall-watch 30
```

应用先跑通真实业务闭环：中文守护主界面、真实 IMU 跌倒检测、手动 SOS、倒计时确认、事件历史、设置页面，以及串口 JSON 事件输出。主界面已移除演示按钮，只显示守护状态、实时 IMU 判断依据和必要操作入口。

界面使用 `ui/velaguard_font_18.c` 专用小字库，避免 LVGL 默认字体缺少中文字符。

## 团队构建

比赛仓只保存 VelaGuard 作品代码，必须在完整的 openvela 工作区构建，不能只
单独克隆本比赛仓。先按比赛仓 manifest 同步完整工程，再从 openvela 根目录运行：

```sh
export PYTHONPATH="$PWD/prebuilts/tools/python/dist-packages/pyelftools:$PWD/prebuilts/tools/python/dist-packages/cxxfilt:$PYTHONPATH"
export PATH="$PWD/prebuilts/tools/linux/x86_64:$PATH"

cmake -B cmake_out/velaguard_huangshan -S "$PWD/nuttx" -GNinja \
  -DBOARD_CONFIG=../vendor/openvela/boards/contest2026_148_board/configs/nsh \
  -DEXTRA_FLAGS="-Wno-cpp -Wno-deprecated-declarations"

cmake --build cmake_out/velaguard_huangshan
```

这里的 `BOARD_CONFIG` 位于本队比赛仓，它复用官方
`vendor/sifli/boards/sf32lb52/lckfb_huangshan_pi` BSP，并启用 VelaGuard。
不要使用官方 BSP 目录中的 `configs/nsh` 作为 VelaGuard 构建配置，因为那会让
本队应用开关依赖未提交的本地修改。

构建产物：`cmake_out/velaguard_huangshan/nuttx.bin`。

## 目录结构

```text
hello_app/
├── core/                    # 应用主流程、LVGL 页面、安全事件状态机
│   └── velaguard_main.c
├── algorithm/               # 跌倒检测算法
│   ├── velaguard_fall.c
│   └── velaguard_fall.h
├── sensors/                 # 黄山派真实传感器接入
│   ├── velaguard_imu.c
│   └── velaguard_imu.h
├── ui/                      # UI 资源
│   └── velaguard_font_18.c
├── comm/                    # 串口上报与后续蓝牙模块规划
│   └── README.md
├── CMakeLists.txt
├── Kconfig
├── Makefile
└── README.md
```

## 架构分层

1. `core`：VelaGuard 应用层，负责 LVGL 页面、倒计时确认、SOS 告警、事件历史、串口 JSON 输出和 NSH 参数解析。
2. `sensors`：硬件感知层，负责黄山派 LSM6DSx IMU 的 I2C 读写、pinmux 切换、WHO_AM_I 探测和真实加速度/陀螺仪采样。
3. `algorithm`：端侧算法层，负责基于 IMU 的跌倒检测规则，当前使用冲击、角速度、姿态变化、后续静止窗口和置信度评分。
4. `ui`：界面资源层，当前放置中文字体资源，主界面只展示真实守护状态和必要操作入口。
5. `comm`：通信层规划，当前串口 JSON 上报已在 `core` 中跑通；后续 BLE/GATT、手机端连接和多通道上报可以迁入该目录。

## 真机验证

```sh
velaguard
velaguard --imu-scan       # 探测 LSM6DSx，正常应看到 /dev/i2c0 0x6a WHO_AM_I=0x6a
velaguard --imu-test 50    # 打印 50 组真实加速度/陀螺仪数据
velaguard --fall-watch 30  # 连续 30 秒运行真实 IMU 跌倒检测
```

开发调试时仍保留以下参数，用于不依赖硬件动作快速验证事件状态机：

```sh
velaguard --demo fall
velaguard --demo sound
velaguard --demo voice
velaguard --demo sos
```

串口事件前缀为 `VELAGUARD_EVENT`，电脑端 Mock 或 ai_agent 后续可以直接按这个前缀解析 JSON。

## 触摸与性能参数

VelaGuard 针对黄山派 390x450 屏幕做了轻量调优：

- `CONFIG_CONTEST2026_148_VELAGUARD_INPUT_POLL_MS=16`：触摸输入轮询周期。
- `CONFIG_CONTEST2026_148_VELAGUARD_LOOP_SLEEP_MAX_MS=8`：LVGL 主循环最大睡眠时间。
- 内部 250ms 小节拍轮询 `/dev/buttons`，倒计时仍按 1 秒更新。

如果真机仍觉得触摸钝，可以优先把 `INPUT_POLL_MS` 调到 `10` 或把 `LOOP_SLEEP_MAX_MS` 调到 `5`，代价是 CPU 占用会上升。

## AI 硬件赛道对齐

当前版本已经具备“事件主动 + 执行”的最小闭环：真实 IMU 跌倒或用户 SOS 触发后，设备主动进入倒计时、告警、记录和串口上报。

自定义 Skill 放在参赛仓：

```text
contest2026_148_langyongyunji/agent_skills/velaguard-safety.md
```

后续接入 ai_agent 时，将该文件安装到设备端 `/data/agent/skills/velaguard-safety.md`，让 Agent 根据 VelaGuard 事件生成摘要、写日志、震动或通知联系人。

## 下一阶段

1. 用 `tc 5` 与 `velaguard` 对比触摸响应，确认卡顿来自应用重绘还是触摸驱动。
2. 固化电脑端 Mock：监听串口，解析 `VELAGUARD_EVENT`，显示事件类型、风险、置信度和摘要。
3. 将串口 JSON 上报抽成 `comm/velaguard_report.c`，为蓝牙上报复用同一事件结构。
4. 持续记录 `--fall-watch` 与主界面真实跌倒触发数据，按误报/漏报继续调节阈值。
5. 在 ai_agent 固件或外部 Agent 中演示 `velaguard-safety` Skill，补齐赛道要求。
