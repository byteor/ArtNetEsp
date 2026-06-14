"""
PlatformIO extra-script: minify the web assets before the LittleFS image is
built.

The source files under `data/` are kept human-readable (HTML/CSS/JS pretty
formatted). Flashing them verbatim wastes flash and, more importantly, RAM:
the captive portal reads `portal.html` whole into a String on memory-tight
ESP8266 boards. So when (and only when) the `buildfs`/`uploadfs` targets run,
this script mirrors `data/` into a throwaway `.fs_build/` directory, minifying
each asset on the way, and repoints PlatformIO's PROJECT_DATA_DIR at it. The
on-device image is minified; the repo sources are not.

`.fs_build/` is gitignored. Plain `pio run` (firmware only) skips all of this.

Minifiers (auto-installed into the PlatformIO penv on first use):
  - HTML -> minify-html   (JS minify left OFF: inline onclick= handlers
                           reference top-level functions that a JS mangler
                           would rename and break)
  - CSS  -> rcssmin
  - JS   -> rjsmin
  - JSON -> json.dumps(separators=(',', ':'))
If a minifier can't be imported or installed, that file is copied unchanged
(the build still succeeds, just larger).
"""

Import("env")  # noqa: F821  (injected by SCons)

import json
import os
import shutil
import subprocess


def _load(module_name, pip_name):
    """Import module_name, pip-installing pip_name into the penv if needed."""
    try:
        return __import__(module_name)
    except ImportError:
        pass
    try:
        subprocess.check_call(
            [env.subst("$PYTHONEXE"), "-m", "pip", "install", "-q", pip_name]
        )
        return __import__(module_name)
    except Exception as exc:  # noqa: BLE001
        print("minify_fs: could not load %s (%s) - copying as-is" % (pip_name, exc))
        return None


def _minify_html(text, mod):
    return mod.minify(
        text,
        minify_css=True,
        minify_js=False,  # inline onclick= handlers call top-level fns; a JS
        #                   mangler would rename and break them
        keep_closing_tags=True,
        preserve_brace_template_syntax=True,  # don't touch {{HOST}} placeholders
    )


def _build_minified(src_dir, out_dir):
    html_mod = _load("minify_html", "minify-html")
    css_mod = _load("rcssmin", "rcssmin")
    js_mod = _load("rjsmin", "rjsmin")

    if os.path.isdir(out_dir):
        shutil.rmtree(out_dir)

    total_src = total_out = 0
    for root, _dirs, files in os.walk(src_dir):
        rel = os.path.relpath(root, src_dir)
        dest_root = out_dir if rel == "." else os.path.join(out_dir, rel)
        os.makedirs(dest_root, exist_ok=True)

        for name in files:
            src = os.path.join(root, name)
            dest = os.path.join(dest_root, name)
            ext = os.path.splitext(name)[1].lower()
            raw = open(src, "rb").read()
            out = raw
            try:
                if ext in (".html", ".htm") and html_mod:
                    out = _minify_html(raw.decode("utf-8"), html_mod).encode("utf-8")
                elif ext == ".css" and css_mod:
                    out = css_mod.cssmin(raw.decode("utf-8")).encode("utf-8")
                elif ext == ".js" and js_mod:
                    out = js_mod.jsmin(raw.decode("utf-8")).encode("utf-8")
                elif ext == ".json":
                    out = json.dumps(
                        json.loads(raw.decode("utf-8")), separators=(",", ":")
                    ).encode("utf-8")
            except Exception as exc:  # noqa: BLE001
                print("minify_fs: %s not minified (%s) - copied raw" % (name, exc))
                out = raw

            open(dest, "wb").write(out)
            total_src += len(raw)
            total_out += len(out)
            if out is not raw and len(out) != len(raw):
                print(
                    "minify_fs: %s  %d -> %d bytes"
                    % (os.path.join(rel, name) if rel != "." else name, len(raw), len(out))
                )

    saved = total_src - total_out
    pct = (100.0 * saved / total_src) if total_src else 0.0
    print(
        "minify_fs: filesystem assets %d -> %d bytes (saved %d, %.0f%%)"
        % (total_src, total_out, saved, pct)
    )


# Only rebuild the minified data dir when an FS image is actually being built;
# leave plain `pio run` (firmware) untouched.
if set(COMMAND_LINE_TARGETS) & {"buildfs", "uploadfs"}:  # noqa: F821
    _src = env.subst("$PROJECT_DATA_DIR")
    _out = os.path.join(env.subst("$PROJECT_DIR"), ".fs_build")
    print("minify_fs: building minified FS image from %s -> %s" % (_src, _out))
    _build_minified(_src, _out)
    env.Replace(PROJECT_DATA_DIR=_out)
