#!/usr/bin/env python3
"""Drive the original Supersonic RC (under Wine) and the port with the same
scripted route, capture both at the same moments, and write side-by-side
sheets plus a difference score for each moment.

The original is the ground truth (docs/PRODUCT_RULES.md, "The baseline").
Output goes to local/reference/runs/ by default, which is git-ignored:
captures are game-derived and must never be committed.

Usage:
  scripts/reference/compare_route.py tests/routes/straight.json \
      --orig-dir /path/with/SupersonicRC.exe+game.dcr+GUILibrary.cct \
      --godot /path/to/godot --data /path/to/converted [--only port|orig]

Needs (Linux): wine (wine64 + wine32), Xvfb, xdotool, ImageMagick `import`,
xvfb-run, Python 3 with Pillow.

A route is JSON:
  {"mode": 0,                       # 0 Free Driving ... 4 Supersonic Challenge
   "input": "accelerate:0-4.5,steer_left:2-3",   # seconds after driving starts
   "shots": [1.5, 3, 4.5],          # capture times, seconds after driving starts
   "note": "what this route checks"}
"""
import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile
import threading
import time
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageStat

REPO = Path(__file__).resolve().parents[2]
KEYS = {"accelerate": "Up", "brake": "Down", "steer_left": "Left", "steer_right": "Right", "handbrake": "space"}
# Level select button centres in screen coordinates (docs/REFERENCE.md).
MODE_Y = [239, 322, 406, 490, 573]
STAGE = (192, 144, 832, 624)  # the 640x480 stage on a 1024x768 display
DISPLAY = ":77"


def parse_input(spec):
    events = []
    for w in filter(None, spec.split(",")):
        action, rng = w.split(":")
        t0, t1 = (float(x) for x in rng.split("-"))
        events.append((t0, "keydown", KEYS[action]))
        events.append((t1, "keyup", KEYS[action]))
    return sorted(events)


