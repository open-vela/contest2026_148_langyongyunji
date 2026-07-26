# VelaGuard 手机端开发规格（MVP）

## 1. 目标流程

1. 首次打开 App，申请蓝牙权限并让用户选择一个紧急联系人。
2. 主动扫描设备名 `VelaGuard`，直接建立 BLE GATT 连接并自动重连；MVP 不走系统
   “蓝牙配对”页面，也不要求配对/绑定。
3. 订阅 Event Characteristic 的 Notify。
4. 收到合法 `CALL_REQUEST` 后立即展示全屏紧急提示，震动并播放提示音。
5. 默认等待 3 秒，用户可点“取消”；没有取消则拨打已绑定联系人。
6. 将事件、拨号结果和时间写入本地历史。

> Android 建议先做。自动直接拨号需要 `CALL_PHONE` 权限；审核或系统策略不允许时，
> 降级为打开带号码的系统拨号界面。iOS 不允许第三方 App 静默直接拨号，只能通过
> `tel:` 拉起系统确认界面。

## 2. BLE 接口

```text
设备名    VelaGuard
Service   6f70656e-7665-4c61-9361-726456470001
Event     6f70656e-7665-4c61-9361-726456470002
属性      Read + Notify（MVP 无需配对或加密）
```

连接成功后必须打开 Event 的 CCC Notify。App 进入后台、蓝牙重启或设备断开后应自动
恢复连接与订阅。只连接已由用户绑定的设备，不要响应附近其他同名设备。

这里的“绑定”是 App 保存设备 BLE 地址/标识，不是 Android 系统设置中的蓝牙配对。
设备名位于 Scan Response，扫描器必须启用 Active Scan。不要调用 `createBond()`；
扫描到设备后直接 `connectGatt()`、发现服务并写入 CCC 开启 Notify。

## 3. CALL_REQUEST 数据包

固定 16 字节，小端序：

| 偏移 | 长度 | 字段 | 说明 |
| --- | --- | --- | --- |
| 0 | 2 | magic | ASCII `VG` |
| 2 | 1 | version | 当前为 `1` |
| 3 | 1 | command | `1` = CALL_REQUEST |
| 4 | 1 | event_type | `1` 手动 SOS；`2` 跌倒；`3` 语音 SOS |
| 5 | 1 | risk | 风险等级 0–4 |
| 6 | 1 | confidence | 置信度 0–100 |
| 7 | 1 | flags | bit0 = 用户确认求助 |
| 8 | 4 | event_id | 事件编号，uint32 LE |
| 12 | 4 | uptime_ms | 设备开机毫秒数，uint32 LE |

校验顺序：长度为 16 → magic → version → command → event_id 去重。相同 event_id 在
一次连接或重连后只能拨打一次。未知版本或命令只记录，不拨号。

## 4. 手机界面

- 设备页：扫描、绑定、连接状态、电量（后续）、解除绑定。
- 联系人页：姓名、电话号码、测试拨号；号码只保存在手机本地安全存储。
- 紧急页：事件类型、风险、置信度、3 秒倒计时、“立即拨打”和“取消”。
- 历史页：事件时间、类型、是否拨号、拨号结果。
- 常驻通知（Android）：显示 VelaGuard 守护服务正在运行，保证后台连接稳定。

## 5. Android 权限与后台要求

- Android 12+：`BLUETOOTH_SCAN`、`BLUETOOTH_CONNECT`。
- Android 11 及以下按系统要求申请蓝牙/定位权限。
- 直接拨号：`CALL_PHONE`；降级拨号盘可使用 `ACTION_DIAL`。
- 使用 Foreground Service 维持 BLE 连接，并处理开机启动、蓝牙恢复和 App 被回收。
- 联系人可通过系统联系人选择器选择；避免申请读取整个通讯录的权限。

## 6. 必测场景

- 手动 SOS、跌倒确认、跌倒倒计时结束、语音 SOS 均只拨一次。
- 疑似跌倒后用户取消，不拨号。
- 手机锁屏、App 后台运行时仍能收到事件。
- BLE 断开重连后恢复订阅，不重复处理旧 event_id。
- 没有联系人、没有电话权限、无 SIM、飞行模式时给出明确提示并保留事件。
- 用户在 3 秒内取消时不得拨号。

## 7. 交付物

- 可安装 APK 和完整源码。
- README：构建方法、最低 Android 版本、权限说明。
- BLE 协议解析单元测试。
- 后台接收和真实拨号的演示视频。
- 手机型号/Android 版本兼容测试表。
