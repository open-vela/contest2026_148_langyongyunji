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
  -> create vg_ble task (only once)
  -> vg_ble task: framework init/enable
  -> wait adapter ready
  -> register VelaGuard GATT service
  -> start advertising
```

正常断开流程：

```text
phone disconnect
  -> clear connected / CCCD state
  -> keep service registered
  -> schedule advertising restart if BLE switch is on
```

普通断线重连不会重新注册 Service，也不会重复初始化 Bluetooth framework。

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
