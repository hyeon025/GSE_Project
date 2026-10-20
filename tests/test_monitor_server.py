import importlib.util
import json
from pathlib import Path
import tempfile
import threading
import time
import unittest
import urllib.error
import urllib.request

MODULE_PATH = Path(__file__).resolve().parents[1] / "tools" / "monitor_server.py"
spec = importlib.util.spec_from_file_location("monitor_server", MODULE_PATH)
monitor = importlib.util.module_from_spec(spec)
spec.loader.exec_module(monitor)


def sample(**changes):
    value = {"schemaVersion": 1, "type": "world.snapshot", "session": "test", "sequence": 1,
             "timestampMs": int(time.time() * 1000), "paused": False, "stopped": False,
             "villages": [], "npcs": [], "events": [], "counts": {}, "player": {}, "statistics": {}, "combat": {}}
    value.update(changes)
    return value


class SnapshotTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.path = Path(self.directory.name) / "snapshot.json"
        self.store = monitor.SnapshotStore(self.path)

    def tearDown(self):
        self.directory.cleanup()

    def write(self, **changes):
        self.path.write_text(json.dumps(sample(**changes)), encoding="utf-8")
        self.store.signature = None

    def test_waiting_live_paused_stopped_and_stale(self):
        self.assertEqual(self.store.state()["status"], "waiting")
        for expected, fields in [("live", {}), ("paused", {"paused": True}),
                                 ("stopped", {"stopped": True}),
                                 ("stale", {"timestampMs": 1, "paused": True})]:
            self.write(**fields)
            self.assertEqual(self.store.state()["status"], expected)

    def test_invalid_and_recovery(self):
        self.path.write_text('{"broken":', encoding="utf-8")
        self.assertEqual(self.store.state()["status"], "error")
        self.write(schemaVersion=99)
        self.assertEqual(self.store.state()["status"], "error")
        self.write(timestampMs=float("inf"))
        self.assertEqual(self.store.state()["status"], "error")
        self.path.write_text(json.dumps(sample()).replace('"sequence": 1', '"sequence": 1e400'), encoding="utf-8")
        self.store.signature = None
        self.assertEqual(self.store.state()["status"], "error")
        self.write()
        self.assertEqual(self.store.state()["status"], "live")

    def test_http_routes_and_local_only(self):
        self.write()
        server = monitor.MonitorServer(("127.0.0.1", 0), monitor.MonitorHandler)
        server.store = self.store
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        base = f"http://127.0.0.1:{server.server_address[1]}"
        try:
            with urllib.request.urlopen(base + "/api/state") as response:
                self.assertEqual(json.load(response)["status"], "live")
                self.assertEqual(response.headers["Cache-Control"], "no-store")
            with urllib.request.urlopen(base + "/") as response:
                self.assertIn(b"dashboard.js", response.read())
            with urllib.request.urlopen(base + "/api/health") as response:
                health = json.load(response)
                self.assertNotIn("snapshot", health)
                self.assertEqual(health["status"], "live")
                self.assertEqual(health["sequence"], 1)
                self.assertEqual(health["session"], "test")
                self.assertEqual(health["bridge"]["service"], "gse-world-monitor")
                self.assertEqual(health["bridge"]["snapshotPath"], str(self.path.resolve()))
                self.assertEqual(health["bridge"]["snapshotBytes"], self.path.stat().st_size)
            for request, code in [
                (urllib.request.Request(base + "/api/state", headers={"Host": "untrusted.example"}), 403),
                (urllib.request.Request(base + "/api/state", headers={"Origin": "https://untrusted.example"}), 403),
                (urllib.request.Request(base + "/../SimpleGame/GameWorld.h"), 404),
            ]:
                with self.assertRaises(urllib.error.HTTPError) as error:
                    urllib.request.urlopen(request)
                self.assertEqual(error.exception.code, code)
        finally:
            server.shutdown()
            server.server_close()
            thread.join()

    def test_last_good_snapshot_and_diagnostics(self):
        self.write(sequence=7)
        good = self.store.state()
        self.assertIsNone(good["bridge"]["error"])
        self.path.write_text('{"broken":', encoding="utf-8")
        failed = self.store.state()
        self.assertEqual(failed["status"], "error")
        self.assertEqual(failed["snapshot"], good["snapshot"])
        self.assertIn("JSONDecodeError", failed["bridge"]["error"])
        self.path.unlink()
        missing = self.store.state()
        self.assertEqual(missing["status"], "waiting")
        self.assertEqual(missing["snapshot"], good["snapshot"])
        self.assertEqual(missing["bridge"]["error"], "source_missing")
        self.write(sequence=8)
        recovered = self.store.state()
        self.assertEqual(recovered["status"], "live")
        self.assertEqual(recovered["snapshot"]["sequence"], 8)
        self.assertIsNone(recovered["bridge"]["error"])


if __name__ == "__main__":
    unittest.main()
