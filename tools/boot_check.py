#!/usr/bin/env python3
"""Phase 0.5 HIL boot check for the ArtNetEsp bench device.

Resets the board over USB-CDC (DTR/RTS sequence) and watches serial output
for the `BOOT OK v<version>` sentinel, known crash signatures, and
boot-loop evidence. Writes the raw capture to .hil/last-boot.log.

Run with PlatformIO's bundled interpreter so pyserial is available:
    ~/.platformio/penv/bin/python tools/boot_check.py

Exit codes:
  0  ok      - BOOT OK sentinel seen, no crash signature, no boot-loop
  1  crash   - crash signature matched, or boot-loop detected
  2  timeout - watch window elapsed with no sentinel and no crash
  3  no port - device never enumerated (needs a physical hand, see below)
"""
import argparse
import glob
import os
import re
import sys
import time

import serial

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HIL_DIR = os.path.join(ROOT, ".hil")
HIL_ENV_FILE = os.path.join(ROOT, "tools", "hil.env")

CRASH_PATTERNS = [
    re.compile(p)
    for p in (
        r"Guru Meditation Error",
        r"abort\(\) was called",
        r"assert failed",
        r"Backtrace:",
        r"Task watchdog got triggered",
        r"Brownout detector was triggered",
        r"CORRUPT HEAP",
        r"Exception \(",
        r"wdt reset",
        r"Soft WDT reset",
        r"Stack smashing",
    )
]
BROWNOUT_RE = re.compile(r"Brownout detector was triggered")

BOOT_OK_RE = re.compile(r"BOOT OK v(\S+)")
STARTING_RE = re.compile(r"^Starting\.\.\.")
IP_RE = re.compile(r"^(\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3})$")


def load_env():
    env = {}
    if os.path.exists(HIL_ENV_FILE):
        for line in open(HIL_ENV_FILE):
            line = line.strip()
            if not line or line.startswith("#") or "=" not in line:
                continue
            k, v = line.split("=", 1)
            env[k.strip()] = v.strip()
    return env


def reset_board(ser):
    """USB-CDC DTR/RTS reset sequence (works for ESP32-S3 native USB)."""
    ser.dtr = False
    ser.rts = True
    time.sleep(0.1)
    ser.rts = False
    time.sleep(0.1)
    ser.dtr = True


def find_port(preferred, port_glob):
    if preferred and os.path.exists(preferred):
        return preferred
    candidates = sorted(glob.glob(port_glob))
    return candidates[0] if candidates else None


def run(port=None, port_glob="/dev/cu.usbmodem*", baud=115200, timeout=30.0, grace=4.0, no_reset=False):
    """Reset (unless no_reset) and watch the bench device's serial output.

    Returns a dict: result ('ok'|'crash'|'timeout'|'no_port'), port,
    reenum_count, starting_count, boot_ok_count, boot_ok_version, device_ip,
    log_path, crash_hits (list of (pattern, line)), log_lines.
    """
    os.makedirs(HIL_DIR, exist_ok=True)
    log_path = os.path.join(HIL_DIR, "last-boot.log")

    out = {
        "result": "no_port",
        "port": None,
        "reenum_count": 0,
        "starting_count": 0,
        "boot_ok_count": 0,
        "boot_ok_version": None,
        "device_ip": None,
        "log_path": log_path,
        "crash_hits": [],
        "log_lines": [],
    }

    port_wait_deadline = time.time() + 10.0
    found = find_port(port, port_glob)
    while found is None and time.time() < port_wait_deadline:
        time.sleep(0.5)
        found = find_port(port, port_glob)

    if found is None:
        open(log_path, "wb").close()
        return out

    try:
        ser = serial.Serial(found, baud, timeout=0.2)
    except serial.SerialException:
        open(log_path, "wb").close()
        return out

    if not no_reset:
        # Discard anything buffered from before we opened the port - e.g. the
        # tail of the auto-reboot triggered by `pio run -t upload`, which would
        # otherwise show up as a spurious extra "Starting..." and look like a
        # boot loop.
        ser.reset_input_buffer()
        reset_board(ser)

    log_lines = out["log_lines"]
    raw_chunks = []
    crash_hits = out["crash_hits"]
    last_was_connected = False

    def process_line(line):
        nonlocal last_was_connected
        log_lines.append(line)
        for pat in CRASH_PATTERNS:
            if pat.search(line):
                crash_hits.append((pat.pattern, line))
        m = BOOT_OK_RE.search(line)
        if m:
            out["boot_ok_count"] += 1
            out["boot_ok_version"] = m.group(1)
        if STARTING_RE.search(line):
            out["starting_count"] += 1

        if last_was_connected:
            ipm = IP_RE.match(line.strip())
            if ipm:
                out["device_ip"] = ipm.group(1)
            last_was_connected = False
        elif "Connected :)" in line:
            last_was_connected = True

    current_port = found
    start = time.time()
    deadline = start + timeout
    buf = ""
    grace_deadline = None

    while True:
        now = time.time()
        active_deadline = grace_deadline if grace_deadline is not None else deadline
        if now >= active_deadline:
            break

        try:
            chunk = ser.read(256) if ser is not None else b""
        except (serial.SerialException, OSError):
            chunk = b""
            try:
                ser.close()
            except Exception:
                pass
            ser = None

        if ser is None:
            out["reenum_count"] += 1
            new_port = None
            wait_until = min(now + 8.0, active_deadline)
            while new_port is None and time.time() < wait_until:
                time.sleep(0.2)
                new_port = find_port(port, port_glob)
            if new_port is None:
                break
            try:
                ser = serial.Serial(new_port, baud, timeout=0.2)
                current_port = new_port
            except serial.SerialException:
                ser = None
                break
            continue

        if chunk:
            raw_chunks.append(chunk)
            buf += chunk.decode(errors="replace")
            while "\n" in buf:
                line, buf = buf.split("\n", 1)
                process_line(line.rstrip("\r"))

            if out["boot_ok_count"] and grace_deadline is None and not crash_hits:
                grace_deadline = time.time() + grace

        if crash_hits:
            break

    if buf:
        process_line(buf)

    if ser is not None:
        try:
            ser.close()
        except Exception:
            pass

    with open(log_path, "wb") as f:
        for c in raw_chunks:
            f.write(c)

    out["port"] = current_port
    boot_loop = out["boot_ok_count"] > 1 or out["starting_count"] > 1

    if crash_hits:
        out["result"] = "crash"
    elif boot_loop:
        out["result"] = "crash"
    elif out["boot_ok_count"] >= 1:
        out["result"] = "ok"
    else:
        out["result"] = "timeout"

    return out


