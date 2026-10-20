"""Run from plateguard_backend:  python -m unittest -v"""

import io
import os
import sqlite3
import tempfile
import threading
import time
import unittest
from contextlib import closing, redirect_stderr, redirect_stdout

import manage
from plateguard import create_app, db, plates
from plateguard.plates import PlateStatus


class TempDbTestCase(unittest.TestCase):
    def setUp(self):
        fd, self.db_path = tempfile.mkstemp(suffix=".db")
        os.close(fd)
        self.app = create_app({"DATABASE": self.db_path, "TESTING": True})
        self.client = self.app.test_client()

    def tearDown(self):
        os.remove(self.db_path)

    def conn(self):
        return closing(db.connect(self.db_path))


class CheckPlateApiTests(TempDbTestCase):
    def check(self, plate):
        return self.client.post("/api/check_plate", json={"plate": plate})

    def test_new_plate_creates_row(self):
        r = self.check("TXA1234")
        self.assertEqual(r.status_code, 200)
        body = r.get_json()
        self.assertIs(body["exists"], False)
        self.assertEqual(body["attempts"], 1)
        self.assertEqual(body["status"], "UNAUTHORIZED")
        self.assertIs(body["authorized"], False)
        self.assertEqual(body["plate"], "TXA1234")

    def test_repeat_lookup_increments_attempts(self):
        self.check("TXA1234")
        self.check("TXA1234")
        body = self.check("TXA1234").get_json()
        self.assertIs(body["exists"], True)
        self.assertEqual(body["attempts"], 3)
        with self.conn() as c:
            count = c.execute("SELECT COUNT(*) FROM plates WHERE plate = 'TXA1234'").fetchone()[0]
        self.assertEqual(count, 1)

    def test_lookup_updates_timestamp(self):
        first = self.check("TXA1234").get_json()["timestamp"]
        time.sleep(1.1)  # timestamps have one-second resolution
        second = self.check("TXA1234").get_json()["timestamp"]
        self.assertGreater(second, first)

    def test_authorized_plate(self):
        self.check("TXA1234")
        with self.conn() as c:
            plates.set_status(c, "TXA1234", PlateStatus.AUTHORIZED)
        body = self.check("TXA1234").get_json()
        self.assertEqual(body["status"], "AUTHORIZED")
        self.assertIs(body["authorized"], True)
        self.assertEqual(body["attempts"], 2)

    def test_plates_are_distinct(self):
        self.check("TXA1234")
        body = self.check("ZZZ9999").get_json()
        self.assertIs(body["exists"], False)
        self.assertEqual(body["attempts"], 1)

    def test_response_fits_esp_buffer(self):
        # the ESP reads at most 255 bytes of the response
        r = self.check("X" * 31)
        self.assertLess(len(r.data), 256)

    def test_bad_requests(self):
        cases = [
            ({"json": {}}, 400),
            ({"json": {"plate": ""}}, 400),
            ({"json": {"plate": 1234}}, 400),
            ({"json": {"plate": "X" * 32}}, 400),
            ({"json": ["TXA1234"]}, 400),
            ({"data": "plate=TXA1234", "content_type": "text/plain"}, 400),
            ({"json": {"plate": "X" * 2000}}, 413),
        ]
        for kwargs, code in cases:
            with self.subTest(kwargs=str(kwargs)[:60]):
                r = self.client.post("/api/check_plate", **kwargs)
                self.assertEqual(r.status_code, code)
                self.assertIn("error", r.get_json())
        with self.conn() as c:
            self.assertEqual(c.execute("SELECT COUNT(*) FROM plates").fetchone()[0], 0)

    def test_get_not_allowed(self):
        self.assertEqual(self.client.get("/api/check_plate").status_code, 405)


class PlatesModuleTests(TempDbTestCase):
    def test_set_status_missing_plate(self):
        with self.conn() as c:
            self.assertIsNone(plates.set_status(c, "NOPE", "AUTHORIZED"))
            self.assertIsNone(plates.get_plate(c, "NOPE"))

    def test_set_status_create(self):
        with self.conn() as c:
            p = plates.set_status(c, "NEW123", "AUTHORIZED", create=True)
            self.assertEqual((p.attempts, p.status), (0, PlateStatus.AUTHORIZED))
            p, existed = plates.record_lookup(c, "NEW123")
        self.assertTrue(existed)
        self.assertEqual((p.attempts, p.status), (1, PlateStatus.AUTHORIZED))

    def test_invalid_status_rejected(self):
        with self.conn() as c:
            plates.record_lookup(c, "TXA1234")
            with self.assertRaises(ValueError):
                plates.set_status(c, "TXA1234", "MAYBE")
            with self.assertRaises(sqlite3.IntegrityError):  # the CHECK constraint backs it up
                c.execute("UPDATE plates SET status = 'MAYBE'")

    def test_concurrent_lookups_of_new_plate(self):
        n = 20
        errors = []

        def worker():
            try:
                with self.conn() as c:
                    plates.record_lookup(c, "RACE1")
            except Exception as e:  # noqa: BLE001
                errors.append(e)

        threads = [threading.Thread(target=worker) for _ in range(n)]
        for t in threads:
            t.start()
        for t in threads:
            t.join()

        self.assertEqual(errors, [])
        with self.conn() as c:
            self.assertEqual(plates.get_plate(c, "RACE1").attempts, n)

    def test_list_filter(self):
        with self.conn() as c:
            plates.record_lookup(c, "AAA111")
            plates.record_lookup(c, "BBB222")
            plates.set_status(c, "BBB222", "AUTHORIZED")
            self.assertEqual([p.plate for p in plates.list_plates(c, "AUTHORIZED")], ["BBB222"])
            self.assertEqual(len(plates.list_plates(c)), 2)


class ManageCliTests(TempDbTestCase):
    def run_cli(self, *argv):
        out, err = io.StringIO(), io.StringIO()
        os.environ["PLATEGUARD_DATABASE"] = self.db_path
        try:
            with redirect_stdout(out), redirect_stderr(err):
                code = manage.main(list(argv))
        finally:
            del os.environ["PLATEGUARD_DATABASE"]
        return code, out.getvalue(), err.getvalue()

    def test_set_status_flow(self):
        self.client.post("/api/check_plate", json={"plate": "TXA1234"})

        code, out, _ = self.run_cli("set-status", "TXA1234", "authorized")  # case-insensitive
        self.assertEqual(code, 0)
        self.assertIn("AUTHORIZED", out)

        code, out, _ = self.run_cli("show", "TXA1234")
        self.assertEqual(code, 0)
        self.assertIn("TXA1234", out)

        code, out, _ = self.run_cli("list", "--status", "UNAUTHORIZED")
        self.assertIn("(no plates)", out)

    def test_set_status_unknown_plate(self):
        code, _, err = self.run_cli("set-status", "NOPE", "AUTHORIZED")
        self.assertEqual(code, 1)
        self.assertIn("--create", err)

        code, _, _ = self.run_cli("set-status", "NOPE", "AUTHORIZED", "--create")
        self.assertEqual(code, 0)


if __name__ == "__main__":
    unittest.main()
