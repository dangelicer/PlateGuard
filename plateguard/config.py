from pathlib import Path

BASE_DIR = Path(__file__).resolve().parent.parent


class Config:
    """Defaults. Any of these can be overridden with an environment variable
    prefixed PLATEGUARD_, e.g. PLATEGUARD_DATABASE=C:/data/test.db."""

    DATABASE = str(BASE_DIR / "database.db")

    # 0.0.0.0 so the ESP (and later, other laptops on the ESP's Wi-Fi) can reach us
    HOST = "0.0.0.0"
    PORT = 5000  # must match DB_SERVER_PORT in the ESP's network_config.h

    # Requests from the ESP are tiny; reject anything bigger than this (Flask returns 413)
    MAX_CONTENT_LENGTH = 1024

    # Matches the ESP's DETECTION_PLATE_LEN (32) minus the terminating '\0'
    PLATE_MAX_LEN = 31
