#!/usr/bin/env python3
"""Symbolize ESP32/ESP8266 crash backtraces from a HIL boot log.

Finds `Backtrace: addr:sp addr:sp ...` (ESP32) and `epcN=0x.../excvaddr=0x...`
(ESP8266) addresses and runs the toolchain's addr2line against
.pio/build/<env>/firmware.elf, turning hex addresses into file:line + function.

Run with PlatformIO's bundled interpreter:
    ~/.platformio/penv/bin/python tools/decode_backtrace.py [logfile]
"""
import argparse
import glob
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HIL_ENV_FILE = os.path.join(ROOT, "tools", "hil.env")
PACKAGES_DIR = os.path.expanduser("~/.platformio/packages")

# env name -> (toolchain package glob, binutils prefix)
TOOLCHAIN_BY_ENV = {
    "d1_mini_oled": ("toolchain-xtensa*", "xtensa-lx106-elf"),
    "d1_mini": ("toolchain-xtensa*", "xtensa-lx106-elf"),
    "nodemcuv2": ("toolchain-xtensa*", "xtensa-lx106-elf"),
    "heltec_wifi_kit_8": ("toolchain-xtensa*", "xtensa-lx106-elf"),
    "sonoff_basic": ("toolchain-xtensa*", "xtensa-lx106-elf"),
    "sonoff_s31": ("toolchain-xtensa*", "xtensa-lx106-elf"),
    "lolin32": ("toolchain-xtensa-esp32@*", "xtensa-esp32-elf"),
    "esp32-devkitc-v4": ("toolchain-xtensa-esp32@*", "xtensa-esp32-elf"),
    "lolin_s2_mini": ("toolchain-xtensa-esp32s2*", "xtensa-esp32s2-elf"),
    "esp32-s3-devkitc-1": ("toolchain-xtensa-esp32s3*", "xtensa-esp32s3-elf"),
    "seeed_xiao_esp32s3": ("toolchain-xtensa-esp32s3*", "xtensa-esp32s3-elf"),
    "seeed_xiao_esp32s3_oled": ("toolchain-xtensa-esp32s3*", "xtensa-esp32s3-elf"),
}

BACKTRACE_RE = re.compile(r"Backtrace:\s*(.+)")
ADDR_PAIR_RE = re.compile(r"(0x[0-9a-fA-F]{8}):0x[0-9a-fA-F]{8}")
EPC_RE = re.compile(r"\b(?:epc1|epc2|epc3|excvaddr)=(0x[0-9a-fA-F]+)")


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


def find_addr2line(pkg_glob, prefix):
    candidates = sorted(glob.glob(os.path.join(PACKAGES_DIR, pkg_glob, "bin", f"{prefix}-addr2line")))
    for c in candidates:
        if "@" not in c:
            return c
    return candidates[0] if candidates else None


def extract_addresses(text):
    addrs = []
    for m in BACKTRACE_RE.finditer(text):
        addrs.extend(ADDR_PAIR_RE.findall(m.group(1)))
    for m in EPC_RE.finditer(text):
        addrs.append(m.group(1))
    return addrs


def main():
    hil_env = load_env()
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument(
        "logfile",
        nargs="?",
        default=os.path.join(ROOT, ".hil", "last-boot.log"),
        help="boot log to scan ('-' for stdin, default .hil/last-boot.log)",
    )
    parser.add_argument("--env", default=hil_env.get("HIL_ENV", "seeed_xiao_esp32s3"))
    parser.add_argument("--elf", help="override path to firmware.elf")
    args = parser.parse_args()

    text = sys.stdin.read() if args.logfile == "-" else open(args.logfile, errors="replace").read()

    addrs = extract_addresses(text)
    if not addrs:
        print("No Backtrace:/epcN=/excvaddr= addresses found in the log.")
        return 1

    elf = args.elf or os.path.join(ROOT, ".pio", "build", args.env, "firmware.elf")
    if not os.path.exists(elf):
        print(f"firmware.elf not found at {elf} - build the env first (pio run -e {args.env}).")
        return 1

    pkg_glob, prefix = TOOLCHAIN_BY_ENV.get(args.env, (None, None))
    addr2line = find_addr2line(pkg_glob, prefix) if pkg_glob else None
    if not addr2line:
        print(f"Could not find {prefix}-addr2line for env {args.env!r} under {PACKAGES_DIR}")
        return 1

    print(f"Using {addr2line}")
    print(f"Against {elf}")
    print(f"{len(addrs)} address(es): {' '.join(addrs)}")
    print()

    result = subprocess.run(
        [addr2line, "-pfiaC", "-e", elf] + addrs,
        capture_output=True,
        text=True,
    )
    sys.stdout.write(result.stdout)
    if result.stderr:
        sys.stderr.write(result.stderr)
    return 0 if result.returncode == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
