#!/usr/bin/env python3
"""JSON IR programs against SimulatedRover (virtual time, no HTTP)."""

from __future__ import annotations

import json
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TOOLS = ROOT / "ai-camera-firmware" / "tools"
FIXTURES = Path(__file__).resolve().parent / "fixtures"
sys.path.insert(0, str(TOOLS))

from c2_program import run_program  # noqa: E402
from c2_sim import SimulatedRover  # noqa: E402


def load_fixture(name: str) -> dict:
    return json.loads((FIXTURES / name).read_text(encoding="utf-8"))


def motor_bodies(sim: SimulatedRover) -> list[dict]:
    return [entry["body"] for entry in sim.commands("/c2/api/motors")]


class ProgramTests(unittest.TestCase):
    def setUp(self) -> None:
        self.sim = SimulatedRover(
            heartbeat_timeout_ms=None,
            kinematics=False,
            clock="virtual",
        )

    def test_seed_forward_then_right_turn(self) -> None:
        run_program(self.sim, load_fixture("seed.json"))
        self.assertEqual(
            motor_bodies(self.sim),
            [
                {"left": 70, "right": 70},
                {"left": 0, "right": 0},
                {"left": 70, "right": -70},
                {"left": 0, "right": 0},
            ],
        )
        self.assertEqual(self.sim.now_ms(), 1800 + 90 * 8)

    def test_on_start_hat_unwraps_body(self) -> None:
        run_program(self.sim, load_fixture("on_start.json"))
        self.assertEqual(
            motor_bodies(self.sim),
            [
                {"left": 70, "right": 70},
                {"left": 0, "right": 0},
                {"left": 70, "right": -70},
                {"left": 0, "right": 0},
            ],
        )

    def test_avoid_turns_when_close(self) -> None:
        self.sim.inject_sensors(distance_cm=10)
        run_program(self.sim, load_fixture("avoid.json"))
        self.assertEqual(
            motor_bodies(self.sim),
            [
                {"left": -70, "right": 70},
                {"left": 0, "right": 0},
            ],
        )

    def test_avoid_moves_when_clear(self) -> None:
        self.sim.inject_sensors(distance_cm=80)
        run_program(self.sim, load_fixture("avoid.json"))
        self.assertEqual(
            motor_bodies(self.sim),
            [
                {"left": 70, "right": 70},
                {"left": 0, "right": 0},
            ],
        )

    def test_lights_look_and_color(self) -> None:
        run_program(self.sim, load_fixture("lights.json"))
        self.assertTrue(self.sim.lamp)
        self.assertEqual(self.sim.servo, 45)
        self.assertEqual(self.sim.rgb, (255, 0, 0))
        self.assertEqual(
            [entry["path"] for entry in self.sim.commands()],
            ["/c2/api/lamp", "/c2/api/servo", "/c2/api/rgb"],
        )

    def test_set_power_changes_move(self) -> None:
        run_program(
            self.sim,
            {
                "version": 1,
                "steps": [
                    {"set_power": 40},
                    {"move_forward": "1sec"},
                ],
            },
        )
        self.assertEqual(self.sim.settings["move_power"], 40)
        self.assertEqual(
            motor_bodies(self.sim),
            [
                {"left": 40, "right": 40},
                {"left": 0, "right": 0},
            ],
        )

    def test_stop_all_halts_later_steps(self) -> None:
        run_program(
            self.sim,
            {
                "version": 1,
                "steps": [
                    {"lamp": "on"},
                    {"stop": "all"},
                    {"look": 20},
                ],
            },
        )
        self.assertTrue(self.sim.lamp)
        self.assertEqual(self.sim.servo, 90)
        self.assertEqual(self.sim.left, 0)
        self.assertFalse(self.sim.running)

    def test_heartbeat_timeout_zeros_motors(self) -> None:
        timed = SimulatedRover(heartbeat_timeout_ms=400, kinematics=False, clock="virtual")
        timed.set_motors(70, 70)
        timed.advance(500)
        self.assertEqual(timed.left, 0)
        self.assertEqual(timed.right, 0)
        self.assertFalse(timed.running)

    def test_kinematics_close_on_forward(self) -> None:
        moving = SimulatedRover(heartbeat_timeout_ms=None, kinematics=True, clock="virtual")
        start = moving.distance_cm
        moving.set_motors(70, 70)
        moving.advance(1800)
        self.assertAlmostEqual(moving.distance_cm, start - 30.48, places=4)

    def test_preview_pose_forward_then_right(self) -> None:
        moving = SimulatedRover(heartbeat_timeout_ms=None, kinematics=True, clock="virtual")
        moving.set_motors(70, 70)
        moving.advance(1800)
        self.assertAlmostEqual(moving.x_cm, 0.0, places=1)
        self.assertAlmostEqual(moving.y_cm, 30.48, places=1)
        self.assertAlmostEqual(moving.heading_deg, 90.0, places=1)
        moving.set_motors(70, -70)
        moving.advance(90 * 8)
        self.assertAlmostEqual(moving.heading_deg % 360.0, 0.0, places=1)
        preview = moving.state()["preview"]
        self.assertEqual(preview["action"], "turn right")
        self.assertGreater(len(preview["trail"]), 1)

    def test_reset_pose_returns_to_origin(self) -> None:
        moving = SimulatedRover(heartbeat_timeout_ms=None, kinematics=True, clock="virtual")
        moving.set_motors(70, 70)
        moving.advance(1800)
        moving.reset_pose()
        self.assertEqual(moving.x_cm, 0.0)
        self.assertEqual(moving.y_cm, 0.0)
        self.assertEqual(moving.heading_deg, 90.0)


if __name__ == "__main__":
    unittest.main()
