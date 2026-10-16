# Protocol

UNO, CAM, iOS, and C2 share one binary language. The CAM is a bridge: it does not interpret motor bytes; it frames them for UART and fans telemetry back to whoever is connected.

Canonical definitions:

- UNO: [`galaxy-rvr/protocol.h`](../galaxy-rvr/protocol.h) / [`protocol.cpp`](../galaxy-rvr/protocol.cpp)
- CAM: [`ai-camera-firmware/include/serial_bridge.h`](../ai-camera-firmware/include/serial_bridge.h)
- iOS: [`ios/GalaxyRVR/GalaxyRVR/Protocol/BinaryProtocol.swift`](../ios/GalaxyRVR/GalaxyRVR/Protocol/BinaryProtocol.swift)

IDs `0x06` and telemetry `0x84` are unused.

## Control entities (app → UNO)

Apps send a stream of **entity tag + payload**. UDP and C2 send raw entity bytes (no `0xA0` wrapper). The CAM wraps them as `WSB+` for the UNO. WebSocket binary is the same payload.

The UNO `onReceive()` walks `aiCam.recvBuffer` and switches on each tag.

| ID | Name | Payload | Effect |
|---|---|---|---|
| `0x01` | Motors | `int8 left`, `int8 right` | Sets `leftMotorPower` / `rightMotorPower` (applied in APP + `MODE_APP_CONTROL`) |
| `0x02` | RGB | `u8 r, g, b` | `rover.lights.set(r,g,b)` immediately |
| `0x03` | Servo | `u8 angle` | `rover.look(angle)` clamped 0–140 |
| `0x04` | Lamp | `u8` (`1` = on) | `rover.cameraLamp` (ESP32 flash via `aiCam`) |
| `0x05` | Obstacle | `u8 state`, `u8 mode`, `u8 power` | See modes below |
| `0x07` | Config | `u8 count`, then `count × (u8 id + int16 BE)` | `roverConfigSet` |

Unknown bytes are skipped.

### Obstacle (`0x05`)

| `state` | `mode` | Result |
|---|---|---|
| `0` | ignored | `MODE_APP_CONTROL`, motor powers cleared |
| ≠0 | `0` | `MODE_OBSTACLE_AVOIDANCE` |
| ≠0 | ≠0 | `MODE_OBSTACLE_FOLLOWING` |

`power` becomes `obstaclePower` (iOS sends `65`).

iOS helpers:

| Drive mode | Bytes |
|---|---|
| Drive | `05 00 00 41` |
| Avoid | `05 01 00 41` |
| Follow | `05 01 01 41` |

(`0x41` = 65)

### Config (`0x07`)

C2 builds this from JSON keys (`follow_stop_cm`, …). iOS does not send `0x07` today.

| ID | Key (C2 JSON) | Variable | Default | Clamp |
|---|---|---|---|---|
| 1 | `follow_stop_cm` | `followStopCm` | 12 | 1–100 |
| 2 | `follow_slow_cm` | `followSlowCm` | 25 | 1–200 |
| 3 | `follow_distance_cm` | `followDistanceCm` | 50 | 1–300 |
| 4 | `follow_slow_power` | `followSlowPower` | 30 | 0–100 |
| 5 | `avoid_threshold_cm` | `avoidThresholdCm` | 35 | 1–300 |
| 6 | `reverse_cm` | `reverseCm` | 15 | 1–100 |
| 7 | `move_power` | `movePower` | 70 | 0–100 |
| 8 | `ms_per_foot` | `msPerFoot` | 1800 | 100–10000 |
| 9 | `ms_per_deg` | `msPerDeg` | 8 | 1–100 |

IDs 1–6 are consumed by follow/avoid (`wallAhead()` uses 5). IDs 7–9 are stored and telemetried for C2 timed moves; the UNO drive API does not read them.

## Telemetry (UNO → apps)

Built in `handleSensorData()` / `buildTelemetry()`, sent with `aiCam.sendBinaryData`. CAM fans the framed packet to WS clients, the UDP peer, and C2.

### Packet layout

`PKT_SIZE` = 12 + `CFG_TELEMETRY_BYTES` (18) = **30 bytes**.

| Offset | Content |
|---|---|
| 0 | `0xA0` start |
| 1 | Length = `PKT_SIZE - 4` (26) |
| 2 | XOR checksum of bytes `0 … PKT_SIZE-2` (everything except the last byte) |
| 3 | `0x81` ultrasonic |
| 4–5 | Distance in **millimetres**, big-endian `u16` (`distanceCm * 10`) |
| 6 | `0x82` IR |
| 7 | IR bits: `(left << 1) \| right` (bit 1 = left, bit 0 = right). `1` = blocked |
| 8 | `0x83` battery |
| 9 | `round((volts - 6.0) * 100)` |
| 10 | `0x85` config |
| 11–28 | 9 × `int16` big-endian, IDs 1–9 in order |
| 29 | `0xA1` end |

Checksum check (iOS): XOR of every byte except the final `0xA1` must be `0` (the stored checksum is chosen so that is true).

Battery on the wire is **not** percent. iOS maps 6.6–8.4 V to the fuel bar. `rover.sensors.batteryPercent()` is a separate 6.6–8.4 map on the UNO.

