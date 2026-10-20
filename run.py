"""Start the PlateGuard server: python run.py"""

from plateguard import create_app

app = create_app()

if __name__ == "__main__":
    app.run(host=app.config["HOST"], port=app.config["PORT"])
