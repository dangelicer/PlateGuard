# PlateGuard backend

Flask server that runs on the laptop connected to the ESP32's Wi-Fi (`my-esp`).
The ESP sends it every plate the Pi detects; it records each lookup and reports
the plate's status.

## Setup and running

```
python -m venv .venv
.venv\Scripts\activate
pip install -r requirements.txt
python run.py
```

The server listens on port 5000 on all interfaces. `database.db` is created
next to `run.py` on first start. Windows Firewall must allow Python on the
network the ESP's Wi-Fi is classed as (usually Public).

## Database

One table, `plates`, with one row per plate:

| Column      | Meaning                                                          |
|-------------|------------------------------------------------------------------|
| `plate`     | Plate string sent by the Pi (primary key, so unique)             |
| `attempts`  | How many times the ESP has looked this plate up                  |
| `timestamp` | Local date/time the row was last changed (ISO 8601)              |
| `status`    | `UNAUTHORIZED` (default) or `AUTHORIZED`; set by an admin        |

## Admin commands

Run from this folder (safe while the server is running):

```
python manage.py set-status TXA1234 AUTHORIZED
python manage.py set-status TXA1234 UNAUTHORIZED
python manage.py set-status NEW123 AUTHORIZED --create   # plate not seen yet
python manage.py show TXA1234
python manage.py list
python manage.py list --status UNAUTHORIZED
```

## API

`POST /api/check_plate` — called by the ESP for each detection.

```
request:  {"plate": "TXA1234"}
response: {"plate": "TXA1234", "exists": true, "attempts": 3,
           "status": "UNAUTHORIZED", "authorized": false,
           "timestamp": "2026-10-19T11:44:00-05:00"}
```

Each call counts as an attempt: a known plate's `attempts` goes up by 1, and
an unknown plate is added with `attempts` 1 and status `UNAUTHORIZED`.
`exists` says whether the plate was in the database *before* this call.
Plates are at most 31 characters and are matched exactly (case-sensitive).
Bad input gets `400 {"error": "..."}`; bodies over 1 KB get `413`.

## Layout

```
run.py                start the server
manage.py             admin commands
plateguard/
  __init__.py         create_app(): config, database, blueprints
  config.py           settings (override with PLATEGUARD_<NAME> env vars)
  db.py               SQLite connections and transactions
  plates.py           plates table and all operations on it (no Flask)
  api.py              /api routes
tests/                python -m unittest -v
```

### Adding features

- **Website** (pages loaded by other laptops on the ESP's Wi-Fi): add
  `plateguard/web.py` with a Blueprint, plus `plateguard/templates/` and
  `plateguard/static/`, and register it in `create_app()`. Pages can call
  functions in `plates.py` directly; JavaScript can use new JSON routes in
  `api.py`.
- **New table**: give the module a `SCHEMA` string like `plates.py` and add it
  to the `schemas` list in `create_app()`.
