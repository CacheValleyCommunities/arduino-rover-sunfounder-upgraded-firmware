#!/usr/bin/env python3
"""Flash the Arduino UNO and package the ESP32-CAM OTA image.

The rover is two boards. This script:
  1. Compiles galaxy-rvr and uploads it over USB (robot switch on Upload).
  2. Packs C2 Blockly, builds the CAM firmware, and copies firmware.bin
     for the web OTA page (join the rover AP, open http://192.168.4.1).

Wi-Fi (soft-AP) defaults to GalaxyRVR / 12345678. Override at deploy time;
the CAM broadcasts `<ssid>-<last 6 MAC hex>`.

Examples:
  python3 tools/deploy.py
  python3 tools/deploy.py --ssid MyRover --password hunter2x
  python3 tools/deploy.py uno -p /dev/cu.usbmodem1101
  python3 tools/deploy.py cam --ssid MyRover --password hunter2x
"""

from __future__ import annotations

import argparse
import glob
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
UNO_SKETCH = ROOT / "galaxy-rvr"
CAM_DIR = ROOT / "ai-camera-firmware"
UNO_FQBN = "arduino:avr:uno"
UNO_OUT = ROOT / "build" / "uno"
CAM_ENV = "esp32cam"
CAM_BUILD_BIN = CAM_DIR / ".pio" / "build" / CAM_ENV / "firmware.bin"
OTA_SLOT_BYTES = 1280 * 1024  # stock ESP32-CAM Arduino OTA partition
MAIN_CPP = CAM_DIR / "src" / "main.cpp"
WIFI_HEADER = ROOT / "build" / "wifi_override.h"
DEFAULT_AP_SSID = "GalaxyRVR"
DEFAULT_AP_PASSWORD = "12345678"
SSID_MAX = 25  # ESP32 AP name is "<ssid>-" + 6 MAC hex chars (32 max)
PASSWORD_MIN = 8
PASSWORD_MAX = 63


class DeployError(RuntimeError):
    """A step failed with a message already printed for the user."""


def die(message: str, code: int = 1) -> None:
    print(message, file=sys.stderr)
    raise SystemExit(code)


def require(name: str) -> str:
    path = shutil.which(name)
    if not path:
        die(f"Need {name} on PATH.")
    return path


def run(cmd: list[str], cwd: Path | None = None, env: dict[str, str] | None = None) -> None:
    print("+ " + " ".join(cmd))
    merged = None
    if env:
        merged = os.environ.copy()
        merged.update(env)
    try:
        subprocess.run(cmd, cwd=cwd, check=True, env=merged)
    except subprocess.CalledProcessError as err:
        raise DeployError(f"command failed ({err.returncode}): {' '.join(cmd)}") from err
    except FileNotFoundError as err:
        raise DeployError(f"missing executable: {cmd[0]}") from err


def c_string(value: str) -> str:
    return '"' + value.replace("\\", "\\\\").replace('"', '\\"') + '"'


def validate_wifi(ssid: str, password: str) -> None:
    forbidden = set("\n\r\0")
    if not 1 <= len(ssid.encode("utf-8")) <= SSID_MAX:
        die(f"SSID must be 1–{SSID_MAX} bytes (CAM appends -XXXXXX to the name).")
    if any(ch in forbidden for ch in ssid):
        die("SSID cannot contain newlines.")
    if not PASSWORD_MIN <= len(password) <= PASSWORD_MAX:
        die(f"Password must be {PASSWORD_MIN}–{PASSWORD_MAX} characters (WPA2).")
    if any(ch in forbidden for ch in password):
        die("Password cannot contain newlines.")


def resolve_wifi(ssid: str | None, password: str | None) -> tuple[str, str] | None:
    ssid = ssid or os.environ.get("GALAXY_AP_SSID") or None
    password = password or os.environ.get("GALAXY_AP_PASSWORD") or None
    if ssid is None and password is None:
        return None
    chosen_ssid = ssid or DEFAULT_AP_SSID
    chosen_password = password or DEFAULT_AP_PASSWORD
    validate_wifi(chosen_ssid, chosen_password)
    return chosen_ssid, chosen_password


