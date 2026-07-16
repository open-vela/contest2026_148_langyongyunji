# VelaGuard Comm

本目录用于放置 VelaGuard 的事件上报与外部通信能力。

当前版本已经在 `core/velaguard_main.c` 中实现串口 JSON 上报，事件统一以
`VELAGUARD_EVENT` 为前缀输出，便于电脑端 Mock、手机端工具或后续 Agent 解析。

后续蓝牙接入建议在本目录新增：

- `velaguard_ble.c` / `velaguard_ble.h`：BLE 广播、连接和 GATT 服务。
- `velaguard_report.c` / `velaguard_report.h`：统一封装串口、BLE、Mock 等上报通道。

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
