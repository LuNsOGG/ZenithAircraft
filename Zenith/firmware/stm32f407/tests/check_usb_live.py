"""Read-only live CDC check, with optional explicitly requested relative zero.

This script does not flash/reset the MCU. It opens only Zenith's VID/PID,
records raw lines under build/checks and checks real firmware output.
"""
import argparse
from dataclasses import asdict
from datetime import datetime, timezone
import json
from pathlib import Path
import sys
import time

import serial
from serial.tools import list_ports

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT.parents[1] / "tools" / "altitude_monitor"))
from telemetry import Decoder

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--duration", type=float, default=20.0)
parser.add_argument("--zero", action="store_true", help="Request one new relative zero after 3 s")
args = parser.parse_args()
ports = [p for p in list_ports.comports() if (p.vid, p.pid) == (0xCAFE, 0x4001)]
if len(ports) != 1:
    raise SystemExit(f"Expected one Zenith CDC port, found {len(ports)}")
out = ROOT / "build" / "checks" / ("usb_live_" + datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ") + ".jsonl")
out.parent.mkdir(parents=True, exist_ok=True)
decoder = Decoder()
samples = []
zero_sent = False
start = time.monotonic()
with serial.Serial(ports[0].device, 115200, timeout=0.1, write_timeout=0.5) as connection, out.open("xb") as log:
    connection.dtr = True
    while time.monotonic() - start < args.duration:
        chunk = connection.read(min(max(connection.in_waiting, 1), 8192))
        if chunk:
            log.write(chunk)
            samples.extend(decoder.feed(chunk))
        if args.zero and not zero_sent and samples and time.monotonic() - start >= 3:
            connection.write(b"ZERO\n")
            zero_sent = True
if not samples:
    raise SystemExit("No valid USB telemetry received")
result = {
    "port": ports[0].device, "serial": ports[0].serial_number,
    "frames": len(samples), "decode_errors": decoder.errors,
    "first": asdict(samples[0]), "last": asdict(samples[-1]),
    "observed_relative_valid": any(s.rel_ok for s in samples),
    "observed_absolute_valid": any(s.asl_ok for s in samples),
    "zero_sent": zero_sent, "zero_acknowledged": samples[-1].zero > samples[0].zero,
    "usb_sequence_gaps": sum(max(0, ((b.seq - a.seq) & 0xFFFFFFFF) - 1) for a, b in zip(samples, samples[1:])),
    "log": str(out),
}
out.with_suffix(".summary.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
print(json.dumps(result, indent=2))
if decoder.errors or not result["observed_relative_valid"] or not result["observed_absolute_valid"]:
    raise SystemExit(1)
if args.zero and (not result["zero_acknowledged"] or not samples[-1].rel_ok):
    raise SystemExit(1)
