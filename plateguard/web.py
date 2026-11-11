"""The dashboard website, served at /dashboard/.

Laptops on the ESP's Wi-Fi open http://192.168.4.1/ ; the ESP forwards every
/dashboard/... request here and streams the response back in small chunks
(see web_proxy.c in the ESP code). The page's files live in ../dashboard/
(templates/index.html, static/style.css) so they can be edited there.

All URLs in the page are relative (static/style.css, api/status, ...) so they
resolve under /dashboard/ both through the ESP and when opened directly on
this laptop at http://localhost:5000/dashboard/.

JSON routes (read-only, built from the plates table):
  GET /dashboard/api/status    the most recently seen plate
  GET /dashboard/api/activity  the most recently seen plates
  GET /dashboard/api/alerts    unauthorized plates seen ALERT_THRESHOLD+ times
"""

from datetime import datetime

from flask import Blueprint, current_app, jsonify, render_template

from . import plates
from .config import BASE_DIR
from .db import get_db
from .plates import PlateStatus

DASHBOARD_DIR = BASE_DIR / "dashboard"

bp = Blueprint(
    "dashboard",
    __name__,
    url_prefix="/dashboard",
    template_folder=str(DASHBOARD_DIR / "templates"),
    static_folder=str(DASHBOARD_DIR / "static"),   # served at /dashboard/static/
)

ALERT_REASON = "Repeated Unauthorized Access"


def _display_time(iso):
    """'2026-10-19T11:44:00-05:00' -> '2026-10-19 11:44:00' (as the page expects)."""
    try:
        return datetime.fromisoformat(iso).strftime("%Y-%m-%d %H:%M:%S")
    except ValueError:
        return iso


def _row(p):
    return {
        "plate": p.plate,
        "status": p.status.value,
        "authorized": p.authorized,
        "attempts": p.attempts,
        "timestamp": _display_time(p.timestamp),
    }


def _is_alert(p):
    return (p.status is PlateStatus.UNAUTHORIZED
            and p.attempts >= current_app.config["ALERT_THRESHOLD"])


@bp.get("/")
def index():
    return render_template("index.html")


@bp.get("/api/status")
def status():
    latest = plates.recent_plates(get_db(), 1)
    if not latest:
        return jsonify(plate="-------", status="NO DETECTIONS", authorized=False,
                       attempts=0, alert=False, timestamp=None)
    p = latest[0]
    return jsonify(alert=_is_alert(p), **_row(p))


@bp.get("/api/activity")
def activity():
    rows = plates.recent_plates(get_db(), current_app.config["DASHBOARD_ROWS"])
    return jsonify([_row(p) for p in rows])


@bp.get("/api/alerts")
def alerts():
    rows = plates.recent_plates(
        get_db(),
        current_app.config["DASHBOARD_ROWS"],
        status=PlateStatus.UNAUTHORIZED,
        min_attempts=current_app.config["ALERT_THRESHOLD"],
    )
    return jsonify([dict(_row(p), reason=ALERT_REASON) for p in rows])
