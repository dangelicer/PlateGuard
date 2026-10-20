from flask import Flask

from . import api, db, plates
from .config import Config


def create_app(overrides=None):
    app = Flask(__name__)
    app.config.from_object(Config)
    app.config.from_prefixed_env("PLATEGUARD")  # e.g. PLATEGUARD_DATABASE=...
    if overrides:
        app.config.update(overrides)

    # Tables owned by each feature module; add new modules' schemas here.
    db.init_app(app, schemas=[plates.SCHEMA])

    # Routes. The website (pages served to other laptops on the ESP's Wi-Fi)
    # goes in its own blueprint registered here, with templates/ and static/.
    app.register_blueprint(api.bp)

    return app
