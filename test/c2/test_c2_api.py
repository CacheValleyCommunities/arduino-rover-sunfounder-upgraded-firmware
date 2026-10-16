#!/usr/bin/env python3
"""HTTP API parity for the local C2 server."""

from __future__ import annotations

import json
import sys
import time
import unittest
from pathlib import Path
from urllib.error import HTTPError
from urllib.request import Request, urlopen

ROOT = Path(__file__).resolve().parents[2]
TOOLS = ROOT / "ai-camera-firmware" / "tools"
WWW = ROOT / "ai-camera-firmware" / "www-c2"
sys.path.insert(0, str(TOOLS))

from c2_local import start_server  # noqa: E402
from c2_sim import SimulatedRover  # noqa: E402


class ApiTests(unittest.TestCase):
    def setUp(self) -> None:
        self.sim = SimulatedRover(
            heartbeat_timeout_ms=None,
            kinematics=False,
            clock="wall",
        )
        self.httpd = start_server(self.sim, WWW, host="127.0.0.1", port=0, quiet=True)
        host, port = self.httpd.server_address[:2]
        self.base = f"http://{host}:{port}"

    def tearDown(self) -> None:
        runner = getattr(self.httpd, "runner", None)
        if runner is not None:
            runner.clear()
        self.httpd.shutdown()
        self.httpd.server_close()

    def request(self, path: str, method: str = "GET", body=None):
        data = None if body is None else json.dumps(body).encode("utf-8")
        req = Request(self.base + path, data=data, method=method)
        if body is not None:
            req.add_header("Content-Type", "application/json")
        with urlopen(req) as res:
            raw = res.read()
            headers = dict(res.headers)
            payload = json.loads(raw.decode("utf-8")) if raw else None
            return res.status, payload, headers

    def test_state_defaults(self) -> None:
        status, payload, _ = self.request("/c2/api/state")
        self.assertEqual(status, 200)
        self.assertEqual(payload["sensors"]["distance_cm"], 80)
        self.assertFalse(payload["sensors"]["ir_left"])
        self.assertEqual(payload["settings"]["move_power"], 70)
        self.assertEqual(payload["settings"]["ms_per_foot"], 1800)
        self.assertFalse(payload["running"])
        preview = payload["preview"]
        self.assertEqual(preview["x_cm"], 0)
        self.assertEqual(preview["y_cm"], 0)
        self.assertEqual(preview["heading_deg"], 90)
        self.assertEqual(preview["action"], "stopped")
        self.assertEqual(preview["rgb"], [0, 0, 0])

    def test_motors_rgb_servo_lamp_settings(self) -> None:
        self.request("/c2/api/motors", "POST", {"left": 40, "right": -20})
        self.request("/c2/api/rgb", "POST", {"r": 255, "g": 0, "b": 8})
        self.request("/c2/api/servo", "POST", {"angle": 45})
        self.request("/c2/api/lamp", "POST", {"on": True})
        self.request("/c2/api/settings", "POST", {"move_power": 55})
        _, state, _ = self.request("/c2/api/state")
        self.assertEqual(self.sim.left, 40)
        self.assertEqual(self.sim.right, -20)
        self.assertEqual(self.sim.rgb, (255, 0, 8))
        self.assertEqual(self.sim.servo, 45)
        self.assertTrue(self.sim.lamp)
        self.assertEqual(state["settings"]["move_power"], 55)
        self.assertTrue(state["running"])
        self.assertEqual(state["preview"]["left"], 40)
        self.assertEqual(state["preview"]["right"], -20)
        self.assertEqual(state["preview"]["action"], "turn right")
        self.assertEqual(state["preview"]["rgb"], [255, 0, 8])
        self.assertEqual(state["preview"]["servo"], 45)
        self.assertTrue(state["preview"]["lamp"])
        _, log_payload, _ = self.request("/c2/api/_debug/log")
        paths = [entry["path"] for entry in log_payload["log"]]
        self.assertEqual(
            paths,
            [
                "/c2/api/motors",
                "/c2/api/rgb",
                "/c2/api/servo",
                "/c2/api/lamp",
                "/c2/api/settings",
            ],
        )

    def test_clamps(self) -> None:
        self.request("/c2/api/motors", "POST", {"left": 150, "right": -200})
        self.request("/c2/api/rgb", "POST", {"r": 300, "g": -4, "b": 9})
        self.request("/c2/api/servo", "POST", {"angle": 200})
        self.assertEqual((self.sim.left, self.sim.right), (100, -100))
        self.assertEqual(self.sim.rgb, (255, 0, 9))
        self.assertEqual(self.sim.servo, 140)

    def test_heartbeat_and_stop(self) -> None:
        _, payload, _ = self.request("/c2/api/heartbeat", "POST", {})
        self.assertEqual(payload, {"ok": True})
        _, state, _ = self.request("/c2/api/state")
        self.assertTrue(state["running"])
        self.request("/c2/api/motors", "POST", {"left": 30, "right": 30})
        self.request("/c2/api/stop", "POST", {})
        self.assertEqual(self.sim.left, 0)
        self.assertEqual(self.sim.right, 0)
        self.assertFalse(self.sim.running)

    def test_debug_sensors_and_reset(self) -> None:
        self.request(
            "/c2/api/_debug/sensors",
            "POST",
            {"distance_cm": 12, "ir_left": True, "battery_v": 7.1},
        )
        _, state, _ = self.request("/c2/api/state")
        self.assertEqual(state["sensors"]["distance_cm"], 12)
        self.assertTrue(state["sensors"]["ir_left"])
        self.assertEqual(state["sensors"]["battery_v"], 7.1)
        self.request("/c2/api/_debug/reset", "POST", {})
        _, state, _ = self.request("/c2/api/state")
        self.assertEqual(state["sensors"]["distance_cm"], 80)
        self.assertFalse(state["sensors"]["ir_left"])
        self.assertEqual(self.sim.commands(), [])

    def test_cors_options(self) -> None:
        req = Request(self.base + "/c2/api/motors", method="OPTIONS")
        with urlopen(req) as res:
            self.assertEqual(res.status, 204)
            self.assertEqual(res.headers.get("Access-Control-Allow-Origin"), "*")

    def test_reset_pose_endpoint(self) -> None:
        self.request("/c2/api/rgb", "POST", {"r": 1, "g": 2, "b": 3})
        self.request("/c2/api/_debug/pose", "POST", {})
        _, state, _ = self.request("/c2/api/state")
        self.assertEqual(state["preview"]["x_cm"], 0)
        self.assertEqual(state["preview"]["heading_deg"], 90)
        self.assertEqual(self.sim.rgb, (1, 2, 3))

    def test_index_and_js_assets(self) -> None:
        req = Request(self.base + "/c2")
        with urlopen(req) as res:
            html = res.read().decode("utf-8")
            self.assertIn('id="blocklyDiv"', html)
            self.assertIn('id="btnDeploy"', html)
            self.assertIn('id="blockHelp"', html)
            self.assertIn("IR left / right", html)
            self.assertIn("/c2/c2.js", html)
            self.assertIn("text/html", res.headers.get("Content-Type", ""))
        req = Request(self.base + "/c2/c2.js")
        with urlopen(req) as res:
            js = res.read().decode("utf-8")
            self.assertIn("generateProgram", js)
            self.assertIn("rover_lights", js)
            self.assertIn("COLOR_RGB", js)
            self.assertIn("BLOCK_HELP", js)
            self.assertIn("ignoreAbort", js)
            self.assertIn("text/javascript", res.headers.get("Content-Type", ""))

    def test_deploy_runs_onboard(self) -> None:
        program = {"version": 1, "steps": [{"lamp": "on"}, {"look": 45}]}
        self.request("/c2/api/program", "POST", program)
        deadline = time.time() + 2
        while time.time() < deadline:
            if self.sim.lamp and self.sim.servo == 45:
                break
            time.sleep(0.05)
        self.assertTrue(self.sim.lamp)
        self.assertEqual(self.sim.servo, 45)
        _, stored, _ = self.request("/c2/api/program")
        self.assertTrue(stored["deployed"])
        self.assertEqual(stored["program"]["steps"][0]["lamp"], "on")

    def test_deploy_stop_and_remove(self) -> None:
        self.request(
            "/c2/api/program",
            "POST",
            {"version": 1, "steps": [{"forever": [{"wait": "0.2s"}]}]},
        )
        deadline = time.time() + 2
        onboard = False
        while time.time() < deadline:
            _, state, _ = self.request("/c2/api/state")
            if state.get("onboard"):
                onboard = True
                break
            time.sleep(0.05)
        self.assertTrue(onboard)
        self.request("/c2/api/stop", "POST", {})
        deadline = time.time() + 2
        while time.time() < deadline:
            _, state, _ = self.request("/c2/api/state")
            if not state.get("onboard"):
                break
            time.sleep(0.05)
        _, state, _ = self.request("/c2/api/state")
        self.assertFalse(state["onboard"])
        self.assertTrue(state["deployed"])
        self.request("/c2/api/program", "DELETE")
        _, stored, _ = self.request("/c2/api/program")
        self.assertFalse(stored["deployed"])
        self.assertIsNone(stored["program"])

    def test_deploy_rejects_bad_program(self) -> None:
        with self.assertRaises(HTTPError) as raised:
            self.request("/c2/api/program", "POST", {"nope": 1})
        self.assertEqual(raised.exception.code, 400)
        raised.exception.close()

    def test_unknown_path_404(self) -> None:
        with self.assertRaises(HTTPError) as raised:
            urlopen(self.base + "/c2/nope")
        self.assertEqual(raised.exception.code, 404)
        raised.exception.close()


if __name__ == "__main__":
    unittest.main()
