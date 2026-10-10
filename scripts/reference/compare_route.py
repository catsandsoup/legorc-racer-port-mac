#!/usr/bin/env python3
"""Drive the original Supersonic RC (under Wine) and the port with the same
short scripted route, from the same spawn point, and compare them.

For each route it produces:
- physics telemetry from both games (centre of mass, velocity, car axes),
  the original's read straight from its memory (orig_memory.py)
- the original run several times, to measure its own run-to-run spread;
  only differences beyond that spread count as port bugs
- a report (report.md) with key numbers and the first moment the port
  leaves the original's spread
- plots: top-down path, height, speed, roll, pitch (plot.png)
- side-by-side screenshots at chosen moments (sheet_*.png)

The original is the ground truth (docs/PRODUCT_RULES.md, "The baseline").
Output goes to local/reference/runs/ by default, which is git-ignored:
captures are game-derived and must never be committed.

Usage:
  scripts/reference/compare_route.py tests/routes/hard_left.json \\
      --orig-dir /dir/with/SupersonicRC.exe+game.dcr+GUILibrary.cct \\
      --godot /path/to/godot --data /path/to/converted [--only port|orig]

Needs (Linux): wine (wine64 + wine32), Xvfb, xdotool, ImageMagick `import`,
xvfb-run, Python 3 with numpy and Pillow. Reading the original's memory
needs permission to ptrace it (same user, or root).

A route is JSON:
  {"mode": 0,                          # 0 Free Driving ... 4 Supersonic Challenge
   "spawn": [x, y, z, heading_deg],    # Director units; z of the car model
                                       # (default: the original start)
   "settle": 1.0,                      # seconds at rest after spawning
   "input": "accelerate:0-4.5,steer_left:2-3",   # seconds after settling
   "shots": [1.5, 3, 4.5],             # screenshot times, seconds after settling
   "duration": 6,                      # telemetry length (default: last event + 0.5)
   "orig_runs": 3,                     # repetitions in the original
   "note": "what this route checks"}
"""
import argparse
import csv
import json
import math
import os
import shutil
import subprocess
import sys
import tempfile
import threading
import time
from pathlib import Path

import numpy as np
from PIL import Image, ImageChops, ImageDraw, ImageStat

sys.path.insert(0, str(Path(__file__).resolve().parent))
import orig_memory  # noqa: E402

REPO = Path(__file__).resolve().parents[2]
KEYS = {"accelerate": "Up", "brake": "Down", "steer_left": "Left", "steer_right": "Right", "handbrake": "space"}
MODE_Y = [239, 322, 406, 490, 573]  # level select buttons, screen y (docs/REFERENCE.md)
STAGE = (192, 144, 832, 624)        # the 640x480 stage on a 1024x768 display
DISPLAY = ":77"
START = [3790.17, -5696.19, 338.6598, 0.0]
COM_BELOW_MODEL = 8.5               # centre of mass below the car model origin (Director units)
COLS = ["t", "cx", "cy", "cz", "vx", "vy", "vz", "wx", "wy", "wz", "xx", "xy", "xz", "yx", "yy", "yz", "zx", "zy", "zz"]


def parse_input(spec, offset=0.0):
    events = []
    for w in filter(None, spec.split(",")):
        action, rng = w.split(":")
        t0, t1 = (float(x) for x in rng.split("-"))
        events.append((t0 + offset, "keydown", KEYS[action]))
        events.append((t1 + offset, "keyup", KEYS[action]))
    return sorted(events)


def route_duration(route):
    ends = [float(s) for s in route.get("shots", [])]
    for w in filter(None, route.get("input", "").split(",")):
        ends.append(float(w.split(":")[1].split("-")[1]))
    return float(route.get("duration", max(ends + [1.0]) + 0.5))


# ---------------------------------------------------------------- original

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
    return white > 25 and ImageStat.Stat(img.crop((0, 200, 640, 480))).mean[0] > 30


def xdo(env, *args):
    subprocess.run(["xdotool", *args], env=env, check=False)


