"""Rebuild the Preact web UI (web/) into data/www/ before a filesystem image
is built or uploaded.

Hooked as a `post:` extra_script (see [common] in platformio.ini) so the
`buildfs` / `uploadfs` platform targets already exist when we attach to them.
Runs `npm run build` in web/ ahead of `pio run -t buildfs` / `-t uploadfs`, so
the committed node build output in data/www/ is always fresh when flashed.

Behaviour:
  - web/ missing                -> nothing to do (silently skip)
  - Node.js + npm not on PATH   -> warn, flash the committed data/www/ as-is
  - node_modules/ missing       -> `npm install` first (warn + skip on failure)
  - build fails (toolchain ok)  -> abort the build (don't flash stale assets)

This only affects the FS image; firmware-only builds (`pio run`, `-t upload`)
never invoke buildfs and are untouched.
"""

Import("env")

import os
import shutil
import subprocess

PROJECT_DIR = env["PROJECT_DIR"]
WEB_DIR = os.path.join(PROJECT_DIR, "web")

# uploadfs depends on buildfs, so without a guard the hook would fire twice for
# a single `-t uploadfs`. Run the rebuild at most once per pio invocation.
_state = {"done": False}


def _banner(lines):
    width = max(len(line) for line in lines)
    bar = "+" + "-" * (width + 2) + "+"
    print(bar)
    for line in lines:
        print("| " + line.ljust(width) + " |")
    print(bar)


def rebuild_web_ui(source, target, env):
    if _state["done"]:
        return
    _state["done"] = True

    if not os.path.isdir(WEB_DIR):
        return  # No web/ sources in this checkout - nothing to rebuild.

    npm = shutil.which("npm")
    node = shutil.which("node")
    if not (npm and node):
        _banner([
            "Web UI toolchain (Node.js + npm) not found on PATH.",
            "Skipping the Preact rebuild - flashing the committed",
            "data/www/ build output as-is.",
            "",
            "To enable automatic rebuilds: install Node.js, then",
            "    cd web && npm install",
        ])
        return

    # First run after a fresh clone: pull deps so `npm run build` can succeed.
    if not os.path.isdir(os.path.join(WEB_DIR, "node_modules")):
        print("[web] node_modules/ missing - running `npm install`...")
        if subprocess.run([npm, "install"], cwd=WEB_DIR).returncode != 0:
            _banner([
                "`npm install` failed - flashing the committed",
                "data/www/ build output as-is.",
            ])
            return

    print("[web] rebuilding Preact UI -> data/www/ ...")
    if subprocess.run([npm, "run", "build"], cwd=WEB_DIR).returncode != 0:
        # Toolchain is present but the build broke - that's a real error;
        # don't silently package stale/partial assets into the FS image.
        print("[web] ERROR: `npm run build` failed - aborting filesystem image.")
        env.Exit(1)
    print("[web] data/www/ is up to date.")


env.AddPreAction("buildfs", rebuild_web_ui)
env.AddPreAction("uploadfs", rebuild_web_ui)
