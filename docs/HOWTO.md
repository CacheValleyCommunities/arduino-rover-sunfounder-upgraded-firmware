# How to GalaxyRVR

Flash the rover, drive it from the iOS app, program it in the browser with C2 Blockly, or write Arduino sketches with `rover.*`.

Kit assembly and wiring photos: [SunFounder GalaxyRVR docs](https://docs.sunfounder.com/projects/galaxy-rvr/en/latest/). Wire format: [protocol.md](protocol.md).

## 1. What you have

```
Phone or laptop
        │  Wi-Fi  (AP GalaxyRVR / 12345678)
        ▼
ESP32-CAM     camera, Wi-Fi, C2 Blockly, UDP/WebSocket bridge
        │  UART 115200  (WSB+ binary frames)
        ▼
Arduino UNO   motors, RGB, IR, ultrasonic, servo, battery
```

- **Arduino UNO** owns the body. Firmware: [`galaxy-rvr/`](../galaxy-rvr/). Beginner surface: `rover.drive`, `rover.lights`, `rover.sensors`, `rover.look`.
- **ESP32-CAM** owns the network. Firmware: [`ai-camera-firmware/`](../ai-camera-firmware/). Settings on port 80, MJPEG on 9000, WebSocket **30102**, UDP **30103**.
- Remotes talk to the CAM, never the UNO: in-tree **iOS app**, **C2 Blockly** (`http://192.168.4.1/c2`), or **SunFounder Controller**.

Default Wi-Fi: SSID `GalaxyRVR`, password `12345678`, AP `192.168.4.1`. RGB **blinks green** while idle, **magenta** when a remote is connected.

## 2. Get it running

A kit that already runs can skip to [Join the rover Wi-Fi](#join-the-rover-wi-fi).

### Flash the Arduino UNO

Robot switch on **Upload**. From the repo root on the host:

```bash
python3 tools/deploy.py uno
# or both boards' artifacts: python3 tools/deploy.py
```

Or open [`galaxy-rvr/galaxy-rvr.ino`](../galaxy-rvr/galaxy-rvr.ino) in Arduino IDE 2.x. Board **Arduino UNO**, pick the USB port, Upload.

Libraries: `SoftPWM`, `Servo`, `ArduinoJson`, `SunFounder_AI_Camera`. Board package: Arduino AVR 1.8.3. Switch the robot back to **Run**.

### Flash the ESP32-CAM (OTA)

The CAM has no USB. Package an OTA `.bin`, then upload it in the web UI:

```bash
python3 tools/deploy.py cam
# writes firmware.bin at the repo root
```

Use [`firmware.bin`](../firmware.bin) at the repo root (do not upload `firmware.elf`). Then:

1. Switch the robot to **Run**, join Wi-Fi `GalaxyRVR` / `12345678`
2. Open [http://192.168.4.1](http://192.168.4.1)
3. Use **Update** to upload `firmware.bin` only

Factory reset (forgotten AP password): short **IO13–IO15**, press reset, wait for two bright-LED blinks, release, reset again. UART commands: [ai-camera-firmware/Readme.md](../ai-camera-firmware/Readme.md).

### Join the rover Wi-Fi

1. Power the rover; wait until RGB blinks green.
2. Join `GalaxyRVR` / `12345678`. You should get `192.168.4.x`. The CAM is `192.168.4.1`.

If the AP never appears, factory-reset the CAM. If the AP appears but RGB never goes magenta, check the serial link and UNO firmware.

## 3. Drive with the mobile app

Source: [`ios/GalaxyRVR/`](../ios/GalaxyRVR/). iOS 17+, landscape, physical iPhone (simulator cannot join the rover AP).

1. Open [`ios/GalaxyRVR/GalaxyRVR.xcodeproj`](../ios/GalaxyRVR/GalaxyRVR.xcodeproj) in Xcode.
2. Set your **Signing Team**, run on a device, allow **Local Network**.

Scan listens for UDP beacons on **30103**. **Join rover Wi-Fi** uses SSID prefix `GalaxyRVR` / `12345678`. Tap a rover to Drive: UDP ping, drive mode, servo 90°, MJPEG at `http://<ip>:9000/mjpg`.

| Control | What it does |
|---|---|
| Left joystick | Arcade mix. Disabled in AVOID/FOLLOW. |
| Look pad | Camera servo 0–140°. |
| LAMP | ESP32 flash. |
| AVOID / FOLLOW | Onboard IR + ultrasonic on the UNO. |
| CODE | C2 Blockly (`http://<ip>/c2`). |
| Camera aperture | JPEG 8–20, QVGA / HVGA / VGA (port 80). |
| Fuel / radar | Battery (~6.6–8.4 V) and ultrasonic + IR. |

SunFounder Controller over WebSocket port **30102** still works.

## 4. Code in the browser (C2)

C2 is Blockly on the rover. Open [http://192.168.4.1/c2](http://192.168.4.1/c2) or **CODE** in the iOS app.

**First program:** when robot turns on → forever → if distance `<` 20 then turn left 90 else move forward 1 ft → **Run** to try it, or **Deploy** to store it on the rover. **Stop** cuts motors (live and onboard). **Remove from rover** forgets the stored program. Heartbeat ~400 ms; a dropped tab times out a live **Run**, but a **Deploy**ed program keeps going after you close the page. Stacks hang off the **when robot turns on** hat; Run/Deploy only execute those entry stacks if any exist.

**Control:** when robot turns on, wait, repeat, forever, if, wait until, stop all. **Sensors:** distance, IR, battery, named thresholds. **Move:** timed forward/back (feet/cm from `move_power` + `ms_per_foot`), turn (`ms_per_deg`), RGB, lamp, look.

**Thresholds** in the sidebar write config IDs 1–9 (see [protocol.md](protocol.md)). Follow/avoid on the UNO use 1–6; C2 timed moves use 7–9. C2 does **not** send obstacle entity `0x05` — onboard AVOID/FOLLOW is the iOS buttons.

Edit UI in [`ai-camera-firmware/www-c2/`](../ai-camera-firmware/www-c2/), then `python tools/pack_c2.py`.

Run C2 without a rover (Blockly UI + simulated `/c2/api/*`):

```bash
cd ai-camera-firmware
python3 tools/c2_local.py
# open http://127.0.0.1:8765/c2
```

The sidebar **Preview** map moves with motors, shows RGB/lamp/look, and traces a trail. **Reset map** puts the rover back at the origin. On a real camera this panel stays hidden (no `preview` in `/c2/api/state`). Local **Deploy** runs the JSON on the sim so you can close the editor while the preview keeps moving.

The Program sidebar JSON (`{ version: 1, steps: [...] }`) can be executed against the same sim (from the repo root):

```bash
python3 ai-camera-firmware/tools/c2_program.py test/c2/fixtures/seed.json
python3 -m unittest discover -s test/c2 -v
```

## 5. Code the robot in Arduino

The stock sketch already runs the app. You can keep it and call `rover.*`, or replace `loop()` (the phone will not drive until you put the app loop back).

```cpp
#include "defaults.h"
#include "rover.h"

void setup() {
  Serial.begin(115200);
  rover.begin();
  rover.lights.set(RED);
}

void loop() {
  ultrasonicUpdate();
  if (rover.sensors.wallAhead()) {
    rover.drive.stop();
  } else {
    rover.drive.forward(50);
  }
}
```

Power is roughly **0–100**. Look is **0–140°** (center **90**). Distance is **cached** — call `ultrasonicUpdate()` once per loop. Failed pings return `-1`; `wallAhead()` treats that as an obstacle.

`rover.begin()` needs globals `Servo servo` and `AiCamera aiCam` (same as the stock sketch).

### Pins

| Function | Pin |
|---|---|
| Motor left A/B | 2, 3 |
| Motor right A/B | 4, 5 |
| Servo | 6 |
| IR right / left | 7 / 8 |
| Ultrasonic trig+echo | 10 |
| RGB B, R, G | 11, 12, 13 |
| Battery | A3 |

Defined in [`pins.h`](../galaxy-rvr/pins.h). Product defaults (AP, follow distances, debug flags) in [`defaults.h`](../galaxy-rvr/defaults.h).

### Recipes

Blink: `rover.lights.set(RED)` / `off()`. Colors: `RED`, `GREEN`, `BLUE`, `ORANGE`, `YELLOW`, `CYAN`, `MAGENTA`, `WHITE`, or `set(r,g,b)`.

Stop at a wall: `ultrasonicUpdate()` then `distanceCm() < 25` → stop + red, else forward + green.

Sweep: `rover.look(a)` from 40 to 140.

Follow / avoid: `ultrasonicUpdate(); rover.follow();` (or `rover.avoid()`). Same as the app buttons. Tunables are the config IDs in [protocol.md](protocol.md).

IR bumper: `obstacleLeft()` → `backRight(60)`, `obstacleRight()` → `backLeft(60)`. IR reads **LOW when blocked**. `irBits()` is `(left << 1) | right`.

### Keep the app alive

```cpp
void loop() {
  appStateUpdate(aiCam.ws_connected);
  if (currentState == STATE_APP) {
    ultrasonicUpdate();
    handleSensorData();
    modeHandler();
  }
  aiCam.loop();  // faster than ~80 ms
}
```

Put extra behavior only in `MODE_APP_CONTROL`, or the app and your sketch fight over the motors.

Host tests (no hardware) — UNO (`test/native`) and ESP32-CAM (`test/cam`):

```bash
cmake --preset default && cmake --build --preset default
ctest --preset default --output-on-failure
python3 -m unittest discover -s test/c2 -v
```

## 6. Code the mobile app

SwiftUI, iOS 17+. Protocol tests without a phone:

```bash
swift test --package-path ios
```

Change UI in `DriveView` / `DriveSession`. Change bytes in `BinaryProtocol`, send them from `RoverUDPClient`. Golden packets live in [`ios/GalaxyRVRProtocolTests/BinaryProtocolTests.swift`](../ios/GalaxyRVRProtocolTests/BinaryProtocolTests.swift).

Discovery: ASCII `GRVR?`; keepalive `ping` every 0.5 s. Do not resolve `GalaxyRVR.local` — browse TXT (`ap` / `sta`) and unicast those IPs. Motors: arcade-mix, at most every 50 ms, flush zeros immediately.

```bash
python3 ios/tools/udp_probe.py
python3 ios/tools/udp_probe.py 192.168.4.1
```

## 7. Next

| | |
|---|---|
| [protocol.md](protocol.md) | Entities, telemetry, serial, UDP, WebSocket, config IDs |
| [ai-camera-firmware/Readme.md](../ai-camera-firmware/Readme.md) | CAM UART, OTA, factory reset |
| [CLAUDE.md](../CLAUDE.md) | Build, tests, versioning |

Regenerate CAM web assets after editing C2 or the settings page:

```bash
cd ai-camera-firmware
python tools/pack_c2.py
python tools/file_to_c_gz.py
```
