"""Admin commands for the plate database.

    python manage.py set-status TXA1234 AUTHORIZED
    python manage.py set-status TXA1234 UNAUTHORIZED
    python manage.py set-status NEW123 AUTHORIZED --create   # pre-authorize an unseen plate
    python manage.py show TXA1234
    python manage.py list
    python manage.py list --status UNAUTHORIZED

Safe to run while the server is running.
"""

import argparse
import sys
from contextlib import closing

from plateguard import create_app, db, plates
from plateguard.plates import PlateStatus

STATUS_CHOICES = [s.value for s in PlateStatus]


def print_table(rows):
    if not rows:
        print("(no plates)")
        return
    width = max(len("PLATE"), *(len(p.plate) for p in rows))
    print(f"{'PLATE':<{width}}  {'ATTEMPTS':>8}  {'STATUS':<12}  LAST CHANGED")
    for p in rows:
        print(f"{p.plate:<{width}}  {p.attempts:>8}  {p.status.value:<12}  {p.timestamp}")


def cmd_set_status(conn, args):
    plate = plates.set_status(conn, args.plate, args.status, create=args.create)
    if plate is None:
        print(f"Plate {args.plate!r} is not in the database. "
              f"Use --create to add it with status {args.status}.", file=sys.stderr)
        return 1
    print_table([plate])
    return 0


def cmd_show(conn, args):
    plate = plates.get_plate(conn, args.plate)
    if plate is None:
        print(f"Plate {args.plate!r} is not in the database.", file=sys.stderr)
        return 1
    print_table([plate])
    return 0


def cmd_list(conn, args):
    print_table(plates.list_plates(conn, args.status))
    return 0


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)

    p = sub.add_parser("set-status", help="set a plate's status")
    p.add_argument("plate")
    p.add_argument("status", type=str.upper, choices=STATUS_CHOICES)
    p.add_argument("--create", action="store_true", help="add the plate if it isn't in the database")
    p.set_defaults(func=cmd_set_status)

    p = sub.add_parser("show", help="show one plate")
    p.add_argument("plate")
    p.set_defaults(func=cmd_show)

    p = sub.add_parser("list", help="list all plates, most recently changed first")
    p.add_argument("--status", type=str.upper, choices=STATUS_CHOICES)
    p.set_defaults(func=cmd_list)

    args = parser.parse_args(argv)

    app = create_app()  # same config (and DB path) as the server; also creates the table
    with closing(db.connect(app.config["DATABASE"])) as conn:
        return args.func(conn, args)


if __name__ == "__main__":
    sys.exit(main())
