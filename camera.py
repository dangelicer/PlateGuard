import numpy as np
from picamera2 import Picamera2, Preview

import config
class Camera:
    def __init__(self, preview = False):
        self.picam2 = Picamera2()
        cfg = self.picam2.create_video_configuration(
            main={"size": config.FRAME_SIZE, "format": "RGB888"},
            lores={"size": config.LORES_SIZE, "format": "YUV420"}, #motion
            controls={"FrameRate": config.FRAME_RATE},
        )
        
        self.picam2.configure(cfg)
        self.picam2.start(show_preview=Preview.QT if preview else False) #preview window
        self.prev = None
    
    def motion(self):
        """true when something moves in approach lane (320x240 only)"""
        w, h = config.LORES_SIZE
        gray = self.picam2.capture_buffer("lores")[: w * h].reshape(h,w)
        x1, y1, x2, y2 =config.ROI
        lane = gray[int(y1 *h):int(y2 *h), int(x1 *w):int(x2 *w)] #ROI
        lane = lane.astype(np.int16)
        diff = 0 if self.prev is None else np.abs(lane - self.prev).mean()
        self.prev = lane
        return diff > config.MOTION_THRESHOLD
    
    def grab(self):
        """Full-res BGR grame and light level measure"""
        request = self.picam2.capture_request()
        frame = request.make_array("main")
        lux = request.get_metadata().get("Lux", 100.0)
        request.release()
        return frame, lux
    
    def close(self):
        self.picam2.stop()