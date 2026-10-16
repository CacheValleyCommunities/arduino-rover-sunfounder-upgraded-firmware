#!/usr/bin/env python3
"""In-memory GalaxyRVR stand-in for the C2 Blockly UI and JSON harness."""

from __future__ import annotations

import math
import threading
import time
from dataclasses import dataclass
from typing import Any, Optional

SETTING_KEYS = (
    "follow_stop_cm",
    "follow_slow_cm",
    "follow_distance_cm",
    "follow_slow_power",
    "avoid_threshold_cm",
    "reverse_cm",
    "move_power",
    "ms_per_foot",
    "ms_per_deg",
)

SETTING_DEFAULTS = {
    "follow_stop_cm": 12,
    "follow_slow_cm": 25,
    "follow_distance_cm": 50,
    "follow_slow_power": 30,
    "avoid_threshold_cm": 35,
    "reverse_cm": 15,
    "move_power": 70,
    "ms_per_foot": 1800,
    "ms_per_deg": 8,
}

HEARTBEAT_TIMEOUT_MS = 400
DISTANCE_MIN_CM = 5.0
DISTANCE_MAX_CM = 250.0
FOOT_CM = 30.48
IR_RANGE_CM = 15.0
ROBOT_RADIUS_CM = 8.0
# Axis-aligned floor. Start at origin heading +y so the north wall is 80 cm ahead.
ARENA = (-125.0, -40.0, 125.0, 80.0)
TRAIL_MAX = 120


def clamp_int(value: Any, lo: int, hi: int) -> int:
    try:
        number = int(value)
    except (TypeError, ValueError):
        number = 0
    return max(lo, min(hi, number))


@dataclass
class Command:
    t_ms: float
    path: str
    body: dict

    def as_dict(self) -> dict:
        return {"t_ms": self.t_ms, "path": self.path, "body": self.body}


