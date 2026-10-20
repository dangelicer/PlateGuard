"""JSON API used by the ESP32 (and later, the website's JavaScript)."""

from flask import Blueprint, current_app, jsonify, request
from werkzeug.exceptions import RequestEntityTooLarge

from . import plates
from .db import get_db

bp = Blueprint("api", __name__, url_prefix="/api")


def _error(message, code):
    return jsonify(error=message), code


@bp.errorhandler(RequestEntityTooLarge)
def _too_large(e):
    return _error(f"Body larger than {current_app.config['MAX_CONTENT_LENGTH']} bytes", 413)


@bp.post("/check_plate")
def check_plate():
    """Called by the ESP for every detection from the Pi.

    Request:  {"plate": "TXA1234"}
    Response: {"plate": "TXA1234", "exists": true, "attempts": 3,
               "status": "UNAUTHORIZED", "authorized": false,
               "timestamp": "2026-10-19T11:44:00-05:00"}

    "exists" is whether the plate was already in the database before this
    lookup (the field the ESP currently reads). Every call is counted.
    """
    data = request.get_json(silent=True)
    plate = data.get("plate") if isinstance(data, dict) else None

    max_len = current_app.config["PLATE_MAX_LEN"]
    if not isinstance(plate, str) or not plate:
        return _error("Expected JSON with a non-empty string 'plate'", 400)
    if len(plate) > max_len:
        return _error(f"'plate' longer than {max_len} characters", 400)

    record, existed = plates.record_lookup(get_db(), plate)
    return jsonify(exists=existed, **record.to_dict())
