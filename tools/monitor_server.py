"""Loopback-only, read-only HTTP bridge for the game's atomic JSON snapshots."""

import argparse
import json
import math
import os
from pathlib import Path
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlsplit

MAX_BYTES = 64 * 1024 * 1024
WEB_ROOT = Path(__file__).resolve().parents[1] / "monitor"


def reject_constant(value):
    raise ValueError("Non-finite JSON number: " + value)


def finite_float(value):
    result = float(value)
    if not math.isfinite(result):
        raise ValueError("Non-finite JSON number")
    return result


class SnapshotStore:
    def __init__(self, path):
        self.path = Path(path)
        self.lock = threading.Lock()
        self.signature = None
        self.snapshot = None
        self.bytes = 0

    def response(self, status, age=None, error=None):
        return {"status": status, "ageSeconds": age, "snapshot": self.snapshot,
                "bridge": {"service": "gse-world-monitor", "projectRoot": str(WEB_ROOT.parent),
                           "snapshotPath": str(self.path.resolve()), "snapshotBytes": self.bytes,
                           "serverTimeMs": int(time.time() * 1000), "error": error}}

    def state(self):
        with self.lock:
            try:
                stat = self.path.stat()
                if stat.st_size > MAX_BYTES:
                    raise ValueError("Snapshot exceeds 64 MiB")
                signature = (stat.st_mtime_ns, stat.st_size)
                if signature != self.signature:
                    with self.path.open("rb") as source:
                        raw = source.read(MAX_BYTES + 1)
                    if len(raw) > MAX_BYTES:
                        raise ValueError("Snapshot exceeds 64 MiB")
                    snapshot = json.loads(raw, parse_constant=reject_constant, parse_float=finite_float)
                    if (not isinstance(snapshot, dict)
                            or snapshot.get("schemaVersion") != 1
                            or snapshot.get("type") != "world.snapshot"
                            or not isinstance(snapshot.get("session"), str)
                            or not isinstance(snapshot.get("sequence"), int)
                            or type(snapshot.get("timestampMs")) not in (int, float)
                            or snapshot["timestampMs"] < 0
                            or not isinstance(snapshot.get("npcs"), list)
                            or not isinstance(snapshot.get("villages"), list)
                            or not isinstance(snapshot.get("events"), list)
                            or any(not isinstance(snapshot.get(key), dict)
                                   for key in ("counts", "player", "statistics", "combat"))):
                        raise ValueError("Unsupported snapshot schema")
                    self.snapshot = snapshot
                    self.signature = signature
                    self.bytes = len(raw)
                age = max(0, time.time() - self.snapshot["timestampMs"] / 1000)
                status = ("stopped" if self.snapshot.get("stopped") else "stale" if age > 5
                          else "paused" if self.snapshot.get("paused") else "live")
                return self.response(status, round(age, 2))
            except FileNotFoundError:
                return self.response("waiting", error="source_missing")
            except (OSError, ValueError, TypeError, OverflowError) as error:
                return self.response("error", error=type(error).__name__ + ": " + str(error)[:200])


class MonitorHandler(BaseHTTPRequestHandler):
    def respond(self, code, body, content_type):
        self.send_response(code)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.send_header("X-Content-Type-Options", "nosniff")
        self.send_header("X-Frame-Options", "DENY")
        self.send_header("Referrer-Policy", "no-referrer")
        self.send_header("Content-Security-Policy", "default-src 'self'; script-src 'self'; style-src 'self'; "
                         "connect-src 'self'; img-src 'self' data:; object-src 'none'; frame-ancestors 'none'")
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        port = self.server.server_address[1]
        allowed = {f"127.0.0.1:{port}", f"localhost:{port}"}
        origin = self.headers.get("Origin")
        if self.headers.get("Host") not in allowed or (origin and origin not in {"http://" + h for h in allowed}):
            self.respond(403, b"Local requests only", "text/plain; charset=utf-8")
            return
        path = urlsplit(self.path).path
        try:
            if path in ("/api/state", "/api/health"):
                state = self.server.store.state()
                if path == "/api/health":
                    snapshot = state.pop("snapshot")
                    state["sequence"] = snapshot.get("sequence") if snapshot else None
                    state["session"] = snapshot.get("session") if snapshot else None
                payload = json.dumps(state, ensure_ascii=True, allow_nan=False).encode("utf-8")
                self.respond(200, payload, "application/json; charset=utf-8")
                return
            assets = {"/": ("index.html", "text/html"), "/index.html": ("index.html", "text/html"),
                      "/dashboard.js": ("dashboard.js", "text/javascript"), "/styles.css": ("styles.css", "text/css")}
            if path not in assets:
                self.respond(404, b"Not found", "text/plain; charset=utf-8")
                return
            name, mime = assets[path]
            self.respond(200, (WEB_ROOT / name).read_bytes(), mime + "; charset=utf-8")
        except (BrokenPipeError, ConnectionResetError, TimeoutError):
            return
        except OSError:
            self.respond(500, b"Dashboard asset unavailable", "text/plain; charset=utf-8")

    def log_message(self, format, *args):
        pass


class MonitorServer(ThreadingHTTPServer):
    daemon_threads = True
    allow_reuse_address = False

    def get_request(self):
        connection, address = super().get_request()
        connection.settimeout(5)
        return connection, address


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=8765)
    base = Path(os.environ.get("LOCALAPPDATA", Path.home() / "AppData" / "Local"))
    parser.add_argument("--snapshot", type=Path, default=base / "GSE_Project" / "Monitor" / "snapshot.json")
    args = parser.parse_args()
    if not 1024 <= args.port <= 65535:
        parser.error("Port must be between 1024 and 65535")
    try:
        server = MonitorServer(("127.0.0.1", args.port), MonitorHandler)
    except OSError as error:
        parser.exit(1, f"Cannot bind local port {args.port}: {error}. Choose another --port.\n")
    server.store = SnapshotStore(args.snapshot)
    print(f"Monitor: http://127.0.0.1:{args.port}/", flush=True)
    print(f"Snapshot: {args.snapshot}\nStop with Ctrl+C.", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
