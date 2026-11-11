"""PlateGuard camera, waits for a car-> 5 sharp frames -> vote -> send"""
import time

from gpiozero import LED
import config
from camera import Camera
from sender import build_payload, send
from vision import blur_score, read_plate 
from voter import vote

cam = Camera ()
leds = LED (config.LED_PIN)
print("PlateGuard camera ready")

while True:
    if not cam.motion():
        time.sleep(0.05)
        continue
    
    reads, tries = [], 0
    while len(reads) < config.FRAMES_TO_VOTE and tries < 30:
        tries += 1
        frame, lux = cam.grab()
        if lux < config.LOW_LIGHT_LUX:
            leds.on()
        else:
            leds.off()
        if blur_score(frame) < config.BLUR_THRESHOLD:
            continue
        plate = read_plate(frame)
        if plate is None:
            continue
        reads.append(plate)
         
    if not reads:
        continue
    result = vote(reads)
    if result ["status"] != "RECOGNIZED":
        print("not sent:", result ["plate"] or "?", result["status"], result["confidence"])
        continue
    payload = build_payload(result)
    print(payload, "->", send(payload))
    