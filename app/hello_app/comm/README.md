# VelaGuard Comm

本目录用于放置 VelaGuard 的事件上报与外部通信能力。

当前版本实现了串口 JSON 和 BLE 紧急呼叫传输。疑似跌倒和手动 SOS 阶段先给用户
取消窗口；倒计时结束、用户点击确认，或者语音层识别到“救命/求助”后，设备才
向已连接手机发送 `CALL_REQUEST`。

BLE 设备名为 `VelaGuard`，自定义服务和事件特征 UUID 为：
手机连接后还需要写入一次校时特征，用手机当前 UTC 时间校准设备 RTC：

```text
service  6f70656e-7665-4c61-9361-726456470001
event    6f70656e-7665-4c61-9361-726456470002 (read + notify)
time     6f70656e-7665-4c61-9361-726456470003 (read + write)
test     6f70656e-7665-4c61-9361-726456470004 (read + write + notify)
```

MVP 使用 App 内绑定：手机主动扫描后直接建立 GATT 连接并订阅 Notify，不要求在
Android 系统蓝牙页面配对，也不要求加密连接。

连接流程建议：

1. 扫描并连接 `VelaGuard`。
2. 发现 service `...0001`。
3. 向 time characteristic `...0003` 写入 8 字节 little-endian
   `unix_epoch_ms`，必须是 UTC Unix 毫秒时间戳。
4. 订阅 event characteristic `...0002` 的 Notify。
5. 收到 `CALL_REQUEST` 后，读取本地绑定的紧急联系人并调用系统拨号能力。

基础链路调试时先不要触发 SOS/跌倒，可以只验证 test characteristic：

1. 订阅 test characteristic `...0004` 的 Notify。
2. 手机向 `...0004` 写入任意 1 字节，设备串口应打印
   `VelaGuard BLE TEST: write ...`。
3. 订阅成功后，设备每秒 Notify 一个 4 字节 little-endian `uint32` 递增计数，
   手机应能连续收到 `1, 2, 3...`。

订阅成功以 CCC 写入成功为准；设备不会在刚订阅时发送空事件或心跳事件。只有手动
SOS 倒计时结束、跌倒确认或语音 SOS 确认后，event characteristic 才会 Notify
`CALL_REQUEST`。

事件特征发送固定 16 字节小端数据包：`VG`、协议版本、命令、事件类型、风险、
置信度、确认标志、event_id、uptime_ms。手机端订阅通知，收到命令 1 后读取本地
绑定的紧急联系人并调用系统拨号能力。手机应用必须自行申请 BLE 和电话权限；
黄山派不能绕过手机系统权限直接拨号。

手机端解析方式：

```text
offset  size  value
0       2     magic: "VG"
2       1     version: 1
3       1     command: 1 = CALL_REQUEST
4       1     event_type: 1=manual_sos, 2=fall, 3=voice_sos
5       1     risk: 0..5
6       1     confidence: 0..100
7       1     flags: bit0=user_confirmed
8       4     event_id, little-endian uint32
12      4     uptime_ms, little-endian uint32
```

App 侧不要等待设备再发 JSON；BLE notify 收到上述二进制包且
`magic=="VG" && version==1 && command==1` 时，就可以弹确认页或直接拨打
用户绑定的紧急联系人。

推荐事件负载保持与当前串口 JSON 字段一致：

```json
{
  "app": "VelaGuard",
  "id": 1001,
  "phase": "suspected",
  "type": "fall_suspected",
  "uptime_ms": 123456,
  "risk": 3,
  "confidence": 78,
  "summary": "..."
}
```