def main():
    env = load_env()
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument(
        "--port", default=env.get("HIL_PORT"), help="serial port (default: tools/hil.env HIL_PORT)"
    )
    parser.add_argument(
        "--port-glob",
        default=env.get("HIL_PORT_GLOB", "/dev/cu.usbmodem*"),
        help="glob used to find a re-enumerated port",
    )
    parser.add_argument("--baud", type=int, default=int(env.get("HIL_BAUD", "115200")))
    parser.add_argument("--timeout", type=float, default=30.0, help="total watch window in seconds")
    parser.add_argument(
        "--grace",
        type=float,
        default=4.0,
        help="extra seconds to watch for a boot-loop after BOOT OK is seen",
    )
    parser.add_argument(
        "--no-reset", action="store_true", help="don't issue a DTR/RTS reset; just watch"
    )
    args = parser.parse_args()

    out = run(
        port=args.port,
        port_glob=args.port_glob,
        baud=args.baud,
        timeout=args.timeout,
        grace=args.grace,
        no_reset=args.no_reset,
    )

    if out["result"] == "no_port":
        print("BOOT_RESULT=no_port")
        print(f"No serial port found (looked for {args.port!r} and {args.port_glob!r}).")
        print("Recovery needs a physical hand: hold BOOT, tap RESET to enter the ROM")
        print("bootloader, then reflash. An agent can't press the buttons.")
        return 3

    print(f"PORT={out['port']}")
    print(f"REENUM_COUNT={out['reenum_count']}")
    print(f"STARTING_COUNT={out['starting_count']}")
    print(f"BOOT_OK_COUNT={out['boot_ok_count']}")
    if out["boot_ok_version"]:
        print(f"BOOT_OK_VERSION={out['boot_ok_version']}")
    if out["device_ip"]:
        print(f"DEVICE_IP={out['device_ip']}")
    print(f"LOG={os.path.relpath(out['log_path'], ROOT)}")

    if out["result"] == "crash":
        for pattern, line in out["crash_hits"]:
            tag = (
                "BROWNOUT (likely USB power/cable, not firmware)"
                if BROWNOUT_RE.search(line)
                else "CRASH"
            )
            print(f"{tag}: matched /{pattern}/ in: {line}")
        if not out["crash_hits"]:
            print(
                f"BOOT_RESULT=crash  # boot-loop: "
                f"Starting...x{out['starting_count']} BOOT_OKx{out['boot_ok_count']}"
            )
            for line in out["log_lines"][-10:]:
                print(f"  | {line}")
            return 1
        for line in out["log_lines"][-10:]:
            print(f"  | {line}")
        print("BOOT_RESULT=crash")
        return 1

    if out["result"] == "ok":
        print("BOOT_RESULT=ok")
        return 0

    print("BOOT_RESULT=timeout")
    return 2


if __name__ == "__main__":
    sys.exit(main())
