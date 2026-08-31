# VelaGuard BLE Protocol

本目录维护 VelaGuard 的外部通信能力。当前 BLE 实现基于 openvela Bluetooth
framework，`velaguard_ble.c` 只作为 VelaGuard 自定义 GATT Service 的 adapter：
负责注册特征值、广播、连接状态、CCCD、Notify、校时和事件补发，不直接管理一套
私有蓝牙协议栈。

## 角色与状态

- 设备角色：BLE Peripheral / GATT Server。
- 设备名：`VelaGuard_<MAC 最后四位>`，例如 `VelaGuard_3412`。
- App 角色：BLE Central / GATT Client。
- 配对要求：MVP 不要求系统蓝牙页面配对，不要求加密连接。
- 蓝牙 UI 开关：打开表示设备侧蓝牙功能启用；关闭表示强制关闭蓝牙、停止广播并断开当前连接。
- 连接状态：是否已连接手机单独显示，不能用来替代开关状态。

设备端启动流程：

```text
velaguard main loop
  -> vg_ble_init()
  -> Framework instance / adapter callback setup (only once)
  -> framework enable
  -> wait adapter ready
  -> register VelaGuard GATT service
  -> start advertising
```

正常断开流程：

```text
phone connects
  -> defer Framework advertising-object cleanup to VelaGuard main loop
  -> release the legacy advertising slot, even though the controller has
     already stopped advertising for the connection
phone disconnect
  -> clear connected / CCCD state
  -> keep service registered
  -> schedule advertising restart if BLE switch is on
```

普通断线重连不会重新注册 Service，也不会重复初始化 Bluetooth framework。
设备会等待控制器完成链路拆除、并确认上一个 advertising slot 已释放后再恢复广播；这避免
legacy 广播在连接后自动停止却仍占用 Framework slot，导致后续断线无法重新被扫描。若
Framework 未返回广播启动结果，3 秒后会先释放该请求的 slot，再按退避策略重试。

SOS 的本地告警页面与 BLE `CALL_REQUEST` 上报不依赖麦克风采集。当前配置保留 WAV
扬声器提示，关闭 ADC 麦克风采集和语音触发；这不影响服务发现、状态心跳、校时或紧急事件
上报。恢复采集时，应用会在播放 WAV 前暂停采集、收到播放完成消息后再恢复，避免两个流
同时争用 SF32LB52 的音频消息队列和 DMA。

## Framework 使用约束

`velaguard_ble.c` 只做业务适配，调用 openvela Bluetooth Framework 的公开接口：

- 只保留一次 `vg_ble_init()`、Framework instance、adapter callback 和 GATT Service
  注册；重连不重新初始化、不重复注册。
- 不直接调用 zblue 私有 API 或 `bt_enable()`，不直接操作 H4 UART、controller、ring
  buffer，也不在应用层实现 Framework 的事件循环。
- Framework 的接收、ATT/GATT 请求和 HCI 处理由官方框架线程负责；VelaGuard 主任务只处理
  一次性初始化后的业务状态、连接状态、心跳和待发送事件。回调中只更新状态/入队，耗时操作
  不放入框架回调。
- 设备是 BLE Peripheral/GATT Server，App 是 Central/GATT Client。只打开 BLE 外设所需
  配置，避免启用无业务需要的 BR/EDR、GATT Client、扫描和测试命令。
- `vendor/sifli`、NuttX 和 Framework 的底层改动不能直接提交到主仓库；必须放到
  `contest2026_148_langyongyunji/patches/`，由 `scripts/apply_patches.sh` 统一应用。
  `0007`、`0008` H4 patch 属于已验证链路，必须保留并在编译日志中确认已应用。
  `0010` 将 HCPU 尾部的 mailbox 内存排除在 NuttX 堆之外，避免 BLE IPC 覆盖堆元数据；
  `0011` 负责 TX ring 边界、D-cache 和一次性恢复保护。H4 逐包诊断补丁不在发布链中。
  `framework/0002` 为单 service
  GATT Server 提供无链表的 ATT service 查找保护；`framework/0003` 使 Peripheral-only
  设备的 LE identity address 能通过 Framework 的本机地址查询接口返回。

当出现“可扫描/可连接，但 MTU 或服务发现超时”时，先记录以下完整链路：

```text
App connected
-> device H4 RX received ATT request
-> Framework/ATT produced response
-> device H4 TX sent response
-> App callback completed
```

不要先改 UUID、强制提高 MTU 或增加重复初始化。若 H4 RX 已有而 TX 没有，检查官方
Framework 调度、H4 UART 接收线程和 TX ring；若 TX 已发而手机无回调，再对照 App 和空口
抓包。当前固件已验证 App 连接、服务发现与状态心跳正常。发生新的链路异常时，可临时将
`SF32LB52_BT_TRACE` 和 `SF32LB52_BT_ACL_STAGE_TRACE` 设为 `1` 收集一次完整链路日志；
定位后必须恢复为 `0`，避免日志影响实时性。

## UUID

```text
service  6f70656e-7665-4c61-9361-726456470001
event    6f70656e-7665-4c61-9361-726456470002  read + notify
time     6f70656e-7665-4c61-9361-726456470003  read + write
status   6f70656e-7665-4c61-9361-726456470005  read + notify
```

