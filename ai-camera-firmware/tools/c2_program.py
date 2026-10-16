#!/usr/bin/env python3
"""Run C2 Blockly JSON programs ({version: 1, steps}) against a SimulatedRover."""

from __future__ import annotations

import argparse
import json
import re
import sys
import threading
import time
from pathlib import Path
from typing import Any, Optional

from c2_sim import SimulatedRover

_DURATION = re.compile(
    r"^([+-]?(?:\d+(?:\.\d*)?|\.\d+))(s|sec|ft|cm|deg)?$",
    re.IGNORECASE,
)
_TURN = re.compile(
    r"^(left|right)\s+([+-]?(?:\d+(?:\.\d*)?|\.\d+))deg$",
    re.IGNORECASE,
)

OPS = {
    "eq": lambda a, b: a == b,
    "neq": lambda a, b: a != b,
    "lt": lambda a, b: a < b,
    "lte": lambda a, b: a <= b,
    "gt": lambda a, b: a > b,
    "gte": lambda a, b: a >= b,
}

SENSOR_KEYS = ("distance_cm", "ir_left", "ir_right", "battery_v")
FOOT_CM = 30.48


class Stopped(Exception):
    """Program hit stop-all (mirrors the browser AbortError)."""


def run_program(
    sim: SimulatedRover,
    program: dict,
    *,
    abort: Optional[threading.Event] = None,
    forever_limit: Optional[int] = 100,
    wait_until_limit: Optional[int] = 1000,
) -> None:
    steps = program.get("steps", program if isinstance(program, list) else [])
    try:
        _run_steps(sim, steps, forever_limit, wait_until_limit, abort)
    except Stopped:
        sim.stop()


def _check(abort: Optional[threading.Event]) -> None:
    if abort is not None and abort.is_set():
        raise Stopped()


def _run_steps(
    sim: SimulatedRover,
    steps: Optional[list],
    forever_limit: Optional[int],
    wait_until_limit: Optional[int],
    abort: Optional[threading.Event],
) -> None:
    if not steps:
        return
    for step in steps:
        _check(abort)
        _run_step(sim, step, forever_limit, wait_until_limit, abort)


def _run_step(
    sim: SimulatedRover,
    step: dict,
    forever_limit: Optional[int],
    wait_until_limit: Optional[int],
    abort: Optional[threading.Event],
) -> None:
    if step.get("when") == "start" or "on_start" in step:
        _run_steps(sim, step.get("do") or step.get("on_start"), forever_limit, wait_until_limit, abort)
        return
    if "wait" in step:
        _sleep(sim, _wait_ms(step["wait"]), abort)
        return
    if "repeat" in step:
        times = int(_eval(sim, step["repeat"]) or 0)
        for _ in range(max(0, times)):
            _check(abort)
            sim.tick()
            _run_steps(sim, step.get("steps"), forever_limit, wait_until_limit, abort)
        return
    if "forever" in step:
        loops = 0
        while True:
            _check(abort)
            sim.tick()
            _run_steps(sim, step["forever"], forever_limit, wait_until_limit, abort)
            loops += 1
            if forever_limit is not None and loops >= forever_limit:
                raise RuntimeError(f"forever exceeded {forever_limit} iterations")
    if "if" in step:
        sim.tick()
        if _truthy(_eval(sim, step["if"])):
            _run_steps(sim, step.get("do"), forever_limit, wait_until_limit, abort)
        else:
            _run_steps(sim, step.get("else"), forever_limit, wait_until_limit, abort)
        return
    if "wait_until" in step:
        polls = 0
        while True:
            _check(abort)
            sim.tick()
            if _truthy(_eval(sim, step["wait_until"])):
                return
            _sleep(sim, 80, abort)
            polls += 1
            if wait_until_limit is not None and polls >= wait_until_limit:
                raise RuntimeError(f"wait_until exceeded {wait_until_limit} polls")
    if step.get("stop") == "all":
        sim.stop()
        raise Stopped()
    if step.get("stop") == "motors":
        sim.set_motors(0, 0)
        return
    if "move_forward" in step:
        _move(sim, "forward", step["move_forward"], abort)
        return
    if "move_backward" in step:
        _move(sim, "backward", step["move_backward"], abort)
        return
    if "turn" in step:
        _turn(sim, step["turn"], abort)
        return
    if "set_power" in step:
        sim.patch_settings({"move_power": int(_eval(sim, step["set_power"]))})
        return
    if "set_color" in step:
        color = step["set_color"]
        sim.set_rgb(_eval(sim, color[0]), _eval(sim, color[1]), _eval(sim, color[2]))
        return
    if "lamp" in step:
        sim.set_lamp(str(step["lamp"]).lower() == "on")
        return
    if "look" in step:
        sim.set_servo(_eval(sim, step["look"]))
        return
    raise ValueError(f"unknown C2 step: {step!r}")


def _move(sim: SimulatedRover, direction: str, spec: Any, abort: Optional[threading.Event] = None) -> None:
    amount, unit = _parse_move(spec)
    sim.tick()
    power = float(sim.settings.get("move_power") or 70)
    sign = -1 if direction == "backward" else 1
    ms_foot = float(sim.settings.get("ms_per_foot") or 1800)
    if unit == "ft":
        ms = amount * ms_foot
    elif unit == "cm":
        ms = amount * (ms_foot / FOOT_CM)
    else:
        ms = amount * 1000
    sim.set_motors(sign * power, sign * power)
    try:
        _sleep(sim, ms, abort)
    finally:
        sim.set_motors(0, 0)


