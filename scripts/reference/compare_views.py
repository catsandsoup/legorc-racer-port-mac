#!/usr/bin/env python3
"""Put the original's car (under Wine) and the port's car at the same spots
and headings, let the chase camera settle, and screenshot both side by side.
For comparing models, textures and lighting (docs/PRODUCT_RULES.md, "The
baseline"). Output goes to local/reference/views/ (git-ignored).

Usage:
  WINEPREFIX=... scripts/reference/compare_views.py --orig-dir DIR --godot GODOT --data DATA [--only port|orig]
"""
import argparse
import os
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageStat

sys.path.insert(0, str(Path(__file__).resolve().parent))
import compare_route as cr  # noqa: E402
import orig_memory  # noqa: E402

REPO = Path(__file__).resolve().parents[2]
# name: (x, y, z of the car model, heading) in Director units; heading 0 faces +Y.
VIEWS = {
    "start_fwd": (3790.17, -5696.19, 338.66, 0),
    "start_back": (3790.17, -5696.19, 338.66, 180),
    "start_left": (3790.17, -5696.19, 338.66, 90),
    "start_right": (3790.17, -5696.19, 338.66, 270),
    "upper_mid_w": (3543, -4724, 338.66, 90),
    "upper_mid_e": (1000, -4724, 338.66, 270),
    "dino": (-1496, -6300, 345, 0),
    "dino_side": (-300, -4800, 338.66, 120),
    "lower_n": (0, 3937, 9, 0),
    "lower_s": (0, 3937, 9, 180),
    "lower_e": (0, 3937, 9, 270),
    "lower_w": (0, 3937, 9, 90),
}
SETTLE = 2.5


def run_original(orig_dir, out):
    env, xvfb = cr.launch_original(orig_dir, 0)
    try:
        cs = orig_memory.CarState(orig_memory.find_pid())
        for name, (x, y, z, h) in VIEWS.items():
            cs.teleport((x, y, z - cr.COM_BELOW_MODEL), h)
            time.sleep(SETTLE)
            cr.grab(out / f"orig_{name}.png", env)
            print("[orig]", name)
    finally:
        subprocess.run(["wineserver", "-k"], env=env, check=False)
        xvfb.terminate()


def run_port(godot, data, out):
    for name, v in VIEWS.items():
        home = tempfile.mkdtemp()
        env = dict(os.environ, HOME=home, SSRC_DATA=str(data), SSRC_TEST="1", SSRC_TEST_FLOW="mode:0",
                   SSRC_TEST_SPAWN=",".join(str(c) for c in v), SSRC_TEST_SHOTS=str(SETTLE),
                   SSRC_TEST_OUT=str(out / f"port_{name}"), SSRC_TEST_QUIT=str(SETTLE + 0.3))
        cmd = ["xvfb-run", "-a", "-s", "-screen 0 1280x960x24", str(godot), "--path", str(REPO / "game"),
               "--rendering-driver", "opengl3", "--resolution", "960x720"]
        subprocess.run(cmd, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=300, check=False)
        shutil.rmtree(home, ignore_errors=True)
        src = out / f"port_{name}_drive_{SETTLE:05.2f}.png"
        if src.exists():
            Image.open(src).convert("RGB").resize((640, 480), Image.LANCZOS).save(out / f"port_{name}.png")
            src.unlink()
        print("[port]", name)


def sheets(out):
    rows = []
    for name in VIEWS:
        a, b = out / f"orig_{name}.png", out / f"port_{name}.png"
        if not (a.exists() and b.exists()):
            continue
        ia, ib = Image.open(a).convert("RGB"), Image.open(b).convert("RGB")
        score = sum(ImageStat.Stat(ImageChops.difference(ia, ib)).mean) / 3
        sheet = Image.new("RGB", (1280, 500))
        sheet.paste(ia, (0, 20))
        sheet.paste(ib, (640, 20))
        d = ImageDraw.Draw(sheet)
        d.text((8, 4), f"original (Wine)  {name}", fill=(255, 255, 255))
        d.text((648, 4), f"port  {name}   difference {score:.1f}", fill=(255, 255, 255))
        sheet.save(out / f"view_{name}.png")
        rows.append(f"| {name} | {score:.1f} | ![](view_{name}.png) |")
    (out / "report.md").write_text("# Matched views\n\n| view | difference | sheet |\n|---|---|---|\n" + "\n".join(rows) + "\n")
    print("\n".join(rows))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--orig-dir")
    ap.add_argument("--godot")
    ap.add_argument("--data")
    ap.add_argument("--out")
    ap.add_argument("--only", choices=["port", "orig"])
    a = ap.parse_args()
    out = Path(a.out) if a.out else REPO / "local" / "reference" / "views" / time.strftime("%Y%m%d-%H%M%S")
    out.mkdir(parents=True, exist_ok=True)
    if a.only != "port":
        run_original(a.orig_dir, out)
    if a.only != "orig":
        run_port(a.godot, a.data, out)
    sheets(out)
    print("output:", out)


if __name__ == "__main__":
    main()
