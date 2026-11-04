# Import Flask tools needed for the PlateGuard server
from flask import Flask, render_template, jsonify

# Import datetime so vehicle detections can be timestamped
from datetime import datetime

# Import SQLite for permanent database storage
import sqlite3


# ---------------------------------------------------------
# CREATE FLASK APPLICATION
# ---------------------------------------------------------

app = Flask(__name__)


# ---------------------------------------------------------
# DATABASE SETUP
# ---------------------------------------------------------

DATABASE = "plateguard.db"


def initialize_database():

    # Connect to the SQLite database.
    # If it does not exist, Python creates it.
    connection = sqlite3.connect(DATABASE)

    cursor = connection.cursor()

    # Create the activity table if it does not exist.
    cursor.execute("""
        CREATE TABLE IF NOT EXISTS activity (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            plate TEXT NOT NULL,
            status TEXT NOT NULL,
            attempts INTEGER NOT NULL,
            timestamp TEXT NOT NULL
        )
    """)
        # Create a separate table for security alerts.
    cursor.execute("""
        CREATE TABLE IF NOT EXISTS alerts (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            plate TEXT NOT NULL,
            status TEXT NOT NULL,
            attempts INTEGER NOT NULL,
            reason TEXT NOT NULL,
            timestamp TEXT NOT NULL
        )
    """)
    connection.commit()

    connection.close()


# ---------------------------------------------------------
# SAVE DETECTION TO DATABASE
# ---------------------------------------------------------

def save_detection(detection):

    connection = sqlite3.connect(DATABASE)

    cursor = connection.cursor()

    # Store the vehicle detection in SQLite.
    cursor.execute("""
        INSERT INTO activity (
            plate,
            status,
            attempts,
            timestamp
        )
        VALUES (?, ?, ?, ?)
    """, (
        detection["plate"],
        detection["status"],
        detection["attempts"],
        detection["timestamp"]
    ))

    connection.commit()

    connection.close()


# ---------------------------------------------------------
# SAVE ALERT TO DATABASE
# ---------------------------------------------------------

def save_alert(detection):

    # Connect to the PlateGuard database.
    connection = sqlite3.connect(DATABASE)

    cursor = connection.cursor()

    # Store the alert information in the alerts table.
    cursor.execute("""
        INSERT INTO alerts (
            plate,
            status,
            attempts,
            reason,
            timestamp
        )
        VALUES (?, ?, ?, ?, ?)
    """, (
        detection["plate"],
        detection["status"],
        detection["attempts"],
        "Repeated Unauthorized Access",
        detection["timestamp"]
    ))

    connection.commit()

    connection.close()

# ---------------------------------------------------------
# GET UNAUTHORIZED ATTEMPT COUNT
# ---------------------------------------------------------

def get_attempt_count(plate_number):

    connection = sqlite3.connect(DATABASE)

    cursor = connection.cursor()

    # Count all previous unauthorized detections
    # for this specific license plate.
    cursor.execute("""
        SELECT COUNT(*)
        FROM activity
        WHERE plate = ?
        AND status = 'UNAUTHORIZED'
    """, (
        plate_number,
    ))

    count = cursor.fetchone()[0]

    connection.close()

    return count


# ---------------------------------------------------------
# SIMULATED PLATE DATA
# ---------------------------------------------------------
#
# This temporarily simulates information coming from
# the camera/OCR and authorization database.

test_plates = [

    {
        "plate": "ABC1234",
        "authorized": True,
        "status": "AUTHORIZED",
        "gate": "OPEN"
    },

    {
        "plate": "XYZ9876",
        "authorized": False,
        "status": "UNAUTHORIZED",
        "gate": "OPEN"
    },

    {
        "plate": "DEF5678",
        "authorized": True,
        "status": "AUTHORIZED",
        "gate": "OPEN"
    }

]


# ---------------------------------------------------------
# CURRENT PLATE
# ---------------------------------------------------------

current_plate_index = 0


# ---------------------------------------------------------
# ALERT SETTINGS
# ---------------------------------------------------------

# Generate an alert after 3 unauthorized attempts.
ALERT_THRESHOLD = 3

# ---------------------------------------------------------
# PROCESS VEHICLE DETECTION
# ---------------------------------------------------------

