"""clean up frame, read plate, and describe car"""
import cv2
import numpy as np
from fast_plate_ocr import LicensePlateRecognizer
from open_image_models import create_detector

import config

# pre-processing-------------------------------------------------
CLAHE = cv2.createCLAHE(clipLimit = 3.0, tileGridSize=(4, 4))

def blur_score(frame):
    """Laplacian variance: sharp frame = lots of edges = high score"""
    small = cv2.resize(frame, (640, 480))
    gray = cv2.cvtColor(small, cv2.COLOR_BGR2GRAY)
    return cv2.Laplacian(gray, cv2.CV_64F).var()


def clahe_color(crop):
    if crop.size == 0: return crop
    l, a, b = cv2.split(cv2.cvtColor(crop, cv2.COLOR_BGR2LAB))
    lab = cv2.merge((CLAHE.apply(l), a, b))
    return cv2.cvtColor(lab, cv2.COLOR_LAB2BGR)

# models (ONNX) -------------------------------------------------------------------------
plate_detector = create_detector(config.PLATE_MODEL) # Finds plate
vehicle_detector = None
ocr = LicensePlateRecognizer(config.OCR_MODEL) #reads plate

def _ocr(crop):
    if crop.size == 0: return "", 0.0 # box outside the frame
    rgb = cv2.cvtColor(crop, cv2.COLOR_BGR2RGB)
    pred = ocr.run_one(rgb, return_confidence=True)
    text = pred.plate.upper()
    conf = float(np.mean(pred.char_probs[: len(text)])) if text else 0.0
    return text, conf

def read_plate(frame):
    """find the plate, reads it raw, and with CLAHE, keeps the better read"""
    plates = plate_detector.predict(frame)
    if not plates:
        return None
    box = max(plates, key=lambda d: d.confidence).bounding_box
    crop = frame[max(box.y1, 0):box.y2, max(box.x1,0):box.x2]
    text, conf = max(_ocr(crop),_ocr(clahe_color(crop)), key=lambda r: r[1])
    return {"plate": text, "confidence": conf, "box": (box.x1, box.y1, box.x2, box.y2)}

#car info---------------------------------------------------------------------------------------------
HUES = [("red", 0, 10), ("orange", 10, 22), ("yellow", 22, 35), ("green", 35, 85), ("blue", 85, 130), ("purple", 130, 160), ("red", 160, 181)]
VEHICLE_TYPES = ("car", "truck", "bus", "motorcycle")  #COCO labels

def car_color(frame,box):
    x1, y1, x2, y2 = box
    w, h = x2 - x1, y2 - y1
    body = frame [y1 + int(.4 * h):y1 +int(.8 * h), x1 +int(.2 * w):x2 - int(.2 * w)]
    hsv = cv2.cvtColor(body, cv2.COLOR_BGR2HSV).reshape(-1,3).astype(int)
    hue, sat, val = hsv[:, 0], hsv[:, 1], hsv[:,2]
    
    colorful = (sat>= 70) & (val >= 60)
    if colorful.mean() > 0.35:
        counts = {}
        for name, lo, hi in HUES:
            n = int(((hue >= lo) & (hue< hi) & colorful).sum())
            counts[name] = counts.get(name, 0) + n
        return max(counts, key=counts.get)
    v = np.median(val)
    if v >= 215:
        return "white"
    return "silver" if v >= 130 else "gray" if v >= 70 else "black"

def describe_car(frame):
    """ car type is described from coco detector + color of pixels"""
    global vehicle_detector
    if vehicle_detector is None:
        vehicle_detector = create_detector(config.VEHICLE_MODEL)
    cars = [d for d in vehicle_detector.predict(frame) if d.label in VEHICLE_TYPES]
    if not cars:
        return{"type": "unknown", "color": "unknown"}
    best = max(cars, key=lambda d: d.bounding_box.area) #closest vehicle
    b = best.bounding_box
    return {"type": best.label, "color": car_color(frame, (b.x1, b.y1, b.x2, b.y2))}






