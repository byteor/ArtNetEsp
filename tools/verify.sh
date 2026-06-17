#!/usr/bin/env bash
# Phase 0.5 HIL verification orchestrator.
#
# Usage: tools/verify.sh [env] [--quick|--full]
#   env      build environment (default: HIL_ENV from tools/hil.env)
#   --quick  build -> flash -> boot_check          (~90s,  T1)
#   --full   --quick, then hil_smoke + a soak       (~min,  T2)
#
# On a crash (boot_check exit 1, or a hil_smoke failure), auto-decodes the
# backtrace and tails .hil/last-boot.log.
#
# Exit codes mirror boot_check.py: 0 ok, 1 crash/fail, 2 timeout, 3 no-port.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

PIO="$HOME/.platformio/penv/bin/pio"
PY="$HOME/.platformio/penv/bin/python"
[ -x "$PIO" ] || PIO="pio"
[ -x "$PY" ] || PY="python3"

# shellcheck disable=SC1091
source tools/hil.env

ENV="$HIL_ENV"
MODE="--quick"
SOAK_MINUTES="${VERIFY_SOAK_MINUTES:-4}"

for arg in "$@"; do
  case "$arg" in
    --quick|--full) MODE="$arg" ;;
    -*) echo "verify.sh: unknown flag '$arg'" >&2; exit 64 ;;
    *) ENV="$arg" ;;
  esac
done

on_crash() {
  echo "==> crash detected - decoding backtrace"
  "$PY" tools/decode_backtrace.py --env "$ENV" || true
  echo "---- tail .hil/last-boot.log ----"
  tail -n 30 .hil/last-boot.log 2>/dev/null || true
}

echo "==> [build] pio run -e $ENV"
"$PIO" run -e "$ENV"

echo "==> [flash] pio run -e $ENV -t upload"
"$PIO" run -e "$ENV" -t upload

echo "==> [boot_check]"
set +e
"$PY" tools/boot_check.py
rc=$?
set -e
case $rc in
  0) ;;
  1) on_crash; echo "VERIFY_RESULT=fail  # boot_check crash"; exit 1 ;;
  2) echo "VERIFY_RESULT=fail  # boot_check timeout (no BOOT OK)"; exit 2 ;;
  3) echo "VERIFY_RESULT=fail  # no serial port found"; exit 3 ;;
  *) echo "VERIFY_RESULT=fail  # boot_check exited $rc"; exit "$rc" ;;
esac

if [ "$MODE" = "--quick" ]; then
  echo "VERIFY_RESULT=pass (--quick)"
  exit 0
fi

echo "==> [hil_smoke --soak $SOAK_MINUTES]"
set +e
"$PY" tools/hil_smoke.py --soak "$SOAK_MINUTES"
rc=$?
set -e
if [ $rc -ne 0 ]; then
  on_crash
  echo "VERIFY_RESULT=fail  # hil_smoke"
  exit 1
fi

echo "VERIFY_RESULT=pass (--full)"