def create_detection(plate_data):
    plate_number = plate_data["plate"]
    attempt_count = 0
    alert = False

    # -----------------------------------------------------
    # AUTHORIZATION CHECK
    # -----------------------------------------------------

    if plate_data["authorized"]:
        # Authorized vehicles do not receive
        # unauthorized attempt counts.
        attempt_count = 0
        alert = False
    else:
        # Ask SQLite how many previous unauthorized
        # detections exist for this plate.
        previous_attempts = get_attempt_count(
            plate_number
        )
        # The current detection becomes the next attempt.
        attempt_count = previous_attempts + 1
        # -------------------------------------------------
        # ALERT CHECK
        # -------------------------------------------------
        if attempt_count >= ALERT_THRESHOLD:
            alert = True


    # -----------------------------------------------------
    # CREATE DETECTION RECORD
    # -----------------------------------------------------

    detection = {

        "plate":
            plate_number,

        "authorized":
            plate_data["authorized"],

        "status":
            plate_data["status"],

        "gate":
            plate_data["gate"],

        "attempts":
            attempt_count,

        "alert":
            alert,

        "timestamp":
            datetime.now().strftime(
                "%Y-%m-%d %H:%M:%S"
            )

    }


    # -----------------------------------------------------
    # SAVE DETECTION TO SQLITE
    # -----------------------------------------------------

    # Save every vehicle detection.
    save_detection(
        detection
    )


    # If this detection triggered the alert policy,
    # save it separately in the alerts table.
    if alert:

         save_alert(
        detection
    )


    return detection


# ---------------------------------------------------------
# HOME PAGE
# ---------------------------------------------------------

@app.route("/")
def home():

    return render_template(
        "index.html"
    )


# ---------------------------------------------------------
# CURRENT STATUS API
# ---------------------------------------------------------

@app.route("/api/status")
def status():

    current_plate = test_plates[
        current_plate_index
    ]

    plate_number = current_plate["plate"]


    # -----------------------------------------------------
    # ATTEMPT COUNT
    # -----------------------------------------------------

    if current_plate["authorized"]:

        attempts = 0

    else:

        # Read the current attempt count
        # directly from SQLite.
        attempts = get_attempt_count(
            plate_number
        )


    # -----------------------------------------------------
    # ALERT STATUS
    # -----------------------------------------------------

    alert = (

        not current_plate["authorized"]

        and

        attempts >= ALERT_THRESHOLD

    )


    return jsonify({
        "plate":
            plate_number,
        "authorized":
            current_plate["authorized"],
        "status":
            current_plate["status"],
        "gate":
            current_plate["gate"],
        "attempts":
            attempts,

        "alert":
            alert
    })


# ---------------------------------------------------------
# ACTIVITY HISTORY API
# ---------------------------------------------------------

@app.route("/api/activity")
def activity():

    connection = sqlite3.connect(DATABASE)

    cursor = connection.cursor()


    # Get the 5 most recent detections.
    cursor.execute("""
        SELECT plate, status, attempts, timestamp
        FROM activity
        ORDER BY id DESC
        LIMIT 5
    """)


    rows = cursor.fetchall()

    connection.close()
    # Convert SQLite rows into dictionaries
    # that JavaScript can understand.
    history = []
    for row in rows:
        history.append({
            "plate":
                row[0],
            "status":
                row[1],
            "attempts":
                row[2],
            "timestamp":
                row[3]
        })
    return jsonify(
        history
    )


# ---------------------------------------------------------
# ALERT HISTORY API
# ---------------------------------------------------------

@app.route("/api/alerts")
def alerts():

    connection = sqlite3.connect(DATABASE)

    cursor = connection.cursor()

    # Get the 5 most recent security alerts.
    cursor.execute("""
        SELECT plate, status, attempts, reason, timestamp
        FROM alerts
        ORDER BY id DESC
        LIMIT 5
    """)

    rows = cursor.fetchall()

    connection.close()

    # Convert SQLite rows into dictionaries
    # that JavaScript can understand.
    alert_history = []

    for row in rows:

        alert_history.append({

            "plate": row[0],

            "status": row[1],

            "attempts": row[2],

            "reason": row[3],

            "timestamp": row[4]

        })

    return jsonify(alert_history)

# ---------------------------------------------------------
# NEXT SIMULATED VEHICLE
# ---------------------------------------------------------

@app.route("/api/next")
def next_plate():

    global current_plate_index


    # Move to the next simulated vehicle.
    current_plate_index = (

        current_plate_index + 1

    ) % len(test_plates)


    # Get the selected vehicle.
    current_plate = test_plates[
        current_plate_index
    ]


    # Process the detection.
    detection = create_detection(
        current_plate
    )


    return jsonify(
        detection
    )


# ---------------------------------------------------------
# START SERVER
# ---------------------------------------------------------

if __name__ == "__main__":

    # Make sure the database exists before
    # starting the PlateGuard server.
    initialize_database()


    app.run(

        debug=True,

        host="0.0.0.0",

        port=5000

    )