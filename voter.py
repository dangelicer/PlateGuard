"""Vote across frames the confidence gate"""
import re
from collections import defaultdict
from itertools import product

import config

LOOKALIKE = {"0": "O", "O": "0", "1": "I", "I": "1", "8": "B", "B": "8", "5": "S", "S": "5", "2": "Z", "Z": "2"}


def clean (text):
    return re.sub(r"[^A-Z0-9]", "", text.upper()) #strip spaces



def lookalikes(plate, limit = 32):
    options = [(c, LOOKALIKE[c]) if c in LOOKALIKE else (c,) for c in plate]
    out = []
    for combo in product(*options):
        out.append("".join(combo))
        if len(out) >= limit:
            break
    return out


def vote(reads):
    """ basically votes on the whole string"""
    scores, counts = defaultdict(float), defaultdict(int)
    for r in reads:
        plate = clean(r["plate"])
        if re.match(config.PLATE_PATTERN, plate):   # 1-8 letters/digits
            scores[plate] += r["confidence"]
            counts[plate] += 1
    if not scores:
        return {"plate": "", "confidence": 0.0, "status": "UNKNOWN"}
    
    plate = max(scores, key = scores.get)
    agreement = counts[plate] / len(reads)     # 4 of 5 agree -> 0.8
    full_credit = min(1.0, agreement / 0.6)    #3+ of 5 = full credit
    conf = round(scores[plate] / counts[plate] * full_credit, 3)
    
    if conf >= config.RECOGNIZED_CONF:
        status = "RECOGNIZED"
    elif conf >= config.LOW_CONF:
        status = "LOW_CONFIDENCE"
    else:
        status = "UNKNOWN"
    return {"plate": plate, "confidence": conf, "status": status}