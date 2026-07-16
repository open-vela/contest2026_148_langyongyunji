# VelaGuard Safety Guardian

Analyze VelaGuard safety events and execute an emergency response on openvela devices.

## When to use
Use this skill when a message contains VelaGuard safety data, SOS, fall_suspected, voice_sos, sound_abnormal, high risk, emergency, help, or guard alert.

## How to use
1. Parse the event fields: type, phase, risk, confidence, uptime_ms, and summary.
2. If phase is suspected and risk is 3 or higher, explain why the device entered pre-alert and tell the user how to cancel or confirm.
3. If phase is alert or the user explicitly asks for SOS, execute local emergency actions:
   - vibrate for immediate haptic feedback when available
   - get_current_time for the report timestamp
   - write_file an event record under /data/agent/velaguard-events.log
   - run_shell with a safe diagnostic command such as free or ps when the user asks for device status
4. If Feishu, MQTT, WebSocket, BLE, or another channel is configured, send a short emergency notification to the bound receiver.
5. Generate a concise Chinese event summary that includes event type, risk level, confidence, device action, and recommended next step.

## Important
- Do not treat VelaGuard as a chat-only feature. It is an active safety workflow driven by sensor or UI events.
- If a hardware tool is unavailable, state the unavailable capability and continue with file logging and text notification.
- Keep emergency responses short and action-oriented.

## Example
User: `VELAGUARD_EVENT {"type":"fall_suspected","phase":"alert","risk":3,"confidence":88,"summary":"老人模式：检测到冲击、姿态变化和静止窗口，判定为疑似跌倒。"}`

→ vibrate
→ get_current_time
→ write_file `/data/agent/velaguard-events.log`
→ "已进入跌倒告警：风险 3，置信度 88%。判断依据是冲击、姿态突变和静止窗口。已记录事件并尝试通知联系人，请尽快确认用户状态。"
