# Flash the GalaxyRVR

Hackathon path (parts, tools, which file to edit): [docs/HACKATHON.md](docs/HACKATHON.md).

The rover is two boards. Flash **both** if you changed either firmware.

```
Laptop USB  →  Arduino UNO     (motors, RGB, IR, ultrasonic, servo)
Wi-Fi OTA   →  ESP32-CAM       (camera, AP, C2, UDP/WebSocket)
```

Default AP after a good flash: SSID **GalaxyRVR-XXXXXX**, password **12345678**, CAM at **192.168.4.1**. RGB blinks green while idle, magenta when a remote (phone or C2) is connected. Override the name and password with `python3 tools/deploy.py --ssid … --password …`.

USB flashing from Docker on macOS is unreliable. Compile in the container if you want; flash the UNO from the host.

## 0. One script

From the repo root, with `arduino-cli` and PlatformIO (`pio`) on the **host**:

```bash
python3 tools/deploy.py
```

That uploads the UNO sketch over USB and writes the CAM OTA image to [`firmware.bin`](firmware.bin) (and `ai-camera-firmware/firmware/ai-camera-firmware.v*-ota.bin`).

```bash
python3 tools/deploy.py uno                      # USB flash only
python3 tools/deploy.py uno -p /dev/cu.usbmodem1101
python3 tools/deploy.py cam                      # pack C2 + build + copy .bin
python3 tools/deploy.py --ssid MyRover --password hunter2x
```

`--ssid` / `--password` (or `GALAXY_AP_SSID` / `GALAXY_AP_PASSWORD`) bake the soft-AP into the UNO sketch and the CAM factory defaults. The CAM on-air name is `<ssid>-` plus the last 6 hex digits of its MAC (SSID at most 25 characters, password 8–63). Omit both flags to keep **GalaxyRVR** / **12345678**. The iOS “Join rover Wi-Fi” button still looks for the `GalaxyRVR` prefix; a custom name means you join that network yourself.

Needs Arduino AVR 1.8.3 and libraries `SoftPWM`, `Servo`, `ArduinoJson`, `SunFounder_AI_Camera` (same as Arduino IDE). Then still OTA the `.bin` in the CAM web UI below — the CAM has no USB.

## 1. Arduino UNO (USB)

The robot’s top switch must be **Upload** while the cable is connected.

1. Install [Arduino IDE 2.x](https://www.arduino.cc/en/software). Board package: **Arduino AVR Boards 1.8.3**.
2. Install libraries: `SoftPWM`, `Servo`, `ArduinoJson`, `SunFounder_AI_Camera`.
3. Open [`galaxy-rvr/galaxy-rvr.ino`](galaxy-rvr/galaxy-rvr.ino).
4. Board **Arduino UNO**, pick the USB serial port, **Upload**.
5. Switch the robot back to **Run**, unplug USB if you were only flashing.

Or `python3 tools/deploy.py uno`. Manual equivalent:

```bash
arduino-cli compile --fqbn arduino:avr:uno galaxy-rvr
arduino-cli upload -p /dev/cu.usbmodemXXXX --fqbn arduino:avr:uno galaxy-rvr
```

Use the real port from `arduino-cli board list`.

## 2. ESP32-CAM (OTA)

The CAM has no USB. Build a `.bin` on the laptop, then push it over Wi-Fi.

`python3 tools/deploy.py cam` packs [`ai-camera-firmware/www-c2/`](ai-camera-firmware/www-c2/) (Blockly UI), builds with PlatformIO, rejects images over **1280 KB**, and copies `firmware.bin`. Manual equivalent:

```bash
cd ai-camera-firmware
python3 tools/pack_c2.py
pio run -e esp32cam
```

The OTA image must stay **under 1280 KB** so it fits the stock ESP32-CAM default OTA slot. This build is about **1269 KB**.

`python3 tools/deploy.py cam` copies the same bytes to [`firmware.bin`](firmware.bin) and `ai-camera-firmware/firmware/*-ota.bin`. Raw PIO output: `ai-camera-firmware/.pio/build/esp32cam/firmware.bin`.

Then:

1. Switch the robot to **Run** and power it on.
2. Join Wi-Fi **GalaxyRVR-XXXXXX** / **12345678**.
3. Open [http://192.168.4.1](http://192.168.4.1).
4. **Update** and choose **`firmware.bin` only**. Wait until it reports success, then restart if the page asks.

Do not pick `firmware.elf` (~15 MB) from that same folder. The CAM OTA slot is ~1.2–1.9 MB; the ELF is the debug image and fails with **Not Enough Space**. Skip `.map`, `bootloader.bin`, and any `*-factory.bin` merge as well.

## 3. Check that it took

1. RGB blinks green within a few seconds of **Run**.
2. Phone or laptop sees AP **GalaxyRVR-XXXXXX**.
3. [http://192.168.4.1](http://192.168.4.1) loads the settings page.
4. [http://192.168.4.1/c2](http://192.168.4.1/c2) loads Blockly.

Smoke C2: **when robot turns on** → **move forward 1 sec** → **Run**. RGB should go magenta and the rover should roll, then stop. **Deploy** stores that program on the CAM so it runs after you close the tab.

Local Blockly at `http://127.0.0.1:8765/c2` (`python3 tools/c2_local.py`) is a simulator. It does not flash or drive the robot.

## 4. If the AP is gone

Forgotten password or a wedged CAM: factory-reset the ESP32.

1. Power the board.
2. Short **IO13–IO15**, press reset, wait for two bright-LED blinks, release the short, press reset again.

The stored name and password return to the UNO’s `SET+` values (`GalaxyRVR` / `12345678` with the stock sketch), so the on-air network is **GalaxyRVR-XXXXXX** again. UART `SET+` commands are in [`ai-camera-firmware/Readme.md`](ai-camera-firmware/Readme.md).

If OTA says **Not Enough Space**, you uploaded the ELF or another oversized file. Use `firmware.bin` (under 1280 KB) above.

If the AP appears but RGB never goes magenta and C2/iOS cannot drive, the UART link or the UNO firmware is the problem — reflash the UNO with the switch on **Upload**, then **Run**.