广播数据包含 VelaGuard Service UUID；扫描响应中包含设备名 `VelaGuard`。App 应优先
按 Service UUID 扫描过滤，设备名只作为展示或兜底。

## App 连接顺序

推荐 App 侧按下面顺序接入：

1. 扫描广播，按 Service UUID 过滤 `...0001`。
2. 展示扫描到的多个 VelaGuard 设备，由用户选择绑定；不要写死完整 MAC。
3. 建立 GATT 连接。
4. 发现 Service 和 Characteristic。
5. 向 time characteristic `...0003` 写入 UTC Unix 毫秒时间戳。
6. 订阅 status characteristic `...0005` Notify，接收每秒设备状态心跳。
7. 订阅 event characteristic `...0002` Notify。
8. 等待 event Notify 的 16 字节 `CALL_REQUEST`。

## 校时

手机连接后应写入一次校时数据：

```text
characteristic: time ...0003
payload: 8 bytes little-endian uint64
value: UTC Unix epoch milliseconds
```

示例：App 当前时间为 `epoch_ms = 1787822198407`，按小端写入 8 字节。设备收到后会在
主 UI 流程中执行 `clock_settime(CLOCK_REALTIME, ...)`，避免在蓝牙回调里直接改系统时间。

注意：设备端保存和接收的是 UTC 时间戳。表盘显示本地时间时需要按目标时区换算，目前
项目面向中国区展示，按 UTC+8 显示。

## Event Characteristic

event `...0002` 是正式业务通道。设备只在确认需要求助时发送 Notify：

- 手动 SOS：长按进入倒计时，倒计时结束后发送。
- 跌倒检测：疑似跌倒后进入确认流程，满足跌倒后静止约 5 秒并进入告警阶段后发送。
- 语音 SOS：语音层确认求助后发送。

设备不会在刚订阅 event 时发送空事件或心跳事件。App 收到二进制包后自行决定弹紧急页、
倒计时确认或直接拨打绑定联系人。

## Status Heartbeat Characteristic

status `...0005` 每秒发送 1 字节：

```text
0 = 当前正常，未处于跌倒告警
1 = 当前处于跌倒告警状态
```

状态 `0 -> 1` 时，设备同时通过 event `...0002` 发送一次 16 字节
`CALL_REQUEST`。后续每秒的 status `1` 只是心跳，不是新的跌倒事件，App 不得重复弹窗、
拨号或创建告警。设备取消告警或恢复正常后发送 status `0`。

App 应记录最后一次 status 心跳时间；连续约 3 秒未收到心跳时，显示设备连接异常。status
通知失败不应触发新的跌倒事件。

## CALL_REQUEST 16 字节协议

所有多字节字段均为 little-endian。

```text
offset  size  value
0       2     magic: "VG"
2       1     version: 1
3       1     command: 1 = CALL_REQUEST
4       1     event_type: 1=manual_sos, 2=fall, 3=voice_sos
5       1     risk: 0..5
6       1     confidence: 0..100
7       1     flags: bit0=user_confirmed
8       4     event_id, uint32
12      4     uptime_ms, uint32
```

App 侧判断条件：

```text
magic == "VG" && version == 1 && command == 1
```

满足后进入求助流程。不要等待设备额外发送 JSON；串口 JSON 只用于开发调试。

## 断线与补发

如果事件发生时 App 未连接，或 App 尚未写 event CCCD 打开 Notify，设备会保留一条
pending `CALL_REQUEST`。下次 App 连接并订阅 event Notify 后，设备会补发该事件。

设备端只保留最近一条 pending 紧急事件，避免断线期间事件队列无限增长。App 侧收到
同一个 `event_id` 应做幂等处理，避免重复拨号或重复弹窗。

## App 权限与拨号

黄山派设备端不会也不能直接拨打电话。App 需要自行处理：

- BLE 扫描、连接、后台权限。
- 电话权限。
- 紧急联系人绑定。
- 收到 `CALL_REQUEST` 后的弹窗、倒计时、拨号或取消逻辑。
- 多设备绑定和设备别名展示。

建议 App 保存绑定关系时记录：

```text
service_uuid
device_name
last_seen_address
user_alias
last_connected_time
```

不要只依赖完整 MAC 作为唯一扫描条件；如果底层地址策略变化，固定 MAC 会导致无法发现设备。

## 调试日志

建议 App 和设备联调时对齐以下日志点：

```text
VelaGuard BLE: framework init start
VelaGuard BLE: adapter state=...
VelaGuard BLE: GATT service registered
VelaGuard BLE: advertising ...
VelaGuard BLE: phone connected ...
VelaGuard BLE: time sync queued ...
VelaGuard BLE: time synced ...
VelaGuard BLE: event CCC notify=enabled
VelaGuard BLE: CALL_REQUEST packet=...
VelaGuard BLE: CALL_REQUEST id=... result=...
VelaGuard BLE: phone disconnected ...
```

如果连接后服务发现超时，优先确认广播已启动、Service UUID 是否一致、App 是否写 CCCD、
连接后是否发生断开，以及设备端是否出现 HardFault 或 Bluetooth framework 初始化失败。