class SimulatedRover:
    """Firmware-shaped C2 state: sensors, settings, actuators, command log."""

    def __init__(
        self,
        *,
        heartbeat_timeout_ms: Optional[int] = HEARTBEAT_TIMEOUT_MS,
        kinematics: bool = True,
        clock: str = "virtual",
    ) -> None:
        if clock not in ("virtual", "wall"):
            raise ValueError("clock must be 'virtual' or 'wall'")
        self.heartbeat_timeout_ms = heartbeat_timeout_ms
        self.kinematics = kinematics
        self.clock = clock
        self._lock = threading.RLock()
        self._origin = time.monotonic()
        self.reset()

    def reset(self) -> None:
        with self._lock:
            self._now_ms = 0.0
            self._origin = time.monotonic()
            self._last_tick_ms = 0.0
            self._last_beat_ms = 0.0
            self.settings = dict(SETTING_DEFAULTS)
            self.distance_cm = 80.0
            self.ir_left = False
            self.ir_right = False
            self.battery_v = 7.85
            self.running = False
            self.left = 0
            self.right = 0
            self.rgb = (0, 0, 0)
            self.lamp = False
            self.servo = 90
            self.log: list[Command] = []
            self._freeze_sensors = False
            self.reset_pose()

    def now_ms(self) -> float:
        if self.clock == "virtual":
            return self._now_ms
        return (time.monotonic() - self._origin) * 1000.0

    def tick(self) -> None:
        with self._lock:
            now = self.now_ms()
            dt = now - self._last_tick_ms
            self._last_tick_ms = now
            if dt > 0:
                self._apply_kinematics(dt)
            timeout = self.heartbeat_timeout_ms
            if timeout is not None and self.running and (now - self._last_beat_ms) > timeout:
                self._drop_heartbeat()

    def advance(self, ms: float) -> None:
        if self.clock != "virtual":
            raise RuntimeError("advance() requires a virtual clock")
        if ms <= 0:
            self.tick()
            return
        with self._lock:
            self._now_ms += float(ms)
        self.tick()

    def reset_pose(self) -> None:
        with self._lock:
            self.x_cm = 0.0
            self.y_cm = 0.0
            self.heading_deg = 90.0
            self.trail = [[0.0, 0.0]]
            if not getattr(self, "_freeze_sensors", False):
                self.distance_cm = self._raycast(self.heading_deg)
                self.ir_left = False
                self.ir_right = False

    def inject_sensors(
        self,
        *,
        distance_cm: Optional[float] = None,
        ir_left: Optional[bool] = None,
        ir_right: Optional[bool] = None,
        battery_v: Optional[float] = None,
        freeze: bool = True,
    ) -> None:
        with self._lock:
            if distance_cm is not None:
                self.distance_cm = float(distance_cm)
            if ir_left is not None:
                self.ir_left = bool(ir_left)
            if ir_right is not None:
                self.ir_right = bool(ir_right)
            if battery_v is not None:
                self.battery_v = float(battery_v)
            if freeze:
                self._freeze_sensors = True

    def state(self) -> dict:
        self.tick()
        with self._lock:
            distance = self.distance_cm
            if distance < 0 or distance > 300:
                distance_out: Any = -1
            else:
                distance_out = distance
            return {
                "sensors": {
                    "distance_cm": distance_out,
                    "ir_left": self.ir_left,
                    "ir_right": self.ir_right,
                    "battery_v": self.battery_v,
                },
                "settings": dict(self.settings),
                "running": self.running,
                "preview": self._preview(),
            }

    def heartbeat(self) -> None:
        self.tick()
        with self._lock:
            self._last_beat_ms = self.now_ms()
            self.running = True

    def stop(self) -> None:
        self.tick()
        with self._lock:
            self.left = 0
            self.right = 0
            self.running = False
            self._log("/c2/api/stop", {})

    def set_motors(self, left: Any, right: Any, *, linked: bool = True) -> None:
        self.tick()
        with self._lock:
            self.left = clamp_int(left, -100, 100)
            self.right = clamp_int(right, -100, 100)
            if linked:
                self._link()
            self._log("/c2/api/motors", {"left": self.left, "right": self.right})

    def set_rgb(self, r: Any, g: Any, b: Any) -> None:
        self.tick()
        with self._lock:
            self._link()
            self.rgb = (
                clamp_int(r, 0, 255),
                clamp_int(g, 0, 255),
                clamp_int(b, 0, 255),
            )
            self._log("/c2/api/rgb", {"r": self.rgb[0], "g": self.rgb[1], "b": self.rgb[2]})

    def set_lamp(self, on: Any) -> None:
        self.tick()
        with self._lock:
            self._link()
            self.lamp = bool(on)
            self._log("/c2/api/lamp", {"on": self.lamp})

    def set_servo(self, angle: Any) -> None:
        self.tick()
        with self._lock:
            self._link()
            self.servo = clamp_int(angle if angle is not None else 90, 0, 140)
            self._log("/c2/api/servo", {"angle": self.servo})

    def patch_settings(self, body: dict) -> None:
        self.tick()
        with self._lock:
            self._link()
            applied: dict[str, int] = {}
            for key in SETTING_KEYS:
                if key not in body or body[key] is None:
                    continue
                value = int(body[key])
                self.settings[key] = value
                applied[key] = value
            if applied:
                self._log("/c2/api/settings", applied)

    def commands(self, path: Optional[str] = None) -> list[dict]:
        with self._lock:
            entries = [cmd.as_dict() for cmd in self.log]
        if path is None:
            return entries
        return [entry for entry in entries if entry["path"] == path]

    def _link(self) -> None:
        self._last_beat_ms = self.now_ms()
        self.running = True

    def _drop_heartbeat(self) -> None:
        self.left = 0
        self.right = 0
        self.running = False
        self._log("/c2/api/motors", {"left": 0, "right": 0})

    def _log(self, path: str, body: dict) -> None:
        self.log.append(Command(t_ms=self.now_ms(), path=path, body=dict(body)))

    def _preview(self) -> dict:
        x0, y0, x1, y1 = ARENA
        return {
            "x_cm": round(self.x_cm, 2),
            "y_cm": round(self.y_cm, 2),
            "heading_deg": round(self.heading_deg % 360.0, 2),
            "left": self.left,
            "right": self.right,
            "rgb": [self.rgb[0], self.rgb[1], self.rgb[2]],
            "lamp": self.lamp,
            "servo": self.servo,
            "action": self._action(),
            "bounds": {"x0": x0, "y0": y0, "x1": x1, "y1": y1},
            "trail": [point[:] for point in self.trail[-TRAIL_MAX:]],
        }

    def _action(self) -> str:
        left, right = self.left, self.right
        if abs(left) < 3 and abs(right) < 3:
            return "stopped"
        if left > 0 and right > 0:
            return "forward"
        if left < 0 and right < 0:
            return "backward"
        if right > left:
            return "turn left"
        return "turn right"

    def _raycast(self, heading_deg: float) -> float:
        x0, y0, x1, y1 = ARENA
        rad = math.radians(heading_deg)
        dx, dy = math.cos(rad), math.sin(rad)
        hits: list[float] = []
        if dx > 1e-9:
            hits.append((x1 - self.x_cm) / dx)
        elif dx < -1e-9:
            hits.append((x0 - self.x_cm) / dx)
        if dy > 1e-9:
            hits.append((y1 - self.y_cm) / dy)
        elif dy < -1e-9:
            hits.append((y0 - self.y_cm) / dy)
        ahead = [dist for dist in hits if dist > 0]
        if not ahead:
            return DISTANCE_MAX_CM
        return max(DISTANCE_MIN_CM, min(DISTANCE_MAX_CM, min(ahead)))

    def _apply_kinematics(self, dt_ms: float) -> None:
        if not self.kinematics or dt_ms <= 0:
            return
        power_ref = float(self.settings.get("move_power") or 70) or 70.0
        ms_foot = float(self.settings.get("ms_per_foot") or 1800)
        ms_deg = float(self.settings.get("ms_per_deg") or 8)
        avg = (self.left + self.right) / 2.0
        diff = (self.right - self.left) / 2.0
        linear_cm = (FOOT_CM / ms_foot) * (avg / power_ref) * dt_ms
        angular_deg = (diff / power_ref) * (dt_ms / ms_deg)
        if abs(linear_cm) < 1e-6 and abs(angular_deg) < 1e-6:
            return
        mid_heading = self.heading_deg + angular_deg / 2.0
        rad = math.radians(mid_heading)
        self.x_cm += linear_cm * math.cos(rad)
        self.y_cm += linear_cm * math.sin(rad)
        self.heading_deg = (self.heading_deg + angular_deg) % 360.0
        x0, y0, x1, y1 = ARENA
        self.x_cm = max(x0 + ROBOT_RADIUS_CM, min(x1 - ROBOT_RADIUS_CM, self.x_cm))
        self.y_cm = max(y0 + ROBOT_RADIUS_CM, min(y1 - ROBOT_RADIUS_CM, self.y_cm))
        last = self.trail[-1] if self.trail else None
        if last is None or abs(last[0] - self.x_cm) >= 0.8 or abs(last[1] - self.y_cm) >= 0.8:
            self.trail.append([round(self.x_cm, 2), round(self.y_cm, 2)])
            if len(self.trail) > TRAIL_MAX:
                self.trail = self.trail[-TRAIL_MAX:]
        if self._freeze_sensors:
            return
        self.distance_cm = self._raycast(self.heading_deg)
        self.ir_left = self._raycast(self.heading_deg + 90.0) < IR_RANGE_CM
        self.ir_right = self._raycast(self.heading_deg - 90.0) < IR_RANGE_CM
