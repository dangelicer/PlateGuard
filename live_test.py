import time
import config
from camera import Camera
from vision import blur_score, read_plate
from voter import vote

cam = Camera(preview=True)
print("Live Test running - point the camera at a plate")
reads = []
try:
    while True:
        frame, lux = cam.grab()
        blur = blur_score(frame)
        flag = "  (main.py would skip: blurry)" if blur < config.BLUR_THRESHOLD else ""
        line = f"blur {blur:6.1f}  lux {lux:5.0f}  "
        plate = read_plate(frame)
        if plate is None:
            print(line + "no plate found" + flag)
        else:
            print(line + f"plate {plate['plate']:<9} conf {plate['confidence']:.2f}" + flag)
            reads.append(plate)
            if len(reads) == config.FRAMES_TO_VOTE:
                print("   5-frame vote ->", vote(reads))
                reads =[]
        time.sleep(1)
finally:
    cam.close() 