def write_wifi_header(ssid: str, password: str) -> Path:
    WIFI_HEADER.parent.mkdir(parents=True, exist_ok=True)
    WIFI_HEADER.write_text(
        "#pragma once\n"
        f"#define AP_SSID {c_string(ssid)}\n"
        f"#define AP_PASSWORD {c_string(password)}\n"
        f"#define DEFAULT_AP_SSID {c_string(ssid)}\n"
        f"#define DEFAULT_AP_PASSWORD {c_string(password)}\n",
        encoding="utf-8",
    )
    return WIFI_HEADER


def wifi_include_flag(header: Path) -> str:
    return f"-include {header}"


def cam_version() -> str:
    for line in MAIN_CPP.read_text(encoding="utf-8").splitlines():
        if line.startswith("#define VERSION"):
            return line.split(None, 2)[2].strip().strip('"')
    return "unknown"


def parse_board_list(raw: str) -> list[tuple[str, str]]:
    """Return (address, board-name) pairs from `arduino-cli board list --format json`."""
    data = json.loads(raw)
    ports: list[tuple[str, str]] = []
    entries = data.get("detected_ports", data) if isinstance(data, dict) else data
    if not isinstance(entries, list):
        return ports
    for entry in entries:
        if not isinstance(entry, dict):
            continue
        port = entry.get("port") if isinstance(entry.get("port"), dict) else entry
        address = (port or {}).get("address") or entry.get("address")
        if not address:
            continue
        boards = entry.get("matching_boards") or entry.get("boards") or []
        names = [
            b.get("name") or b.get("fqbn") or ""
            for b in boards
            if isinstance(b, dict)
        ]
        label = next((n for n in names if n), "serial")
        if any(
            (b.get("fqbn") == UNO_FQBN) or ("Uno" in (b.get("name") or ""))
            for b in boards
            if isinstance(b, dict)
        ):
            label = "Arduino Uno"
        ports.append((address, label))
    return ports


def guess_usb_serial() -> list[str]:
    candidates: list[str] = []
    for pattern in ("/dev/cu.usbmodem*", "/dev/cu.usbserial*", "/dev/ttyACM*", "/dev/ttyUSB*"):
        candidates.extend(sorted(glob.glob(pattern)))
    # On Windows, arduino-cli is the source of truth; skip drive-letter globs.
    return candidates


def pick_uno_port(arduino_cli: str, requested: str | None) -> str:
    if requested:
        return requested
    listed: list[tuple[str, str]] = []
    try:
        raw = subprocess.check_output(
            [arduino_cli, "board", "list", "--format", "json"],
            text=True,
        )
        listed = parse_board_list(raw)
    except (subprocess.CalledProcessError, json.JSONDecodeError):
        listed = []
    unos = [addr for addr, name in listed if name == "Arduino Uno"]
    if len(unos) == 1:
        return unos[0]
    if len(listed) == 1:
        return listed[0][0]
    fallback = guess_usb_serial()
    if len(fallback) == 1 and not listed:
        return fallback[0]
    print("Serial ports:", file=sys.stderr)
    if listed:
        for addr, name in listed:
            print(f"  {addr}  ({name})", file=sys.stderr)
    elif fallback:
        for addr in fallback:
            print(f"  {addr}", file=sys.stderr)
    else:
        print("  (none found)", file=sys.stderr)
    die(
        "Pass -p / --port with the UNO USB port.\n"
        "Put the robot switch on Upload, plug in USB, then "
        "`arduino-cli board list`."
    )
    raise AssertionError("unreachable")


def in_docker() -> bool:
    return Path("/.dockerenv").exists() or os.environ.get("REMOTE_CONTAINERS") == "true"


def deploy_uno(port: str | None, wifi_header: Path | None) -> Path:
    arduino_cli = require("arduino-cli")
    if in_docker():
        print(
            "USB upload from Docker on macOS is unreliable. "
            "Run this script on the host if upload fails."
        )
    chosen = pick_uno_port(arduino_cli, port)
    UNO_OUT.mkdir(parents=True, exist_ok=True)
    compile_cmd = [
        arduino_cli,
        "compile",
        "--fqbn",
        UNO_FQBN,
        "--output-dir",
        str(UNO_OUT),
        str(UNO_SKETCH),
    ]
    if wifi_header:
        compile_cmd[4:4] = [
            "--build-property",
            f"compiler.cpp.extra_flags={wifi_include_flag(wifi_header)}",
        ]
        print(f"UNO Wi-Fi override  {wifi_header}")
    print(f"UNO compile  fqbn={UNO_FQBN}")
    run(compile_cmd)
    print(f"UNO upload   port={chosen}")
    print("Robot switch must be Upload. Close Serial Monitor if it is open.")
    run(
        [
            arduino_cli,
            "upload",
            "-p",
            chosen,
            "--fqbn",
            UNO_FQBN,
            "--input-dir",
            str(UNO_OUT),
            str(UNO_SKETCH),
        ]
    )
    hex_files = sorted(UNO_OUT.glob("*.hex"))
    print("UNO flashed. Switch the robot back to Run.")
    return hex_files[0] if hex_files else UNO_OUT