def launch_original(orig_dir, mode):
    env = dict(os.environ, DISPLAY=DISPLAY, WINEDEBUG="-all",
               WINEPREFIX=os.environ.get("WINEPREFIX", str(Path(orig_dir).parent / "wineprefix")))
    xvfb = subprocess.Popen(["Xvfb", DISPLAY, "-screen", "0", "1024x768x24"],
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    time.sleep(2)
    if mode > 0:
        xdo(env, "keydown", "Control_L")  # unlock all challenges, as in the original
    subprocess.Popen(["wine", "SupersonicRC.exe"], cwd=orig_dir, env=env,
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
    time.sleep(1.0)  # let the car come to rest at the start before locating it
    return env, xvfb


def record(cs, t0, duration, rows, stop):
    while not stop.is_set():
        t = time.time() - t0
        if t > duration:
            break
        s = cs.read()
        rows.append([t, *s["com"], *s["vel"], *s["angvel"], *s["x_axis"], *s["y_axis"], *s["z_axis"]])
        time.sleep(1 / 120)


def run_original(route, orig_dir, out, runs):
    mode = int(route.get("mode", 0))
    spawn = route.get("spawn", START)
    settle = float(route.get("settle", 1.0))
    duration = route_duration(route)
    env, xvfb = launch_original(orig_dir, mode)
    try:
        cs = orig_memory.CarState(orig_memory.find_pid())
        for r in range(runs):
            for k in KEYS.values():
                xdo(env, "keyup", k)
            cs.teleport((spawn[0], spawn[1], spawn[2] - COM_BELOW_MODEL), spawn[3] if len(spawn) > 3 else 0.0)
            time.sleep(settle)
            t0 = time.time()
            rows, stop = [], threading.Event()
            rec = threading.Thread(target=record, args=(cs, t0, duration, rows, stop))
            rec.start()
            shots = [(float(t), "shot", None) for t in route.get("shots", [])] if r == 0 else []
            threads = []
            for t, kind, key in sorted(parse_input(route.get("input", "")) + shots, key=lambda e: e[0]):
                delay = t0 + t - time.time()
                if delay > 0:
                    time.sleep(delay)
                if kind == "shot":
                    th = threading.Thread(target=grab, args=(out / f"orig_{t:05.2f}.png", env))
                    th.start()
                    threads.append(th)
                else:
                    xdo(env, kind, key)
            rec.join()
            stop.set()
            for th in threads:
                th.join()
            write_csv(out / f"orig_run{r}.csv", rows)
            print(f"[orig] run {r + 1}/{runs}: {len(rows)} samples")
    finally:
        subprocess.run(["wineserver", "-k"], env=env, check=False)
        xvfb.terminate()


# ---------------------------------------------------------------- port

def run_port(route, godot, data, out, tuning=None):
    home = tempfile.mkdtemp()
    if tuning:  # a tuning.json mod for this run (Linux user folder)
        mod = Path(home) / ".local/share/Supersonic RC/mods/zz_compare"
        mod.mkdir(parents=True)
        shutil.copy(tuning, mod / "tuning.json")
    settle = float(route.get("settle", 1.0))
    spawn = route.get("spawn", START)
    shots = [float(s) + settle for s in route.get("shots", [])]
    inputs = []
    for w in filter(None, route.get("input", "").split(",")):
        action, rng = w.split(":")
        t0, t1 = (float(x) + settle for x in rng.split("-"))
        inputs.append(f"{action}:{t0}-{t1}")
    end = route_duration(route) + settle
    env = dict(os.environ, HOME=home, SSRC_DATA=str(data), SSRC_TEST="1",
               SSRC_TEST_FLOW=f"mode:{int(route.get('mode', 0))}",
               SSRC_TEST_SPAWN=",".join(str(v) for v in spawn),
               SSRC_TEST_INPUT=",".join(inputs),
               SSRC_TEST_SHOTS=",".join(str(s) for s in shots),
               SSRC_TEST_OUT=str(out / "port_"),
               SSRC_TEST_TELEMETRY=str(out / "port_raw.csv"),
               SSRC_TEST_QUIT=str(end + 0.3))
    if shots:
        cmd = ["xvfb-run", "-a", "-s", "-screen 0 1280x960x24", str(godot), "--path", str(REPO / "game"),
               "--rendering-driver", "opengl3", "--resolution", "960x720"]
    else:  # telemetry only: no rendering needed
        cmd = [str(godot), "--headless", "--path", str(REPO / "game")]
    with open(out / "port.log", "w") as log:
        subprocess.run(cmd, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=600, check=False)
    shutil.rmtree(home, ignore_errors=True)
    for s, s_route in zip(shots, route.get("shots", [])):
        src = out / f"port__drive_{s:05.2f}.png"
        if src.exists():
            Image.open(src).convert("RGB").resize((640, 480), Image.LANCZOS).save(out / f"port_{float(s_route):05.2f}.png")
            src.unlink()
    rows = []
    raw = out / "port_raw.csv"
    if raw.exists():
        for r in csv.DictReader(open(raw)):
            t = float(r["t"]) - settle
            if t < 0:
                continue
            rows.append([t] + [float(r[k]) for k in ("cx", "cy", "cz", "vx", "vy", "vz", "wx", "wy", "wz",
                                                      "xx", "xy", "xz", "yx", "yy", "yz", "zx", "zy", "zz")])
    write_csv(out / "port.csv", rows)
    print(f"[port] {len(rows)} samples")


# ---------------------------------------------------------------- compare

def write_csv(path, rows):
    with open(path, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(COLS)
        w.writerows(rows)


def load(path):
    a = np.loadtxt(path, delimiter=",", skiprows=1, ndmin=2)
    return a if a.size else None


def derived(a, ts):
    """Resample a telemetry array at times ts; return named series."""
    def col(name):
        return np.interp(ts, a[:, 0], a[:, COLS.index(name)])
    yx, yy, yz = col("yx"), col("yy"), col("yz")
    xz = col("xz")
    return {
        "x": col("cx"), "y": col("cy"), "z": col("cz"),
        "speed": np.hypot(col("vx"), col("vy")),
        "heading": np.degrees(np.unwrap(np.arctan2(-yx, yy))),
        "pitch": np.degrees(np.arcsin(np.clip(yz, -1, 1))),
        "roll": np.degrees(np.arcsin(np.clip(xz, -1, 1))),
    }


def roll_period(roll, dt=0.05):
    """Mean period of the roll wobble: zero crossings of roll minus its
    1 s moving average."""
    k = int(1.0 / dt)
    base = np.convolve(roll, np.ones(k) / k, mode="same")
    r = roll - base
    z = np.nonzero(np.diff(np.sign(r[k:-k])) != 0)[0]
    return float(2 * np.mean(np.diff(z)) * dt) if len(z) > 2 else float("nan")


def plot(out, ts, origs, port):
    W, H = 1200, 900
    img = Image.new("RGB", (W, H), (20, 20, 24))
    d = ImageDraw.Draw(img)
    panels = [("top-down path (x right, y up)", None, (0, 0, 600, 450)),
              ("height z (in)", "z", (600, 0, 1200, 300)),
              ("speed (in/s)", "speed", (600, 300, 1200, 600)),
              ("roll (deg)", "roll", (0, 450, 600, 675)),
              ("pitch (deg)", "pitch", (0, 675, 600, 900)),
              ("heading (deg)", "heading", (600, 600, 1200, 900))]
    series = [(o, (110, 150, 255)) for o in origs] + ([(port, (255, 150, 60))] if port else [])
    for title, key, (x0, y0, x1, y1) in panels:
        d.rectangle((x0 + 4, y0 + 4, x1 - 4, y1 - 4), outline=(70, 70, 80))
        d.text((x0 + 10, y0 + 8), title + "   blue = original runs, orange = port", fill=(220, 220, 220))
        if key is None:
            xs = np.concatenate([s["x"] for s, _ in series])
            ys = np.concatenate([s["y"] for s, _ in series])
            span = max(xs.max() - xs.min(), ys.max() - ys.min(), 50) * 1.1
            cx, cy = (xs.max() + xs.min()) / 2, (ys.max() + ys.min()) / 2
            def pt(x, y):
                return (x0 + 20 + (x - cx + span / 2) / span * (x1 - x0 - 40),
                        y1 - 20 - (y - cy + span / 2) / span * (y1 - y0 - 50))
            for s, c in series:
                d.line([pt(x, y) for x, y in zip(s["x"], s["y"])], fill=c, width=2)
                d.ellipse([*(v - 4 for v in pt(s["x"][0], s["y"][0])), *(v + 4 for v in pt(s["x"][0], s["y"][0]))], outline=(255, 255, 255))
            continue
        vals = np.concatenate([s[key] for s, _ in series])
        lo, hi = vals.min(), vals.max()
        if hi - lo < 1e-6:
            hi = lo + 1
        def pt(t, v):
            return (x0 + 20 + t / ts[-1] * (x1 - x0 - 40), y1 - 15 - (v - lo) / (hi - lo) * (y1 - y0 - 45))
        d.text((x0 + 10, y1 - 16), f"{lo:.0f}..{hi:.0f}", fill=(150, 150, 150))
        for s, c in series:
            d.line([pt(t, v) for t, v in zip(ts, s[key])], fill=c, width=2)
    img.save(out / "plot.png")


def compare(route, out):
    lines = [f"# {route.get('name', 'route')}", "", route.get("note", ""), "",
             f"Mode {route.get('mode', 0)}, spawn `{route.get('spawn', START)}`, input `{route.get('input', '')}`", ""]
    duration = route_duration(route)
    ts = np.arange(0, duration, 0.05)
    origs = [derived(a, ts) for a in (load(p) for p in sorted(out.glob("orig_run*.csv"))) if a is not None and len(a) > 2]
    pa = load(out / "port.csv") if (out / "port.csv").exists() else None
    port = derived(pa, ts) if pa is not None and len(pa) > 2 else None
    if origs:
        mean_xy = np.mean([[o["x"], o["y"], o["z"]] for o in origs], axis=0)
        spread = np.max([np.linalg.norm(np.array([o["x"], o["y"], o["z"]]) - mean_xy, axis=0) for o in origs], axis=0)
        lines += ["## Physics", "",
                  "| | original (mean of runs) | original spread | port |", "|---|---|---|---|"]
        def stat(name, f):
            ov = [f(o[name]) for o in origs]
            pv = f(port[name]) if port else float("nan")
            return f"| {name} | {np.mean(ov):.1f} | {np.min(ov):.1f} to {np.max(ov):.1f} | {pv:.1f} |"
        lines.append(stat("speed", np.max).replace("| speed", "| top speed (in/s)"))
        lines.append(stat("roll", lambda v: np.max(np.abs(v))).replace("| roll", "| max roll (deg)"))
        lines.append(stat("pitch", lambda v: np.max(np.abs(v))).replace("| pitch", "| max pitch (deg)"))
        lines.append(stat("z", np.max).replace("| z", "| max height (in)"))
        lines.append(stat("roll", roll_period).replace("| roll", "| roll wobble period (s)"))
        lines.append(stat("heading", lambda v: v[-1] - v[0]).replace("| heading", "| heading change (deg)"))
        if port:
            err = np.linalg.norm(np.array([port["x"], port["y"], port["z"]]) - mean_xy, axis=0)
            tol = np.maximum(3 * spread, 15.0)
            bad = np.nonzero(err > tol)[0]
            lines += ["", f"Final position error: {err[-1]:.0f} in (original spread {spread[-1]:.0f} in).",
                      f"**Port leaves the original's spread at t = {ts[bad[0]]:.2f} s.**" if len(bad) else
                      "**Port stays within the original's spread for the whole route.**", ""]
        plot(out, ts, origs, port)
        lines += ["![plot](plot.png)", ""]
    shots = []
    for s in route.get("shots", []):
        a, b = out / f"orig_{float(s):05.2f}.png", out / f"port_{float(s):05.2f}.png"
        if not (a.exists() and b.exists()):
            continue
        ia, ib = Image.open(a).convert("RGB"), Image.open(b).convert("RGB").resize((640, 480))
        diff = ImageChops.difference(ia, ib)
        score = sum(ImageStat.Stat(diff).mean) / 3
        sheet = Image.new("RGB", (1920, 500), (0, 0, 0))
        for i, (im, label) in enumerate(((ia, "original (Wine)"), (ib, "port"), (diff, f"difference {score:.1f}"))):
            sheet.paste(im, (i * 640, 20))
            ImageDraw.Draw(sheet).text((i * 640 + 8, 4), f"{label}  t={s}s", fill=(255, 255, 255))
        sheet.save(out / f"sheet_{float(s):05.2f}.png")
        shots.append(f"| {s} | {score:.1f} | ![](sheet_{float(s):05.2f}.png) |")
    if shots:
        lines += ["## Screenshots", "", "| t (s) | mean abs difference (0-255) | sheet |", "|---|---|---|"] + shots
    (out / "report.md").write_text("\n".join(lines) + "\n")
    print("\n".join(lines))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("route")
    ap.add_argument("--orig-dir")
    ap.add_argument("--godot")
    ap.add_argument("--data")
    ap.add_argument("--out")
    ap.add_argument("--only", choices=["port", "orig"])
    ap.add_argument("--orig-runs", type=int)
    ap.add_argument("--tuning", help="tuning.json to apply to the port for this run")
    a = ap.parse_args()
    route = json.loads(Path(a.route).read_text())
    route.setdefault("name", Path(a.route).stem)
    out = Path(a.out) if a.out else REPO / "local" / "reference" / "runs" / time.strftime("%Y%m%d-%H%M%S") / route["name"]
    out.mkdir(parents=True, exist_ok=True)
    if a.only != "port":
        if not a.orig_dir:
            sys.exit("--orig-dir is required to run the original")
        run_original(route, a.orig_dir, out, a.orig_runs or int(route.get("orig_runs", 3)))
    if a.only != "orig":
        if not (a.godot and a.data):
            sys.exit("--godot and --data are required to run the port")
        run_port(route, a.godot, a.data, out, a.tuning)
    compare(route, out)
    print(f"output: {out}")


if __name__ == "__main__":
    main()
