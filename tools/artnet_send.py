#!/usr/bin/env python3
"""Minimal Art-Net (ArtDmx) sender for HIL testing.

Sends ArtDmx packets (UDP 6454, OpDmx, ProtVer 14) to the bench device.

Modes (pick one, default is a single all-zero/--frame packet):
  --frame CH=VAL[,CH=VAL,...]   send a single frame (unset channels = 0)
  --ramp CH                     ramp 1-based channel CH 0->255->0 over --duration
  --flood                       sustained flood at --fps for --duration,
                                 using --frame as the static payload (if given)

Run with PlatformIO's bundled interpreter:
    ~/.platformio/penv/bin/python tools/artnet_send.py --help
"""
import argparse
import socket
import struct
import sys
import time

ARTNET_PORT = 6454
DMX_CHANNELS = 512


def build_packet(universe, data, sequence=0):
    payload = bytearray(data)
    if len(payload) < 2:
        payload.extend(b"\x00" * (2 - len(payload)))
    if len(payload) % 2:
        payload.append(0)
    pkt = bytearray()
    pkt += b"Art-Net\x00"
    pkt += struct.pack("<H", 0x5000)  # OpCode = OpDmx, low byte first
    pkt += struct.pack(">H", 14)  # ProtVer 14, high byte first
    pkt.append(sequence & 0xFF)
    pkt.append(0)  # Physical
    pkt.append(universe & 0xFF)  # SubUni
    pkt.append((universe >> 8) & 0x7F)  # Net
    pkt += struct.pack(">H", len(payload))  # Length, high byte first
    pkt += payload
    return bytes(pkt)


def parse_frame_arg(spec):
    """'1=255,5=128' -> 512-byte bytearray with those 1-based channels set."""
    buf = bytearray(DMX_CHANNELS)
    if not spec:
        return buf
    for pair in spec.split(","):
        pair = pair.strip()
        if not pair:
            continue
        ch_s, val_s = pair.split("=")
        ch = int(ch_s)
        val = int(val_s)
        if not (1 <= ch <= DMX_CHANNELS):
            raise ValueError(f"channel {ch} out of range 1-{DMX_CHANNELS}")
        if not (0 <= val <= 255):
            raise ValueError(f"value {val} out of range 0-255")
        buf[ch - 1] = val
    return buf


def main():
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("--host", default="255.255.255.255", help="destination (default: broadcast)")
    parser.add_argument("--port", type=int, default=ARTNET_PORT)
    parser.add_argument("--universe", type=int, default=0)
    parser.add_argument("--frame", default="", help="CH=VAL,CH=VAL,... (1-based DMX channels)")
    parser.add_argument("--ramp", type=int, metavar="CH", help="ramp this 1-based channel 0->255->0")
    parser.add_argument("--flood", action="store_true", help="sustained flood at --fps")
    parser.add_argument("--fps", type=float, default=44.0)
    parser.add_argument("--duration", type=float, default=2.0, help="seconds (--ramp/--flood)")
    args = parser.parse_args()

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    dest = (args.host, args.port)

    try:
        base = parse_frame_arg(args.frame)
    except ValueError as e:
        print(f"--frame: {e}")
        return 1

    if args.ramp is not None:
        if not (1 <= args.ramp <= DMX_CHANNELS):
            print(f"--ramp channel must be 1-{DMX_CHANNELS}")
            return 1
        period = max(args.duration, 0.001)
        seq = 1
        start = time.time()
        while True:
            t = time.time() - start
            if t >= period:
                break
            phase = t / period
            level = int(255 * (1 - abs(2 * phase - 1)))  # triangle wave 0->255->0
            buf = bytearray(base)
            buf[args.ramp - 1] = level
            sock.sendto(build_packet(args.universe, buf, seq), dest)
            seq = (seq % 255) + 1
            time.sleep(1.0 / args.fps)
        buf = bytearray(base)
        buf[args.ramp - 1] = 0
        sock.sendto(build_packet(args.universe, buf, seq), dest)
        print(
            f"Sent ramp on channel {args.ramp} for {period:.1f}s @ {args.fps} fps "
            f"to {dest[0]}:{dest[1]} universe {args.universe}"
        )
        return 0

    if args.flood:
        seq = 1
        start = time.time()
        n = 0
        while time.time() - start < args.duration:
            sock.sendto(build_packet(args.universe, base, seq), dest)
            seq = (seq % 255) + 1
            n += 1
            time.sleep(1.0 / args.fps)
        print(
            f"Sent {n} frames @ {args.fps} fps for {args.duration:.1f}s "
            f"to {dest[0]}:{dest[1]} universe {args.universe}"
        )
        return 0

    sock.sendto(build_packet(args.universe, base, 0), dest)
    print(f"Sent 1 frame to {dest[0]}:{dest[1]} universe {args.universe}: {args.frame or '(all zero)'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