def _turn(sim: SimulatedRover, spec: str, abort: Optional[threading.Event] = None) -> None:
    match = _TURN.match(str(spec).strip())
    if not match:
        raise ValueError(f"bad turn spec: {spec!r}")
    direction = match.group(1).lower()
    deg = float(match.group(2))
    sim.tick()
    power = float(sim.settings.get("move_power") or 70)
    ms = abs(deg) * float(sim.settings.get("ms_per_deg") or 8)
    left = -power if direction == "left" else power
    right = power if direction == "left" else -power
    sim.set_motors(left, right)
    try:
        _sleep(sim, ms, abort)
    finally:
        sim.set_motors(0, 0)


def _sleep(sim: SimulatedRover, ms: float, abort: Optional[threading.Event] = None) -> None:
    remaining = max(0.0, float(ms))
    if sim.clock == "wall":
        deadline = time.monotonic() + remaining / 1000.0
        while True:
            _check(abort)
            left = deadline - time.monotonic()
            if left <= 0:
                sim.tick()
                return
            time.sleep(min(0.05, left))
            sim.tick()
            sim.heartbeat()
        return
    timeout = sim.heartbeat_timeout_ms
    while remaining > 0:
        _check(abort)
        if timeout is not None:
            step = min(remaining, 200.0)
            sim.advance(step)
            sim.heartbeat()
            remaining -= step
        else:
            sim.advance(remaining)
            remaining = 0


def _wait_ms(spec: Any) -> float:
    if isinstance(spec, (int, float)):
        return float(spec) * 1000.0
    text = str(spec).strip()
    match = _DURATION.match(text)
    if not match:
        raise ValueError(f"bad wait spec: {spec!r}")
    return float(match.group(1)) * 1000.0


def _parse_move(spec: Any) -> tuple[float, str]:
    text = str(spec).strip()
    match = _DURATION.match(text)
    if not match or not match.group(2):
        raise ValueError(f"bad move spec: {spec!r}")
    unit = match.group(2).lower()
    if unit == "s":
        unit = "sec"
    return float(match.group(1)), unit


def _eval(sim: SimulatedRover, node: Any) -> Any:
    if node is None:
        return None
    if isinstance(node, (int, float, bool)):
        return node
    if isinstance(node, str):
        if node in SENSOR_KEYS:
            return {
                "distance_cm": sim.distance_cm,
                "ir_left": sim.ir_left,
                "ir_right": sim.ir_right,
                "battery_v": sim.battery_v,
            }[node]
        if node in sim.settings:
            return sim.settings[node]
        return node
    if isinstance(node, dict) and "op" in node:
        op = str(node["op"]).lower()
        if op not in OPS:
            raise ValueError(f"unknown compare op: {op}")
        return OPS[op](_eval(sim, node.get("a")), _eval(sim, node.get("b")))
    return node


def _truthy(value: Any) -> bool:
    return bool(value)


class OnboardRunner:
    """Persist a JSON program and run it on the sim without the browser."""

    def __init__(self, sim: SimulatedRover) -> None:
        self.sim = sim
        self.stored: Optional[dict] = None
        self.abort = threading.Event()
        self.thread: Optional[threading.Thread] = None
        self.lock = threading.Lock()

    def is_running(self) -> bool:
        thread = self.thread
        return thread is not None and thread.is_alive()

    def status(self, include_program: bool = False) -> dict:
        payload = {
            "deployed": self.stored is not None,
            "onboard": self.is_running(),
        }
        if include_program:
            payload["program"] = self.stored
        return payload

    def deploy(self, program: dict) -> None:
        if not isinstance(program, dict) or "steps" not in program:
            raise ValueError("program must be {version, steps}")
        self.stop()
        with self.lock:
            self.stored = program
            self.abort = threading.Event()
            self.thread = threading.Thread(target=self._run, daemon=True)
            self.thread.start()

    def stop(self) -> None:
        self.abort.set()
        thread = self.thread
        if thread is not None and thread is not threading.current_thread():
            thread.join(timeout=2)
        self.thread = None
        self.sim.stop()

    def clear(self) -> None:
        self.stop()
        with self.lock:
            self.stored = None

    def _run(self) -> None:
        program = self.stored
        if program is None:
            return
        try:
            run_program(
                self.sim,
                program,
                abort=self.abort,
                forever_limit=None,
                wait_until_limit=None,
            )
        except Exception:
            pass
        finally:
            if not self.abort.is_set():
                self.sim.stop()


def main(argv: Optional[list[str]] = None) -> int:
    parser = argparse.ArgumentParser(description="Run a C2 JSON program against the sim")
    parser.add_argument("program", type=Path, help="JSON file {version:1, steps:[...]}")
    parser.add_argument(
        "--distance",
        type=float,
        default=None,
        help="Inject ultrasonic distance (cm) before running",
    )
    args = parser.parse_args(argv)
    program = json.loads(args.program.read_text(encoding="utf-8"))
    sim = SimulatedRover(heartbeat_timeout_ms=None, kinematics=False, clock="virtual")
    if args.distance is not None:
        sim.inject_sensors(distance_cm=args.distance)
    run_program(sim, program)
    json.dump(sim.commands(), sys.stdout, indent=2)
    sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
