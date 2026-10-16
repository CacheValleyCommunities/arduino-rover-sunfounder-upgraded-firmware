#!/usr/bin/env python3
"""Probe a GalaxyRVR CAM UDP sidecar (port 30103).

Usage:
  python3 udp_probe.py              # broadcast discovery
  python3 udp_probe.py 192.168.4.1  # unicast probe + optional motor twitch
"""

from __future__ import annotations

import socket
import sys
import time

PORT = 30103
PROBE = b"GRVR?"


def discover(target: str | None, timeout: float = 2.0) -> None:
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    sock.settimeout(timeout)
    dest = target or "255.255.255.255"
    sock.sendto(PROBE, (dest, PORT))
    print(f"sent GRVR? to {dest}:{PORT}")
    deadline = time.time() + timeout
    seen: set[str] = set()
    while time.time() < deadline:
        try:
            data, addr = sock.recvfrom(1024)
        except socket.timeout:
            break
        key = f"{addr[0]}:{addr[1]}"
        if key in seen:
            continue
        seen.add(key)
        print(f"from {key}: {data.decode('utf-8', errors='replace')}")
    if not seen:
        print("no replies")
    sock.close()


def twitch(host: str) -> None:
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(1.0)
    motors = bytes([0x01, 40, 40])
    stop = bytes([0x01, 0, 0])
    sock.sendto(motors, (host, PORT))
    time.sleep(0.3)
    sock.sendto(stop, (host, PORT))
    print(f"sent motor twitch to {host}:{PORT}")
    sock.close()


if __name__ == "__main__":
    host = sys.argv[1] if len(sys.argv) > 1 else None
    discover(host)
    if host:
        twitch(host)
