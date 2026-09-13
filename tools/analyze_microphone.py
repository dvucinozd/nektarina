"""Decode MIC records, export mono PCM WAV and estimate dominant frequency.

Requires numpy. Results use the configured sample rate, not an independent clock.
"""
import argparse
import json
import re
import wave
from pathlib import Path
import numpy as np

p = argparse.ArgumentParser()
p.add_argument("capture")
p.add_argument("--output-prefix", required=True)
a = p.parse_args()
current = None
captures = []
for line in Path(a.capture).read_text(encoding="utf-8").splitlines():
    begin = re.fullmatch(r"MICBEGIN rate=(\d+) samples=(\d+)", line)
    if begin:
        if current is not None:
            raise ValueError("Unterminated capture")
        current = {"rate": int(begin[1]), "count": int(begin[2]), "samples": []}
    elif line.startswith("MIC "):
        if current is None:
            raise ValueError("Samples without MICBEGIN")
        _, offset, data = line.split()
        if int(offset) != len(current["samples"]) or len(data) % 4:
            raise ValueError("Missing/corrupt sample record")
        for i in range(0, len(data), 4):
            n = int(data[i:i+4], 16)
            current["samples"].append(n if n < 32768 else n - 65536)
    elif line == "MICEND":
        if current is None or len(current["samples"]) != current["count"]:
            raise ValueError("Incomplete sample count")
        captures.append(current)
        current = None
if current is not None or not captures:
    raise ValueError("No complete captures")
results = []
for idx, capture in enumerate(captures, 1):
    pcm = np.asarray(capture["samples"], dtype="<i2")
    filename = Path(f"{a.output_prefix}-{idx}.wav")
    filename.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(filename), "wb") as f:
        f.setparams((1, 2, capture["rate"], len(pcm), "NONE", "not compressed"))
        f.writeframes(pcm.tobytes())
    # Omit first 100ms to avoid ADC startup transients; DC removal before Hann FFT.
    x = pcm[int(capture["rate"] * 0.1):].astype(float)
    x -= x.mean()
    window = np.hanning(len(x))
    size = 1 << 20
    fft = np.abs(np.fft.rfft(x * window, n=size))
    freqs = np.fft.rfftfreq(size, 1 / capture["rate"])
    lo, hi = np.searchsorted(freqs, [100, 2000])
    peak = lo + int(np.argmax(fft[lo:hi]))
    y = np.log(np.maximum(fft[peak-1:peak+2], 1e-20))
    shift = 0.5 * (y[0] - y[2]) / (y[0] - 2*y[1] + y[2])
    estimate = (peak + shift) * capture["rate"] / size
    t = np.arange(len(x)) / capture["rate"]
    amplitude440 = abs(np.sum(x * window * np.exp(-2j*np.pi*440*t))) * 2 / window.sum()
    results.append({
        "capture": idx, "wav": str(filename), "samples": len(pcm),
        "sample_rate_assumed_hz": capture["rate"],
        "peak_hz_100_2000": float(estimate),
        "rms_dbfs": float(20*np.log10(max(np.sqrt(np.mean(x*x))/32768, 1e-20))),
        "amplitude_440_dbfs": float(20*np.log10(max(amplitude440/32768, 1e-20))),
        "clipped_samples": int(np.count_nonzero((pcm == 32767) | (pcm == -32768))),
    })
print(json.dumps({"clock_caveat": "ADC and DAC share a clock; not absolute calibration", "captures": results}, indent=2))
