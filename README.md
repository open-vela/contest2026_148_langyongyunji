# VelaGuard

VelaGuard 是面向老人安全看护场景的 openvela 智能手表应用，运行在黄山派
SF32LB52 开发板上。设备侧集成跌倒检测、手动 SOS、语音求助提示、表盘 UI、
触摸交互和 BLE 手机联动能力。触发求助后，手表先进入本地确认/倒计时流程；
倒计时结束或用户确认后，才通过 BLE Notify 向手机 App 发送 16 字节
`CALL_REQUEST`，由 App 侧弹出紧急页或拨打绑定联系人。

## 作品方向

本项目属于 AI 硬件产品创新方向，重点验证低功耗穿戴设备上的多模态安全守护：

- IMU 跌倒检测：检测疑似跌倒后，继续确认静止状态，避免把日常甩手、拍打误判为求助。
- 手动 SOS：长按进入倒计时，倒计时结束后上报紧急事件。
- 语音辅助：采集麦克风数据，保留语音求助触发链路。
- BLE 手机协同：手表只负责事件上报，拨号、联系人、权限和后台保活由手机 App 处理。
- 手表 UI：390 x 450 屏幕，包含表盘、告警确认页、蓝牙状态页等。

## 目录结构

```text
app/hello_app/
  algorithm/        跌倒检测算法与状态机
  audio/            麦克风采集、语音活动检测和本地反馈
  comm/             BLE 通信适配、协议说明
  core/             VelaGuard 主流程、UI 调度、命令解析、事件编排
  sensors/          IMU 读取与自测
  ui/               LVGL UI 与图片/字体资源
board/contest_board/
  configs/nsh/      黄山派 NSH 配置，启用 VelaGuard、LVGL、音频、BLE 等能力
logs/               AI Coding 日志
```

## 当前实现状态

- BLE 侧使用 openvela Bluetooth framework 作为底层，不再在业务里直接维护一套
  zblue 栈生命周期。
- `velaguard_ble.c` 是 VelaGuard Service 的 framework adapter，负责注册自定义
  Service/Characteristic、广播、连接状态、CCCD、Notify、校时和事件补发。
- BLE 业务由独立 `vg_ble` task 初始化并轮询处理；Bluetooth framework 自己的
  libuv service loop 仍由官方框架管理，App task 不直接驱动框架内部 loop。
- 蓝牙 UI 开关含义是“蓝牙功能启用/强制关闭”，不是“手机已连接/未连接”。
- App 断开后设备会清连接态和 CCCD 状态；如果蓝牙功能仍启用，会重新请求广播。
- 如果紧急事件发生时 App 未连接或未订阅 event Notify，设备会保留一条 pending
  `CALL_REQUEST`，待下次连接并打开 event CCCD 后补发。

详细 BLE 协议见 [app/hello_app/comm/README.md](app/hello_app/comm/README.md)。

## 编译

在 openvela 工作区根目录执行：

```bash
cmake -S nuttx -B cmake_out/velaguard_huangshan -GNinja \
  -DBOARD_CONFIG="$PWD/contest2026_148_langyongyunji/board/contest_board/configs/nsh"
cmake --build cmake_out/velaguard_huangshan -j2
```

产物位置：

```text
cmake_out/velaguard_huangshan/nuttx.bin
```

当前启用 LVGL 图片资源、Bluetooth framework、zblue 和 mbedTLS，`nuttx.bin` 会比
最小 demo 大。功能稳定后可以再单独裁剪 LVGL demo/example、测试命令和未使用资源。

## 烧录

Linux：

```bash
./tools/sftool -c SF32LB52 -p /dev/ttyUSB0 -b 1000000 \
  --before default_reset --after soft_reset \
  write_flash cmake_out/velaguard_huangshan/nuttx.bin@0x12010000
```

Windows：

```cmd
sftool -c SF32LB52 -p COM18 -b 1000000 --before default_reset --after soft_reset write_flash "nuttx.bin@0x12010000"
```

`Verify success!` 表示 `nuttx.bin` 已写入并校验通过。若开机只打印 `SFBL/CSFBL`，
通常说明应用镜像没有被正常跳转执行，需要确认烧录地址、镜像文件和板子启动状态。

## 运行

应用会自动启动，也可以在 NSH 中手动执行：

```bash
velaguard
```

关键启动日志：

```text
VelaGuard: boot entry
VelaGuard: started. JSON events are printed as VELAGUARD_EVENT.
VelaGuard BLE: framework init start
VelaGuard BLE: GATT service registered
VelaGuard BLE: advertising ...
```

## App 联调要点

App 不需要在系统蓝牙设置中配对，直接在 App 内扫描 BLE 广播并连接。

- 扫描条件：优先按 VelaGuard Service UUID 过滤，设备名 `VelaGuard` 只作为展示或兜底。
- 多设备场景：不要写死完整 MAC；建议展示扫描到的多个 `VelaGuard` 设备，让用户选择绑定。
- 校时：连接并发现服务后，向 time characteristic 写入 8 字节 little-endian UTC
  Unix 毫秒时间戳。
- 链路探测：订阅 test characteristic Notify，写入 1 字节 probe，确认能收到每秒递增计数。
- 正常业务：订阅 event characteristic Notify；收到 `VG + version=1 + command=1`
  的 16 字节包后进入求助流程。
- 电话拨打：由手机 App 申请电话权限并拨打联系人，设备端只发送 BLE 事件，不能绕过
  手机系统权限直接拨号。

## AI Coding 使用说明

本项目使用 AI 辅助完成需求拆解、openvela 框架梳理、BLE 协议设计、跌倒算法调参、
UI 交互优化、构建问题排查和 README 整理。完整对话日志按比赛要求导出到 `logs/`
目录后提交。