Failed ultrasonic pings: cached cm is `-1`, millimetres wrap as unsigned — consumers treat `0` or `> 30000` mm as “no reading”.

## Serial (CAM ↔ UNO)

Baud **115200**. Line-oriented ASCII plus binary after `WSB+`.

### UNO → CAM (`SET+`)

The `SunFounder_AI_Camera` library emits these during `aiCam.begin`. CAM handler: [`main.cpp` `handleSet`](../ai-camera-firmware/src/main.cpp).

| Command | Meaning |
|---|---|
| `SET+NAME…` / `SET+TYPE…` | Discovery / WS hello identity |
| `SET+APSSID…` / `SET+APPSK…` / `SET+APCHN…` | SoftAP |
| `SET+STASSID…` / `SET+STAPSK…` | Home Wi-Fi (optional) |
| `SET+PORT…` | WebSocket listen port (GalaxyRVR: `30102`) |
| `SET+START` | STA connect, print AP IP, start WS + camera + UDP |
| `SET+LAMP<0-100>` | Flash PWM (internally 0–10) |
| `SET+RESET` | ESP restart |
| `SET+RSTSTA` | Reconnect STA |
| `SET+RSTCFG` | Factory-reset NVS |
| `SET+SSID` / `SET+PSK` / `SET+MODE` | Deprecated |

Replies: `[OK]`, `[OK] <ip>`, or `[ERROR] …`.

### CAM → UNO (session)

| Line | When |
|---|---|
| `[CONNECTED] …` | First UDP / WS / C2 peer |
| `[APPSTOP] …` | Last peer gone or idle timeout |
| `[OK]` / `[ERROR]` | `SET+` replies |

### Control / telemetry prefixes

| Prefix | Direction | Body |
|---|---|---|
| `WSB+` | Both | Binary. CAM→UNO: `0xA0` `<len>` `<xor>` `<payload>` `0xA1` if the payload was not already framed. UNO→CAM: full telemetry packet, fanned out |
| `WS+` | UNO→CAM | Text forwarded to all WebSocket clients |
| (legacy) `0x0C` | UNO→CAM | Still parsed in `serialBridgeRead` |

## UDP (port 30103)

Started at `SET+START`. Beacon every **500 ms** to `255.255.255.255` plus AP/STA broadcasts.

### Identity JSON

```json
{"Name":"GalaxyRVR","Type":"GalaxyRVR","udp":30103,"video":"http://192.168.4.1:9000/mjpg","ap":"192.168.4.1","sta":"…"}
```

`sta` is omitted if empty / `0.0.0.0`. `video` prefers STA IP when connected.

Discovery is UDP beacons (no mDNS in this firmware, to keep the OTA image under 1280 KB).

### Client → CAM

| Packet | Behavior |
|---|---|
| ASCII `GRVR?` | Unicast identity JSON to sender |
| ASCII `ping` or single `0x00` | Keepalive; marks UDP peer |
| Other binary | Forward to UNO as control; marks peer |

Peer timeout **3 s**. Telemetry `sendBIN` goes only to the active peer.

Probe from a laptop:

```bash
python3 ios/tools/udp_probe.py
python3 ios/tools/udp_probe.py 192.168.4.1
```

## WebSocket (port from `SET+PORT`, default 8765, GalaxyRVR 30102)

Max **5** clients. On connect, hello JSON:

```json
{"Name":"…","Type":"…","Check":"SC","video":"http://…:9000/mjpg","StaIp":"…","VideoTemplate":"…"}
```

| Client message | Action |
|---|---|
| Text `ping` | `pong <millis>` |
| Text `SET+{json}` | Config / Wi-Fi scan / `restart-sta` |
| Text region JSON (A–Z) | Compressed to serial `WS+` (SunFounder Controller widgets) |
| Binary | Framed `WSB+` to UNO |

Idle ~3 s with no other peers → `[APPSTOP]`.

## iOS arcade mix

Joystick `x` (turn), `y` (throttle, up = forward):

```
left  = throttle + turn
right = throttle - turn
```

Normalize if peak > 1, scale by `maxPower` (100). Result is two `int8` motor powers.

Motors are coalesced: dirty flag, send every **50 ms**, immediate flush on zero.

## C2 program (CAM HTTP)

Blockly **Deploy** POSTs the Program JSON to the CAM. The CAM stores it in NVS (`c2prog`) and runs it onboard (entity frames to the UNO). A live **Run** still executes in the browser.

| Method | Path | Body / result |
|---|---|---|
| `GET` | `/c2/api/program` | `{ deployed, onboard, program }` |
| `POST` | `/c2/api/program` | `{ version: 1, steps: [...] }` — save and start |
| `DELETE` | `/c2/api/program` | Clear stored program and stop |
| `POST` | `/c2/api/stop` | Stop live and onboard runners |

`GET /c2/api/state` also reports `deployed` and `onboard`. Autorun: a deployed program starts again after CAM reboot until **Remove from rover**. Max JSON **3072** bytes.

## Golden examples

Motors left=40, right=-100:

```
01 28 9C
```

RGB 255,0,0:

```
02 FF 00 00
```

Servo 90:

```
03 5A
```

Lamp on:

```
04 01
```

These match [`BinaryProtocolTests.swift`](../ios/GalaxyRVRProtocolTests/BinaryProtocolTests.swift).
