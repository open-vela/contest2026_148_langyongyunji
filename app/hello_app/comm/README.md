# VelaGuard Comm

本目录用于放置 VelaGuard 的事件上报与外部通信能力。

当前版本实现了串口 JSON 和 BLE 紧急呼叫传输。疑似跌倒阶段只询问用户；
倒计时结束、用户点击确认、手动 SOS，或者语音层识别到“救命/求助”后，设备才
向已连接手机发送 `CALL_REQUEST`。

BLE 设备名为 `VelaGuard`，自定义服务和事件特征 UUID 为：

```text
service  6f70656e-7665-4c61-9361-726456470001
event    6f70656e-7665-4c61-9361-726456470002 (read + notify)
```

MVP 使用 App 内绑定：手机主动扫描后直接建立 GATT 连接并订阅 Notify，不要求在
Android 系统蓝牙页面配对，也不要求加密连接。

事件特征发送固定 16 字节小端数据包：`VG`、协议版本、命令、事件类型、风险、
置信度、确认标志、event_id、uptime_ms。手机端订阅通知，收到命令 1 后读取本地
绑定的紧急联系人并调用系统拨号能力。手机应用必须自行申请 BLE 和电话权限；
黄山派不能绕过手机系统权限直接拨号。

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
