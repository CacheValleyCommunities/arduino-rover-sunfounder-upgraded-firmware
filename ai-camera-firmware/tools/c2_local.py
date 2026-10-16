#!/usr/bin/env python3
"""Serve the C2 Blockly UI locally against a SimulatedRover."""

from __future__ import annotations

import argparse
import json
import sys
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Optional
from urllib.parse import urlparse

from c2_program import OnboardRunner
from c2_sim import SimulatedRover
from pack_c2 import ensure_blockly

TOOLS = Path(__file__).resolve().parent
FIRMWARE = TOOLS.parent
WWW = FIRMWARE / "www-c2"

ASSETS = {
    "/c2": ("index.html", "text/html; charset=utf-8"),
    "/c2/": ("index.html", "text/html; charset=utf-8"),
    "/c2/c2.js": ("c2.js", "text/javascript; charset=utf-8"),
    "/c2/c2.css": ("c2.css", "text/css; charset=utf-8"),
    "/c2/blockly.js": ("blockly.min.js", "text/javascript; charset=utf-8"),
}


def make_handler(
    sim: SimulatedRover,
    www: Path,
    quiet: bool = False,
    runner: Optional[OnboardRunner] = None,
):
    if runner is None:
        runner = OnboardRunner(sim)
    class C2Handler(BaseHTTPRequestHandler):
        def log_message(self, fmt: str, *args) -> None:
            if quiet:
                return
            super().log_message(fmt, *args)

        def do_OPTIONS(self) -> None:  # noqa: N802
            if self._path().startswith("/c2/api"):
                self._send(204)
                return
            self._send(404, b"not found")

        def do_GET(self) -> None:  # noqa: N802
            path = self._path()
            if path in ASSETS:
                self._send_asset(path)
                return
            if path == "/c2/api/state":
                payload = sim.state()
                payload.update(runner.status())
                payload["running"] = bool(payload.get("running") or runner.is_running())
                self._send_json(payload)
                return
            if path == "/c2/api/program":
                self._send_json({"ok": True, **runner.status(include_program=True)})
                return
            if path == "/c2/api/_debug/log":
                self._send_json({"log": sim.commands()})
                return
            self._send(404, b"not found")

        def do_POST(self) -> None:  # noqa: N802
            path = self._path()
            body = self._read_json()
            if path == "/c2/api/heartbeat":
                sim.heartbeat()
                self._send_ok()
                return
            if path == "/c2/api/stop":
                runner.stop()
                self._send_ok()
                return
            if path == "/c2/api/motors":
                sim.set_motors(body.get("left", 0), body.get("right", 0))
                self._send_ok()
                return
            if path == "/c2/api/rgb":
                sim.set_rgb(body.get("r", 0), body.get("g", 0), body.get("b", 0))
                self._send_ok()
                return
            if path == "/c2/api/lamp":
                sim.set_lamp(body.get("on", False))
                self._send_ok()
                return
            if path == "/c2/api/servo":
                sim.set_servo(body.get("angle", 90))
                self._send_ok()
                return
            if path == "/c2/api/settings":
                sim.patch_settings(body)
                self._send_ok()
                return
            if path == "/c2/api/_debug/sensors":
                sim.inject_sensors(
                    distance_cm=body.get("distance_cm"),
                    ir_left=body.get("ir_left"),
                    ir_right=body.get("ir_right"),
                    battery_v=body.get("battery_v"),
                )
                self._send_ok()
                return
            if path == "/c2/api/_debug/pose":
                sim.reset_pose()
                self._send_ok()
                return
            if path == "/c2/api/_debug/reset":
                runner.stop()
                sim.reset()
                self._send_ok()
                return
            if path == "/c2/api/program":
                try:
                    runner.deploy(body)
                except (TypeError, ValueError):
                    self._send(400, b'{"ok":false,"error":"bad program"}', content_type="application/json")
                    return
                self._send_ok()
                return
            self._send(404, b"not found")

        def do_DELETE(self) -> None:  # noqa: N802
            if self._path() == "/c2/api/program":
                runner.clear()
                self._send_ok()
                return
            self._send(404, b"not found")

        def _path(self) -> str:
            path = urlparse(self.path).path
            if path == "/c2":
                return "/c2/"
            return path

        def _read_json(self) -> dict:
            length = int(self.headers.get("Content-Length", "0") or 0)
            raw = self.rfile.read(length) if length else b""
            if not raw.strip():
                return {}
            try:
                data = json.loads(raw.decode("utf-8"))
            except (UnicodeDecodeError, json.JSONDecodeError):
                return {}
            return data if isinstance(data, dict) else {}

        def _send_asset(self, path: str) -> None:
            name, mime = ASSETS[path]
            file_path = www / name
            if not file_path.exists():
                self._send(404, f"missing {name}".encode("utf-8"))
                return
            data = file_path.read_bytes()
            self._send(200, data, content_type=mime)

        def _send_ok(self) -> None:
            self._send_json({"ok": True})

        def _send_json(self, payload: dict) -> None:
            data = json.dumps(payload).encode("utf-8")
            self._send(200, data, content_type="application/json")

        def _cors(self) -> list[tuple[str, str]]:
            return [
                ("Access-Control-Allow-Origin", "*"),
                ("Access-Control-Allow-Methods", "OPTIONS, GET, POST, PUT, DELETE"),
                ("Access-Control-Allow-Headers", "Origin, X-Requested-With, Content-Type, Accept"),
            ]

        def _send(
            self,
            code: int,
            body: bytes = b"",
            *,
            content_type: Optional[str] = None,
        ) -> None:
            self.send_response(code)
            for key, value in self._cors():
                self.send_header(key, value)
            if code != 204:
                self.send_header("Content-Type", content_type or "text/plain; charset=utf-8")
                self.send_header("Content-Length", str(len(body)))
            self.send_header("Connection", "close")
            self.end_headers()
            if code != 204 and body:
                self.wfile.write(body)

    C2Handler.protocol_version = "HTTP/1.1"
    return C2Handler


class ReuseThreadingHTTPServer(ThreadingHTTPServer):
    allow_reuse_address = True


def start_server(
    sim: SimulatedRover,
    www: Path,
    *,
    host: str = "127.0.0.1",
    port: int = 0,
    quiet: bool = False,
) -> ThreadingHTTPServer:
    runner = OnboardRunner(sim)
    handler = make_handler(sim, www, quiet=quiet, runner=runner)
    httpd = ReuseThreadingHTTPServer((host, port), handler)
    httpd.runner = runner  # type: ignore[attr-defined]
    thread = threading.Thread(target=httpd.serve_forever, daemon=True)
    thread.start()
    httpd.thread = thread  # type: ignore[attr-defined]
    return httpd


def main(argv: Optional[list[str]] = None) -> int:
    parser = argparse.ArgumentParser(description="Run the C2 Blockly UI against a simulated rover")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--www", type=Path, default=WWW)
    args = parser.parse_args(argv)
    www = args.www.resolve()
    if not (www / "index.html").exists():
        print(f"missing C2 UI at {www}", file=sys.stderr)
        return 1
    ensure_blockly()
    sim = SimulatedRover(clock="wall", kinematics=True)
    runner = OnboardRunner(sim)
    handler = make_handler(sim, www, quiet=False, runner=runner)
    httpd = ReuseThreadingHTTPServer((args.host, args.port), handler)
    url = f"http://{args.host}:{args.port}/c2"
    print(f"C2 Blockly UI: {url}")
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print()
    finally:
        httpd.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
