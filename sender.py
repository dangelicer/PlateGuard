""" send the result to the ESP32 lot server as JSON"""
import time
from datetime import datetime

import requests
import config
from voter import lookalikes

_last_sent = {} # plate -> last send time( 30s duplicate suppression


def build_payload(result):
    return {"plate": result["plate"], "timestamp": datetime.now().isoformat(timespec="seconds"),}

def send(payload):
    key = payload["plate"]
    if time.time() - _last_sent.get(key,0) < config.DUPLICATE_WINDOW_S:
        return "not resent (same plate sent in the last 30s)"    # car still at the gate
    _last_sent[key] = time.time()
    try:
        r = requests.post(config.LOT_SERVER_URL, json=payload, timeout=3)
        return r.text.strip()
    except requests.RequestException as e:
        print("lot server unreachable:",e)
        return None
    
    
def check_plate_direct(plate):
    for candidate in lookalikes(plate):
        r = requests.get(config.FLASK_CHECK_URL, params={"plate": candidate}, timeout = 3)
        if r.text.strip() == "TRUE":
            return candidate
    return None 