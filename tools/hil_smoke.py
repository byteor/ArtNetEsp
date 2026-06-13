#!/usr/bin/env python3
"""Phase 0.5 HIL smoke test for the ArtNetEsp bench device.

Sequence:
  1. boot_check (confirms a clean boot, gives us DEVICE_IP + the live port)
  2. REST contract: GET /heap, GET /config schema, POST /config round-trip
     on the `universe` scratch field
  3. POST /reboot -> boot_check again (confirms reboot path + refreshes IP/port)
  4. Art-Net E2E: send one ArtDmx frame for the configured device, assert the
     device's serial echo of set() within ~1.5s (device-type aware)
  5. Optional --soak N: flood Art-Net at --fps for N minutes, assert the
     windowed-minimum /heap is non-decreasing (no leak trend)

Run with PlatformIO's bundled interpreter:
    ~/.platformio/penv/bin/python tools/hil_smoke.py [--soak MINUTES]
"""
import argparse
import json
import os
import re
import socket
import sys
import time
import urllib.error
import urllib.request

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import artnet_send  # noqa: E402
import boot_check  # noqa: E402

HTTP_TIMEOUT = 5.0

REQUIRED_TOP = ["_needReboot", "hw", "info", "host", "universe", "wifi", "dmx"]
REQUIRED_HW = ["freq", "ledPin", "buttonPin", "longPressDelay"]
REQUIRED_INFO = ["id", "chip", "version", "built", "max_dmx_devices", "ssid", "rssi"]
REQUIRED_STATUS = REQUIRED_INFO + ["uptime", "free_heap"]
REQUIRED_DMX_FIELDS = ["channel", "type", "pin", "level", "multiplier", "pulse", "threshold"]

ECHO_PATTERNS = {
    "BINARY": re.compile(r"^Relay: \d"),
    "DIMMER": re.compile(r"^Dimmer: \d+ = \d+"),
    "SERVO": re.compile(r"^Servo: \d+"),
}


class Check:
    def __init__(self):
        self.failures = []

    def ok(self, label, detail=""):
        print(f"PASS {label}" + (f" - {detail}" if detail else ""))

    def fail(self, label, detail=""):
        self.failures.append(label)
        print(f"FAIL {label}" + (f" - {detail}" if detail else ""))

    def skip(self, label, detail=""):
        print(f"SKIP {label}" + (f" - {detail}" if detail else ""))


def http_get(host, path):
    req = urllib.request.Request(f"http://{host}{path}", method="GET")
    with urllib.request.urlopen(req, timeout=HTTP_TIMEOUT) as resp:
        return resp.read().decode()


def http_post_json(host, path, obj):
    body = json.dumps(obj).encode()
    req = urllib.request.Request(
        f"http://{host}{path}",
        data=body,
        method="POST",
        headers={"Content-Type": "application/json"},
    )
    with urllib.request.urlopen(req, timeout=HTTP_TIMEOUT) as resp:
        return resp.read().decode()


def run_boot_check(check, label, timeout=30.0):
    out = boot_check.run(
        port=None,
        port_glob=boot_check.load_env().get("HIL_PORT_GLOB", "/dev/cu.usbmodem*"),
        timeout=timeout,
    )
    if out["result"] == "ok":
        check.ok(label, f"v{out['boot_ok_version']} ip={out['device_ip']} port={out['port']}")
    else:
        detail = f"result={out['result']}"
        if out["crash_hits"]:
            detail += f" crash={out['crash_hits'][0][1]!r}"
        check.fail(label, detail)
    return out


def check_heap(check, host):
    try:
        body = http_get(host, "/heap")
        heap = int(body.strip())
    except Exception as e:
        check.fail("GET /heap", str(e))
        return None
    if heap <= 0:
        check.fail("GET /heap", f"non-positive heap: {heap}")
        return None
    check.ok("GET /heap", f"{heap} bytes free")
    return heap


