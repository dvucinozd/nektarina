"""Capture M1 UART evidence. Requires pyserial (available in the IDF venv).

Commands: --command 5:? --command 10:x (seconds from port open).
No flash operations. --reset deliberately resets the connected device.
"""
import argparse
import time
from pathlib import Path
import serial

p = argparse.ArgumentParser()
p.add_argument("--port", default="COM6")
p.add_argument("--seconds", type=float, default=30)
p.add_argument("--output", required=True)
p.add_argument("--reset", action="store_true")
p.add_argument("--command", action="append", default=[])
a = p.parse_args()
commands = sorted((float(x.split(":", 1)[0]), x.split(":", 1)[1]) for x in a.command)
out = Path(a.output)
out.parent.mkdir(parents=True, exist_ok=True)
s = serial.Serial()
s.port, s.baudrate, s.timeout = a.port, 115200, 0.2
s.dtr = s.rts = False
with s, out.open("x", encoding="utf-8", newline="\n") as log:
    # Context manager opens the configured port.
    if a.reset:
        s.rts = True
        time.sleep(0.1)
        s.rts = False
    start = time.monotonic()
    pending = ""
    while time.monotonic() - start < a.seconds:
        while commands and commands[0][0] <= time.monotonic() - start:
            _, c = commands.pop(0)
            s.write(c.encode("ascii"))
            log.write(f"HOST command={c} elapsed={time.monotonic()-start:.3f}s\n")
        data = s.read(4096).decode("utf-8", errors="replace").replace("\r", "")
        log.write(data)
        log.flush()
        pending += data
        while "\n" in pending:
            line, pending = pending.split("\n", 1)
            if line and not line.startswith("MIC "):
                print(line, flush=True)
            if "failed=1" in line or "Audio init failed" in line or "Guru Meditation" in line:
                s.write(b"x")
                raise SystemExit("Hardware test failed; stop requested, see capture")
