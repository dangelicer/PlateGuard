from flask import Flask, redirect

from . import api, db, plates, web
from .config import Config


def create_app(overrides=None):
    app = Flask(__name__)
    app.config.from_object(Config)
    app.config.from_prefixed_env("PLATEGUARD")  # e.g. PLATEGUARD_DATABASE=...
    if overrides:
        app.config.update(overrides)

    # Tables owned by each feature module; add new modules' schemas here.
    db.init_app(app, schemas=[plates.SCHEMA])

    app.register_blueprint(api.bp)   # /api/...        used by the ESP
    app.register_blueprint(web.bp)   # /dashboard/...  website, proxied by the ESP

    # Opening the laptop's own server in a browser lands on the dashboard.
    # Relative Location so it also works through the ESP's proxy.
    # (Flask's own /dashboard -> /dashboard/ redirect would be absolute, so
    # /dashboard gets an explicit one too.)
    app.add_url_rule("/", "root", lambda: redirect("/dashboard/"))
    app.add_url_rule("/dashboard", "dashboard_noslash", lambda: redirect("/dashboard/"))

    return app