def check_config_schema(check, host):
    try:
        body = http_get(host, "/config")
        cfg = json.loads(body)
    except Exception as e:
        check.fail("GET /config", str(e))
        return None

    missing = [k for k in REQUIRED_TOP if k not in cfg]
    missing += [f"hw.{k}" for k in REQUIRED_HW if k not in cfg.get("hw", {})]
    missing += [f"info.{k}" for k in REQUIRED_INFO if k not in cfg.get("info", {})]
    if not isinstance(cfg.get("dmx"), list):
        missing.append("dmx (not a list)")
    if not isinstance(cfg.get("wifi"), list):
        missing.append("wifi (not a list)")
    for i, dev in enumerate(cfg.get("dmx", [])):
        for k in REQUIRED_DMX_FIELDS:
            if k not in dev:
                missing.append(f"dmx[{i}].{k}")

    if missing:
        check.fail("GET /config schema", f"missing: {', '.join(missing)}")
        return None

    check.ok(
        "GET /config schema",
        f"v{cfg['info']['version']} host={cfg['host']} universe={cfg['universe']} "
        f"dmx={[d['type'] for d in cfg['dmx']]}",
    )
    return cfg


def check_status(check, host):
    try:
        body = http_get(host, "/status")
        status = json.loads(body)
    except Exception as e:
        check.fail("GET /status", str(e))
        return None

    missing = [k for k in REQUIRED_STATUS if k not in status]
    if missing:
        check.fail("GET /status schema", f"missing: {', '.join(missing)}")
        return None

    check.ok(
        "GET /status schema",
        f"v{status['version']} uptime={status['uptime']} heap={status['free_heap']}",
    )
    return status


def check_config_roundtrip(check, host):
    try:
        before = json.loads(http_get(host, "/config"))
        original = before["universe"]
        scratch = (original + 1) % 64
        if scratch == original:
            scratch = (original + 2) % 64

        resp1 = json.loads(http_post_json(host, "/config", {"universe": scratch}))
        if resp1.get("status") != "pending":
            check.fail("POST /config round-trip", f"POST response={resp1!r}, expected status=pending")
            return
        time.sleep(0.2)  # queued update is applied on the next loop() iteration

        after_set = json.loads(http_get(host, "/config"))
        if after_set.get("universe") != scratch:
            check.fail("POST /config round-trip", f"GET after POST universe={after_set.get('universe')!r}, expected {scratch}")
            return

        resp2 = json.loads(http_post_json(host, "/config", {"universe": original}))
        if resp2.get("status") != "pending":
            check.fail("POST /config round-trip", f"revert POST response={resp2!r}, expected status=pending")
            return
        time.sleep(0.2)  # queued update is applied on the next loop() iteration

        after_revert = json.loads(http_get(host, "/config"))
        if after_revert.get("universe") != original:
            check.fail("POST /config round-trip", f"GET after revert universe={after_revert.get('universe')!r}, expected {original}")
            return

        check.ok("POST /config round-trip", f"universe {original} -> {scratch} -> {original}")
    except Exception as e:
        check.fail("POST /config round-trip", str(e))


def watch_serial(ser, patterns, duration):
    deadline = time.time() + duration
    buf = ""
    lines = []
    crash_hits = []
    matched = None
    while time.time() < deadline:
        chunk = ser.read(256)
        if not chunk:
            continue
        buf += chunk.decode(errors="replace")
        while "\n" in buf:
            line, buf = buf.split("\n", 1)
            line = line.rstrip("\r")
            lines.append(line)
            for pat in boot_check.CRASH_PATTERNS:
                if pat.search(line):
                    crash_hits.append((pat.pattern, line))
            if matched is None:
                for name, pat in patterns.items():
                    if pat.search(line):
                        matched = name
        if matched is not None and crash_hits:
            break
    return matched, lines, crash_hits


