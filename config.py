""" Plateguard camera settings"""

# Camera (Arducam 8MP IMX219)
FRAME_SIZE = (1640, 1232)
LORES_SIZE = (320, 240)
FRAME_RATE = 15

# Motion Trigger
MOTION_THRESHOLD = 8.0
ROI = (0.15, 0.30, 0.85, 1.00) # for approach lane

# Pre-processing
BLUR_THRESHOLD = 140.0 #Laplacian variance (anything under this too blurry)
LOW_LIGHT_LUX = 20

# Models
PLATE_MODEL = "yolo-v9-t-384-license-plate-end2end" #finds plate
OCR_MODEL = "cct-xs-v2-global-model" #reads charecters
VEHICLE_MODEL = "rf-detr-nano-384-coco" #car and type

FRAMES_TO_VOTE = 5
RECOGNIZED_CONF = 0.90 # >= 0.90 RECOGNIZED
LOW_CONF = 0.60 
PLATE_PATTERN = r"^[A-Z0-9]{1,8}$" #1-8 char short + custom plates
DUPLICATE_WINDOW_S = 30 # dont resend the same plate within 30 s

# Network(ESP#@ lot server / Flask)
CAMERA_ID = "gate-1"
# ESP#@ access point
LOT_SERVER_URL = "http://192.168.4.1/api/detections"
# bench test: Flask app directly
FLASK_CHECK_URL = "http://192.168.137.1:5000/check_plate"

# GPIO
LED_PIN = 18