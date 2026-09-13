"""Summarize a completed M1 soak between explicit '?' checkpoints.

Selects the first two status queries after --baseline-after seconds.
Rejects fault counters, counter regression, reset/panic, and short duration.
This checks software evidence only, not acoustic quality or clock accuracy.
"""
import argparse
import json
import re
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument("capture")
p.add_argument("--baseline-after", type=float, default=20)
p.add_argument("--minimum-seconds", type=float, default=600)
a = p.parse_args()
checkpoints = []
pending = None
samples = []
text = Path(a.capture).read_text(encoding="utf-8")
for line in text.splitlines():
    host = re.match(r"HOST command=(.) elapsed=([\d.]+)s", line)
    if host:
        pending = float(host[2]) if host[1] == "?" and float(host[2]) >= a.baseline_after else None
    if "M1: blocks=" in line:
        fields = {k: int(v) for k, v in re.findall(r"(\w+)=(\d+)", line)}
        fields["device_ms"] = int(re.search(r"I \((\d+)\)", line)[1])
        samples.append(fields)
        if pending is not None:
            checkpoints.append({"host_seconds": pending, **fields})
            pending = None
if len(checkpoints) < 2:
    raise SystemExit("Capture lacks two soak checkpoints")
start, end = checkpoints[:2]
window = [s for s in samples if start["device_ms"] <= s["device_ms"] <= end["device_ms"]]
fault_fields = ("deadlines", "write_errors", "short", "tx_q_ovf", "gaps")
faults = []
for key in fault_fields:
    values = [s[key] for s in window]
    if any(x != 0 for x in values):
        faults.append(f"nonzero {key}")
for key in ("blocks", "dma", *fault_fields):
    values = [s[key] for s in window]
    if any(y < x for x, y in zip(values, values[1:])):
        faults.append(f"counter regression: {key}")
if any(s["failed"] or not s["running"] for s in window):
    faults.append("stream failed or stopped in soak window")
duration = (end["device_ms"] - start["device_ms"]) / 1000
if duration < a.minimum_seconds:
    faults.append("soak duration too short")
if any(x in text for x in ("Guru Meditation", "ESP-ROM:", "Audio init failed")):
    faults.append("reset/panic/init failure in capture")
delta_blocks = end["blocks"] - start["blocks"]
if abs(delta_blocks - duration * 375) > 3:
    faults.append("block progress does not match configured 375 blocks/s")
summary = {
    "software_soak_pass": not faults, "faults": faults,
    "duration_seconds": duration,
    "delta_blocks": delta_blocks,
    "delta_frames": delta_blocks * 128,
    "delta_dma": end["dma"] - start["dma"],
    "max_render_us": max(s["render_max_us"] for s in window),
    "fault_deltas": {k: end[k] - start[k] for k in fault_fields},
    "start": start, "end": end, "last_status": samples[-1],
}
print(json.dumps(summary, indent=2))
raise SystemExit(bool(faults))