def check_artnet_echo(check, host, port, baud, cfg):
    import serial as pyserial

    devices = [d for d in cfg.get("dmx", []) if d.get("type") not in (None, "DISABLED")]
    if not devices:
        check.skip("Art-Net E2E", "no enabled DMX devices configured")
        return
    dev = devices[0]
    dtype = dev["type"]
    universe = cfg["universe"]

    try:
        ser = pyserial.Serial(port, baud, timeout=0.2)
    except Exception as e:
        check.fail("Art-Net E2E", f"can't open {port}: {e}")
        return

    time.sleep(0.2)
    ser.reset_input_buffer()

    if dtype == "REPEATER":
        frame = bytearray(512)
        for i in range(512):
            frame[i] = (i * 37) % 256
        pkt = artnet_send.build_packet(universe, frame, 1)
    else:
        ch = dev["channel"]
        frame = artnet_send.parse_frame_arg(f"{ch}=200")
        pkt = artnet_send.build_packet(universe, frame, 1)

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.sendto(pkt, (host, artnet_send.ARTNET_PORT))
    sock.close()

    matched, lines, crash_hits = watch_serial(ser, ECHO_PATTERNS, duration=1.5)
    ser.close()

    if crash_hits:
        check.fail("Art-Net E2E", f"crash after frame: {crash_hits[0][1]}")
        return

    if dtype == "REPEATER":
        check.ok(
            "Art-Net E2E",
            "sent 512ch frame, core-1 task alive (no crash); "
            "per-channel echo n/a for REPEATER, use RS485 fixture",
        )
        return

    if matched == dtype:
        echo_line = next(l for l in lines if ECHO_PATTERNS[dtype].search(l))
        check.ok("Art-Net E2E", f"{dtype} ch={dev['channel']} echoed: {echo_line!r}")
    else:
        check.fail("Art-Net E2E", f"no {dtype!r} echo within 1.5s (got {len(lines)} lines)")


def check_soak(check, host, port, baud, cfg, minutes, fps):
    universe = cfg["universe"]
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    frame = bytearray(512)

    samples = []
    end = time.time() + minutes * 60
    seq = 1
    next_sample = time.time()
    while time.time() < end:
        sock.sendto(artnet_send.build_packet(universe, frame, seq), (host, artnet_send.ARTNET_PORT))
        seq = (seq % 255) + 1
        if time.time() >= next_sample:
            try:
                heap = int(http_get(host, "/heap").strip())
                samples.append((time.time(), heap))
            except Exception:
                pass
            next_sample = time.time() + 1.0
        time.sleep(1.0 / fps)
    sock.close()

    if len(samples) < 4:
        check.fail("Soak heap trend", f"only {len(samples)} /heap samples collected")
        return

    n_windows = max(2, min(6, len(samples) // 5))
    window_size = len(samples) // n_windows
    window_mins = []
    for i in range(n_windows):
        chunk = samples[i * window_size : (i + 1) * window_size] if i < n_windows - 1 else samples[i * window_size :]
        window_mins.append(min(h for _, h in chunk))

    tolerance = 512  # bytes of fragmentation jitter we tolerate
    leak_at = None
    for i in range(1, len(window_mins)):
        if window_mins[i] < window_mins[i - 1] - tolerance:
            leak_at = i
            break

    detail = f"{len(samples)} samples over {minutes}min, window mins: {window_mins}"
    if leak_at is not None:
        check.fail("Soak heap trend", detail)
    else:
        check.ok("Soak heap trend", detail)


def main():
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("--soak", type=float, default=0.0, help="soak duration in minutes (0 = skip)")
    parser.add_argument("--fps", type=float, default=44.0, help="Art-Net flood rate for --soak")
    args = parser.parse_args()

    check = Check()

    boot1 = run_boot_check(check, "boot_check (initial)")
    if boot1["result"] != "ok" or not boot1["device_ip"]:
        print("SMOKE_RESULT=fail")
        return 1
    host = boot1["device_ip"]

    check_heap(check, host)
    check_config_schema(check, host)
    check_status(check, host)
    check_config_roundtrip(check, host)

    try:
        http_post_json(host, "/reboot", {})
    except Exception as e:
        check.fail("POST /reboot", str(e))

    boot2 = run_boot_check(check, "boot_check (post-reboot)")
    if boot2["result"] != "ok" or not boot2["device_ip"]:
        print("SMOKE_RESULT=fail")
        return 1
    host = boot2["device_ip"]
    port = boot2["port"]

    cfg = check_config_schema(check, host)
    if cfg:
        check_artnet_echo(check, host, port, boot_check.load_env().get("HIL_BAUD", 115200), cfg)

        if args.soak > 0:
            check_soak(check, host, port, boot_check.load_env().get("HIL_BAUD", 115200), cfg, args.soak, args.fps)

    if check.failures:
        print(f"SMOKE_RESULT=fail  # {len(check.failures)} failure(s): {', '.join(check.failures)}")
        return 1

    print("SMOKE_RESULT=pass")
    return 0


if __name__ == "__main__":
    sys.exit(main())