def deploy_cam(wifi_header: Path | None) -> Path:
    require("python3")
    pio = shutil.which("pio") or shutil.which("platformio")
    if not pio:
        die("Need pio (PlatformIO) on PATH.")
    pack = CAM_DIR / "tools" / "pack_c2.py"
    if pack.is_file():
        print("Packing C2 Blockly into firmware headers")
        run([sys.executable, str(pack)], cwd=CAM_DIR)
    print(f"Building ESP32-CAM ({CAM_ENV})")
    env = None
    if wifi_header:
        env = {"PLATFORMIO_BUILD_FLAGS": wifi_include_flag(wifi_header)}
        print(f"CAM Wi-Fi override  {wifi_header}")
    run([pio, "run", "-e", CAM_ENV], cwd=CAM_DIR, env=env)
    if not CAM_BUILD_BIN.is_file():
        die(f"Build produced no {CAM_BUILD_BIN}")
    size = CAM_BUILD_BIN.stat().st_size
    print(f"OTA image {size} bytes ({size / 1024:.1f} KiB)")
    if size > OTA_SLOT_BYTES:
        die(
            f"{CAM_BUILD_BIN} is {size} bytes; stock ESP32-CAM OTA slot is "
            f"{OTA_SLOT_BYTES} bytes (1280 KiB). Do not upload firmware.elf."
        )
    version = cam_version()
    firmware_dir = CAM_DIR / "firmware"
    firmware_dir.mkdir(parents=True, exist_ok=True)
    ota_named = firmware_dir / f"ai-camera-firmware.v{version}-ota.bin"
    root_bin = ROOT / "firmware.bin"
    shutil.copy2(CAM_BUILD_BIN, ota_named)
    shutil.copy2(CAM_BUILD_BIN, root_bin)
    print(f"Wrote {root_bin}")
    print(f"Wrote {ota_named}")
    print(
        "OTA: join the rover AP, open http://192.168.4.1, "
        "Update, choose firmware.bin only (not .elf)."
    )
    return root_bin


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Flash the Arduino UNO and package the ESP32-CAM OTA .bin.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument(
        "target",
        nargs="?",
        choices=("all", "uno", "cam"),
        default="all",
        help="all (default): UNO upload + CAM .bin. uno: USB flash only. "
        "cam: pack/build/copy firmware.bin only.",
    )
    parser.add_argument(
        "-p",
        "--port",
        help="UNO USB serial port (default: the only Arduino Uno arduino-cli sees).",
    )
    parser.add_argument(
        "--ssid",
        help="Soft-AP name (default GalaxyRVR, or GALAXY_AP_SSID). "
        f"1–{SSID_MAX} bytes; the CAM broadcasts <ssid>-XXXXXX.",
    )
    parser.add_argument(
        "--password",
        help="Soft-AP WPA2 password (default 12345678, or GALAXY_AP_PASSWORD). "
        f"{PASSWORD_MIN}–{PASSWORD_MAX} characters.",
    )
    args = parser.parse_args()
    wifi = resolve_wifi(args.ssid, args.password)
    header: Path | None = None
    if wifi:
        ssid, password = wifi
        header = write_wifi_header(ssid, password)
        print(f"Wi-Fi AP  {ssid}-XXXXXX  password {len(password)} chars")
        if args.target == "cam":
            print(
                "CAM-only deploy bakes factory-reset defaults. "
                "Flash the UNO too so boot SET+ matches this SSID."
            )
        if args.target == "uno":
            print(
                "UNO will SET+ this SSID on boot. OTA the CAM image from "
                "`deploy.py cam` with the same --ssid/--password so factory "
                "reset matches."
            )
    try:
        if args.target in ("all", "uno"):
            deploy_uno(args.port, header)
        if args.target in ("all", "cam"):
            deploy_cam(header)
    except DeployError as err:
        die(str(err))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
