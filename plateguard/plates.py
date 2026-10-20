"""The plates table and every operation on it.

No Flask in here: the API, the admin CLI and (later) the website all call
these functions with a sqlite3 connection.
"""

from dataclasses import dataclass
from datetime import datetime
from enum import Enum

from .db import transaction


class PlateStatus(str, Enum):
    UNAUTHORIZED = "UNAUTHORIZED"
    AUTHORIZED = "AUTHORIZED"


DEFAULT_STATUS = PlateStatus.UNAUTHORIZED

_STATUS_VALUES = ", ".join(f"'{s.value}'" for s in PlateStatus)

SCHEMA = f"""
CREATE TABLE IF NOT EXISTS plates (
    plate     TEXT    PRIMARY KEY,
    attempts  INTEGER NOT NULL DEFAULT 0 CHECK (attempts >= 0),
    timestamp TEXT    NOT NULL,
    status    TEXT    NOT NULL DEFAULT '{DEFAULT_STATUS.value}'
                      CHECK (status IN ({_STATUS_VALUES}))
);
"""


@dataclass
class Plate:
    plate: str
    attempts: int
    timestamp: str   # when this row was last changed, local time ISO 8601
    status: PlateStatus

    @property
    def authorized(self):
        return self.status is PlateStatus.AUTHORIZED

    def to_dict(self):
        return {
            "plate": self.plate,
            "attempts": self.attempts,
            "timestamp": self.timestamp,
            "status": self.status.value,
            "authorized": self.authorized,
        }


def _now():
    return datetime.now().astimezone().isoformat(timespec="seconds")


def _from_row(row):
    return Plate(row["plate"], row["attempts"], row["timestamp"], PlateStatus(row["status"]))


def _select(conn, plate):
    return conn.execute(
        "SELECT plate, attempts, timestamp, status FROM plates WHERE plate = ?", (plate,)
    ).fetchone()


def record_lookup(conn, plate):
    """Counts one lookup by the ESP. A known plate gets attempts + 1; an
    unknown plate gets a new row with attempts = 1 and the default status.

    Returns (Plate, existed) where existed says whether the plate was already
    in the database before this lookup.
    """
    now = _now()
    with transaction(conn):
        existed = _select(conn, plate) is not None
        if existed:
            conn.execute(
                "UPDATE plates SET attempts = attempts + 1, timestamp = ? WHERE plate = ?",
                (now, plate),
            )
        else:
            conn.execute(
                "INSERT INTO plates (plate, attempts, timestamp, status) VALUES (?, 1, ?, ?)",
                (plate, now, DEFAULT_STATUS.value),
            )
        row = _select(conn, plate)
    return _from_row(row), existed


def set_status(conn, plate, status, create=False):
    """Sets a plate's status. Returns the updated Plate, or None if the plate
    isn't in the database and create is False. With create=True a missing
    plate is added with attempts = 0 (e.g. to pre-authorize it)."""
    status = PlateStatus(status)
    now = _now()
    with transaction(conn):
        cur = conn.execute(
            "UPDATE plates SET status = ?, timestamp = ? WHERE plate = ?",
            (status.value, now, plate),
        )
        if cur.rowcount == 0:
            if not create:
                return None
            conn.execute(
                "INSERT INTO plates (plate, attempts, timestamp, status) VALUES (?, 0, ?, ?)",
                (plate, now, status.value),
            )
        row = _select(conn, plate)
    return _from_row(row)


def get_plate(conn, plate):
    row = _select(conn, plate)
    return _from_row(row) if row is not None else None


def list_plates(conn, status=None):
    """All plates, most recently changed first, optionally filtered by status."""
    if status is None:
        rows = conn.execute(
            "SELECT plate, attempts, timestamp, status FROM plates ORDER BY timestamp DESC"
        )
    else:
        rows = conn.execute(
            "SELECT plate, attempts, timestamp, status FROM plates WHERE status = ? "
            "ORDER BY timestamp DESC",
            (PlateStatus(status).value,),
        )
    return [_from_row(r) for r in rows]
