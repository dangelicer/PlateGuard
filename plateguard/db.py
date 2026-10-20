"""SQLite connection handling.

Each feature module owns its tables and passes its schema to init_app(), so
adding a table later (e.g. website users) doesn't touch this file.
"""

import sqlite3
from contextlib import closing, contextmanager

from flask import current_app, g


def connect(path):
    # isolation_level=None: no implicit transactions; writes use transaction() below
    conn = sqlite3.connect(path, isolation_level=None, timeout=5)
    conn.row_factory = sqlite3.Row
    return conn


def init_schema(conn, schemas):
    for schema in schemas:
        conn.executescript(schema)


@contextmanager
def transaction(conn):
    """BEGIN IMMEDIATE takes the write lock up front, so two requests for the
    same new plate can't both decide it doesn't exist yet."""
    conn.execute("BEGIN IMMEDIATE")
    try:
        yield conn
    except BaseException:
        conn.execute("ROLLBACK")
        raise
    conn.execute("COMMIT")


def get_db():
    """Connection for the current request, opened on first use."""
    if "db" not in g:
        g.db = connect(current_app.config["DATABASE"])
    return g.db


def _close_db(exc=None):
    conn = g.pop("db", None)
    if conn is not None:
        conn.close()


def init_app(app, schemas):
    with closing(connect(app.config["DATABASE"])) as conn:
        init_schema(conn, schemas)
    app.teardown_appcontext(_close_db)
