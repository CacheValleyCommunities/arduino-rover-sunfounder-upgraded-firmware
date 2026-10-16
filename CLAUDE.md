# GalaxyRVR

Arduino UNO firmware for the GalaxyRVR robot car. It talks to the in-tree iOS app (and SunFounder Controller) over ESP32-CAM, and supports app remote control, IR/ultrasonic obstacle avoidance, and IR follow.

Human-facing how-to: [`docs/HOWTO.md`](docs/HOWTO.md). Wire format: [`docs/protocol.md`](docs/protocol.md).

## Build

- **Board**: Arduino UNO
- **IDE**: Arduino IDE 2.x
- **Compiler toolchain**: Arduino AVR Boards 1.8.3

Libraries: `SoftPWM`, `Servo`, `ArduinoJson`, `SunFounder_AI_Camera`

Open `galaxy-rvr/galaxy-rvr.ino` to compile and flash, or from the repo root:

```bash
python3 tools/deploy.py          # UNO USB upload + ESP32-CAM OTA .bin
python3 tools/deploy.py uno      # UNO only
python3 tools/deploy.py cam      # pack C2, build CAM, copy firmware.bin
```

ESP32-CAM firmware is PlatformIO (`cd ai-camera-firmware && pio run -e esp32cam`). Flash steps: [`deploy.md`](deploy.md).

### Dev Container (VS Code / Cursor)

Reopen the folder in a container for CMake, gdb, C++ IntelliSense, `arduino-cli`, and PlatformIO.

1. Dev Containers extension → **Reopen in Container**
2. Wait for `.devcontainer/setup.sh` (CMake tests + Arduino AVR 1.8.3 + libraries)

USB flashing from Docker on macOS is unreliable. Compile in the container if you want; flash the UNO from the host in Arduino IDE.

### Unit tests

Host-side GoogleTest (no hardware) — UNO in `test/native`, ESP32-CAM in `test/cam`:

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default --output-on-failure
```

iOS binary-protocol tests:

```bash
swift test --package-path ios
```

C2 Blockly local UI + JSON harness (no rover):

```bash
python3 -m unittest discover -s test/c2 -v
cd ai-camera-firmware && python3 tools/c2_local.py
# open http://127.0.0.1:8765/c2
```

## Architecture

```
galaxy-rvr.ino     — setup/loop (AiCamera, VERSION)
defaults.h         — AP, NAME, follow/avoid defaults, debug flags
pins.h             — GPIO map
rover.cpp/h        — beginner API, begin/look/lamp, follow/avoid
app_state.cpp/h    — idle/app state machine, mode dispatch
protocol.cpp/h     — WebSocket entities, telemetry, runtime config
car_control.cpp/h  — motors
rgb.cpp/h          — RGB strip
sensors.cpp/h      — ultrasonic, IR, battery
```

```cpp
rover.begin();
rover.drive.forward(50);
rover.lights.set(RED);
float cm = rover.sensors.distanceCm();
rover.look(90);
```

### State machine

`STATE_IDLE` (waiting for app, LED blinks) → `STATE_APP` (connected).

Modes: `MODE_APP_CONTROL` / `MODE_OBSTACLE_AVOIDANCE` / `MODE_OBSTACLE_FOLLOWING`.

App frames over WebSocket (`onReceive()`): motors `0x01`, RGB `0x02`, servo `0x03`, headlights `0x04`, obstacle mode `0x05`, config `0x07`.

## Versioning

```
A . B . C
│ │ └── bugfix, backward compatible
│ └────── minor update, backward compatible
└────────── major update, not backward compatible
```

- `VERSION` is in `galaxy-rvr/galaxy-rvr.ino`
- Every release must be tagged as `A.B.C`

## Git branches

- `main` = current release (v2 is merged; v1 stays on `v1`)
- Major-version work on a `vN` branch, merge to `main` via PR
- Branch number matches major version A

## Release process

```bash
# 1. PR from the development branch to main
git checkout v2
gh pr create --title "Release v2.0.0" --body "..."

# 2. After merge, tag on main
git checkout main
git pull origin main
git tag 2.0.0
git push origin 2.0.0
```

Distribute via tag (`@2.0.0`) or major-version branch (`@v2`).

## Release artifacts (`output/`)

| File | Description |
|------|-------------|
| `galaxy-rvr.ino.X.Y.Z.hex` | Arduino UNO firmware (no bootloader) |
| `galaxy-rvr.ino.X.Y.Z.with_bootloader.hex` | Full firmware with bootloader |
| `ai-camera-firmware.v*.*.*-ota.bin` | ESP32-CAM OTA firmware |
| `avrdude.exe` / `avrdude.conf` | Windows flash tools |
| `update-arduino-firmware.bat` / `-mac.sh` / `-linux.sh` | Flash scripts |

Flash scripts: list serial ports → user picks one → `avrdude -p atmega328p -c arduino -P <port> -D -U flash:w:<hex>:a`. Windows bundles avrdude; macOS `brew install avrdude`; Linux `apt/dnf install avrdude` (user in `dialout`).

## Publish GitHub Release

```bash
rm output/*.zip 2>/dev/null
powershell -Command "Compress-Archive -Path output\* -DestinationPath galaxy-rvr.ino.X.Y.Z.zip -Force"
gh api repos/sunfounder/galaxy-rvr/releases/tags/X.Y.Z --jq '.assets[].id'
gh api -X DELETE repos/sunfounder/galaxy-rvr/releases/assets/<asset-id>
gh release upload X.Y.Z galaxy-rvr.ino.X.Y.Z.zip --clobber
cp galaxy-rvr.ino.X.Y.Z.zip galaxy-rvr.ino.zip
gh release upload X.Y.Z galaxy-rvr.ino.zip --clobber
```

Fixed download URL (course/docs hardcode this): `https://github.com/sunfounder/galaxy-rvr/releases/latest/download/galaxy-rvr.ino.zip`

Every release must also upload `galaxy-rvr.ino.zip`. Write notes in English: Core Upgrade → Breaking Changes → Features → Bug Fixes → Firmware Update.

## v2 features

- **Robo Pilot remote**: replaces the SunFounder Controller app with smoother remote control
- **Mammoth Coding**: works with the Scratch-style block coding platform
