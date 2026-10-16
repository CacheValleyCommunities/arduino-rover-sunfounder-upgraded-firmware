# GalaxyRVR

Arduino UNO firmware for the GalaxyRVR robot car. Drive it from the in-tree iOS app, program it in the browser with C2 Blockly, or keep using SunFounder Controller.

Current version: **2.0.0**

**How to flash, drive, and code:** [docs/HOWTO.md](docs/HOWTO.md)

## Quick Start

1. Open `galaxy-rvr/galaxy-rvr.ino` in Arduino IDE
2. Board: Arduino UNO, pick the port, Upload
3. Join Wi-Fi `GalaxyRVR` / `12345678`
4. Drive with the iOS app (`ios/GalaxyRVR/GalaxyRVR.xcodeproj`), or open `http://192.168.4.1/c2` for Blockly

Libraries: `SoftPWM`, `Servo`, `ArduinoJson`, `SunFounder_AI_Camera`. Board package: Arduino AVR 1.8.3.

```cpp
rover.begin();
rover.drive.forward(50);
rover.lights.set(RED);
float cm = rover.sensors.distanceCm();
rover.look(90);
rover.drive.stop();
```

## Develop in VS Code

Dev Container (CMake, gdb, `arduino-cli`, PlatformIO): **Reopen in Container**, then **Terminal → Run Task** for Test: build / Test: run.

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default --output-on-failure
```

Flash the UNO from the host with Arduino IDE. ESP32-CAM:

```bash
cd ai-camera-firmware && pio run -e esp32cam
```

OTA the `.bin` at `http://192.168.4.1`.

## Project Structure

```
galaxy-rvr/                 # Arduino UNO sketch
  galaxy-rvr.ino            # setup/loop, VERSION
  defaults.h                # AP, NAME, follow/avoid, debug flags
  pins.h                    # GPIO map
  rover.cpp/h               # beginner API + follow/avoid
  app_state.cpp/h           # idle/app + modes
  protocol.cpp/h            # entities, telemetry, config
  car_control.cpp/h         # motors
  rgb.cpp/h                 # LED strip
  sensors.cpp/h             # ultrasonic, IR, battery
ai-camera-firmware/         # ESP32-CAM (PlatformIO)
ios/                        # iOS app + Swift protocol tests
docs/                       # HOWTO + protocol
test/native/                # host GoogleTest (UNO)
test/cam/                   # host GoogleTest (ESP32-CAM)
```

## Docs

| Page | Contents |
|---|---|
| [docs/HOWTO.md](docs/HOWTO.md) | Flash, iOS, C2, Arduino `rover.*` |
| [docs/protocol.md](docs/protocol.md) | Binary entities, serial, UDP, WebSocket |
| [ai-camera-firmware/Readme.md](ai-camera-firmware/Readme.md) | CAM UART, OTA, factory reset |

Kit assembly: https://docs.sunfounder.com/projects/galaxy-rvr/en/latest/

## About SunFounder

SunFounder focuses on STEAM education with open-source robots, dev boards, kits, and modules.

Website: [www.sunfounder.com](https://www.sunfounder.com)

Contact: [service@sunfounder.com](mailto:service@sunfounder.com)