def grab(path, env):
    x0, y0, x1, y1 = STAGE
    subprocess.run(["import", "-window", "root", "-crop", f"{x1 - x0}x{y1 - y0}+{x0}+{y0}", "+repage", str(path)],
                   env=env, check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def stage_image(env):
    with tempfile.NamedTemporaryFile(suffix=".png") as f:
        grab(f.name, env)
        try:
            return Image.open(f.name).convert("RGB")
        except Exception:
            return None


def hud_visible(img):
    """True once the driving HUD ('Time remaining' / 'Points') is drawn."""
    if img is None:
        return False
    region = img.crop((20, 4, 130, 22))
    white = sum(1 for p in region.getdata() if min(p) > 220)
    lower = img.crop((0, 200, 640, 480))
    return white > 25 and ImageStat.Stat(lower).mean[0] > 30


def xdo(env, *args):
    subprocess.run(["xdotool", *args], env=env, check=False)


def run_original(route, orig_dir, out):
    env = dict(os.environ, DISPLAY=DISPLAY, WINEDEBUG="-all",
               WINEPREFIX=os.environ.get("WINEPREFIX", str(Path(orig_dir).parent / "wineprefix")))
    xvfb = subprocess.Popen(["Xvfb", DISPLAY, "-screen", "0", "1024x768x24"],
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    time.sleep(2)
    try:
        mode = int(route.get("mode", 0))
        if mode > 0:
            xdo(env, "keydown", "Control_L")  # unlock all challenges, as in the original
        wine = subprocess.Popen(["wine", "SupersonicRC.exe"], cwd=orig_dir, env=env,
                                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, start_new_session=True)
        deadline = time.time() + 90
        while time.time() < deadline:
            img = stage_image(env)
            if img is not None and ImageStat.Stat(img).mean[0] > 40:
                break
            time.sleep(1)
        else:
            raise RuntimeError("original never showed its title screen")
        time.sleep(3)
        if mode > 0:
            xdo(env, "keyup", "Control_L")
            time.sleep(0.5)
        xdo(env, "mousemove", "512", "400", "click", "1")       # title
        time.sleep(3)
        xdo(env, "mousemove", "300", str(MODE_Y[mode]))          # rollover first
        time.sleep(0.8)
        xdo(env, "click", "1")
        time.sleep(3)
        xdo(env, "mousemove", "512", "400", "click", "1")       # help
        deadline = time.time() + 60
        while time.time() < deadline and not hud_visible(stage_image(env)):
            time.sleep(0.2)
        t0 = time.time()
        print(f"[orig] driving started")
        shots = [(float(t), "shot", None) for t in route.get("shots", [])]
        timeline = sorted(parse_input(route.get("input", "")) + shots, key=lambda e: e[0])
        threads = []
        for t, kind, key in timeline:
            delay = t0 + t - time.time()
            if delay > 0:
                time.sleep(delay)
            if kind == "shot":
                p = out / f"orig_{t:05.2f}.png"
                th = threading.Thread(target=grab, args=(p, env))
                th.start()
                threads.append(th)
            else:
                xdo(env, kind, key)
        for th in threads:
            th.join()
        for k in KEYS.values():
            xdo(env, "keyup", k)
    finally:
        subprocess.run(["wineserver", "-k"], env=env, check=False)
        xvfb.terminate()


def run_port(route, godot, data, out):
    home = tempfile.mkdtemp()
    shots = route.get("shots", [])
    env = dict(os.environ, HOME=home, SSRC_DATA=str(data), SSRC_TEST="1",
               SSRC_TEST_FLOW=f"mode:{int(route.get('mode', 0))}",
               SSRC_TEST_INPUT=route.get("input", ""),
               SSRC_TEST_SHOTS=",".join(str(s) for s in shots),
               SSRC_TEST_OUT=str(out / "port_"),
               SSRC_TEST_QUIT=str(max(shots + [1]) + 0.5),
               SSRC_DEBUG_CAR=str(int((max(shots + [1]) + 1) * 60)))
    cmd = ["xvfb-run", "-a", "-s", "-screen 0 1280x960x24", str(godot), "--path", str(REPO / "game"),
           "--rendering-driver", "opengl3", "--resolution", "1280x960"]
    with open(out / "port.log", "w") as log:
        subprocess.run(cmd, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=300, check=False)
    shutil.rmtree(home, ignore_errors=True)
    # test_driver names shots port__drive_SS.SS.png; normalise to port_SS.SS.png at 640x480.
    for s in shots:
        src = out / f"port__drive_{float(s):05.2f}.png"
        if src.exists():
            Image.open(src).convert("RGB").resize((640, 480), Image.LANCZOS).save(out / f"port_{float(s):05.2f}.png")
            src.unlink()


def compare(route, out):
    rows = []
    for s in route.get("shots", []):
        a, b = out / f"orig_{float(s):05.2f}.png", out / f"port_{float(s):05.2f}.png"
        if not (a.exists() and b.exists()):
            rows.append((s, None, a.exists(), b.exists()))
            continue
        ia, ib = Image.open(a).convert("RGB"), Image.open(b).convert("RGB").resize((640, 480))
        diff = ImageChops.difference(ia, ib)
        score = sum(ImageStat.Stat(diff).mean) / 3
        sheet = Image.new("RGB", (1920, 500), (0, 0, 0))
        for i, (im, label) in enumerate(((ia, "original (Wine)"), (ib, "port"), (diff, f"difference {score:.1f}"))):
            sheet.paste(im, (i * 640, 20))
            ImageDraw.Draw(sheet).text((i * 640 + 8, 4), f"{label}  t={s}s", fill=(255, 255, 255))
        sheet.save(out / f"sheet_{float(s):05.2f}.png")
        rows.append((s, score, True, True))
    with open(out / "report.md", "w") as f:
        f.write(f"# {route.get('name', 'route')}\n\n{route.get('note', '')}\n\n")
        f.write(f"Input: `{route.get('input', '')}` (mode {route.get('mode', 0)})\n\n")
        f.write("| t (s) | mean abs difference (0-255) | sheet |\n|---|---|---|\n")
        for s, score, ha, hb in rows:
            if score is None:
                f.write(f"| {s} | missing ({'orig ok' if ha else 'no orig'}, {'port ok' if hb else 'no port'}) | |\n")
            else:
                f.write(f"| {s} | {score:.1f} | sheet_{float(s):05.2f}.png |\n")
    print((out / "report.md").read_text())


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("route")
    ap.add_argument("--orig-dir")
    ap.add_argument("--godot")
    ap.add_argument("--data")
    ap.add_argument("--out")
    ap.add_argument("--only", choices=["port", "orig"])
    a = ap.parse_args()
    route = json.loads(Path(a.route).read_text())
    route.setdefault("name", Path(a.route).stem)
    out = Path(a.out) if a.out else REPO / "local" / "reference" / "runs" / time.strftime("%Y%m%d-%H%M%S") / route["name"]
    out.mkdir(parents=True, exist_ok=True)
    if a.only != "port":
        if not a.orig_dir:
            sys.exit("--orig-dir is required to run the original")
        run_original(route, a.orig_dir, out)
    if a.only != "orig":
        if not (a.godot and a.data):
            sys.exit("--godot and --data are required to run the port")
        run_port(route, a.godot, a.data, out)
    compare(route, out)
    print(f"output: {out}")


if __name__ == "__main__":
    main()
