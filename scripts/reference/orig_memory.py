#!/usr/bin/env python3
"""Read and write the original Supersonic RC's car physics state in the
Wine process's memory (Linux /proc/<pid>/mem). Used by compare_route.py
for telemetry and for spawning the original's car anywhere.

What was found (session 1, by searching for the known start position and
watching what changed while driving and steering):

The Havok Xtra keeps the car's rigid-body state as float32 in Havok units
(Director inches x 0.0254, Z up, same axes as Director). From the first
position copy ("base"):

  +0x00  position x, y, z (the centre of mass), w
  +0x10  rotation quaternion, stored w, x, y, z
  +0x24  linear velocity x, y, z
  +0x34  angular velocity x, y, z
  +0x44  a second copy of the same layout (position, quaternion, velocities)
  +0x130 the rotation as a 3x3 matrix (columns = car axes)
  +0x154 position again

Writing the position into every copy (and zeroing the velocities) teleports
the car, and the original's own physics carries on from there.
"""
import math
import struct
import numpy as np

SCALE = 0.0254          # Havok metres per Director inch
START = (3790.17, -5696.19)  # the original's start, Director units


def _regions(pid):
    for line in open(f"/proc/{pid}/maps"):
        parts = line.split()
        a, b = (int(x, 16) for x in parts[0].split("-"))
        if "rw" in parts[1] and b - a <= 512 * 1024 * 1024:
            yield a, b


def read(pid, addr, n):
    with open(f"/proc/{pid}/mem", "rb") as f:
        f.seek(addr)
        try:
            return f.read(n)
        except OSError:
            return None


def write(pid, addr, data):
    with open(f"/proc/{pid}/mem", "r+b") as f:
        f.seek(addr)
        f.write(data)


def find_pid():
    import subprocess
    out = subprocess.run(["pgrep", "-f", "SupersonicRC.exe"], capture_output=True, text=True).stdout.split()
    pids = [int(p) for p in out]
    # the Windows process itself is the one named exactly SupersonicRC.exe
    for p in pids:
        try:
            if open(f"/proc/{p}/comm").read().strip().lower().startswith("supersonicrc"):
                return p
        except OSError:
            pass
    return pids[0] if pids else None


def find_position_copies(pid, x, y, tol=0.05):
    """All float32 (x, y) pairs in Havok units near the given point."""
    hits = []
    for a, b in _regions(pid):
        buf = read(pid, a, b - a)
        if not buf:
            continue
        arr = np.frombuffer(buf, dtype="<f4", count=len(buf) // 4)
        with np.errstate(invalid="ignore"):
            m = (np.abs(arr[:-1] - x) < tol) & (np.abs(arr[1:] - y) < tol)
        hits.extend(a + 4 * int(i) for i in np.nonzero(m)[0])
    return hits


class CarState:
    """Locate once (car at rest at the original start), then read/teleport."""

    def __init__(self, pid):
        self.pid = pid
        x, y = START[0] * SCALE, START[1] * SCALE
        hits = find_position_copies(pid, x, y)
        self.base = None
        for h in hits:
            q = struct.unpack("<4f", read(pid, h + 0x10, 16))
            p2 = struct.unpack("<2f", read(pid, h + 0x44, 8))
            if abs(sum(v * v for v in q) - 1) < 1e-3 and abs(p2[0] - x) < 0.05 and abs(p2[1] - y) < 0.05:
                self.base = h
                break
        if self.base is None:
            raise RuntimeError(f"car state not found ({len(hits)} position matches)")
        # Every other live copy of the position (other Havok/Lingo structures).
        self.extra = [h for h in hits if h not in (self.base, self.base + 0x44, self.base + 0x154)]

    def read(self):
        """Director units: position (centre of mass), velocities, car axes."""
        d = read(self.pid, self.base, 0x44)
        px, py, pz, _, qw, qx, qy, qz = struct.unpack("<8f", d[0:0x20])
        vx, vy, vz = struct.unpack("<3f", d[0x24:0x30])
        wx, wy, wz = struct.unpack("<3f", d[0x34:0x40])
        R = quat_to_matrix(qw, qx, qy, qz)
        return {
            "com": (px / SCALE, py / SCALE, pz / SCALE),
            "vel": (vx / SCALE, vy / SCALE, vz / SCALE),
            "angvel": (wx, wy, wz),
            "x_axis": tuple(R[:, 0]), "y_axis": tuple(R[:, 1]), "z_axis": tuple(R[:, 2]),
        }

    def teleport(self, com_dir, heading_deg=0.0):
        """Put the car's centre of mass at com_dir (Director units), level,
        facing heading_deg about +Z (0 faces +Y, like the start), at rest."""
        p = struct.pack("<3f", *(c * SCALE for c in com_dir))
        h = math.radians(heading_deg)
        q = struct.pack("<4f", math.cos(h / 2), 0.0, 0.0, math.sin(h / 2))
        zero = struct.pack("<3f", 0.0, 0.0, 0.0)
        R = quat_to_matrix(math.cos(h / 2), 0.0, 0.0, math.sin(h / 2))
        for off in (0x00, 0x44):
            write(self.pid, self.base + off, p)
            write(self.pid, self.base + off + 0x10, q)
            write(self.pid, self.base + off + 0x24, zero)
            write(self.pid, self.base + off + 0x34, zero)
        write(self.pid, self.base + 0x130, struct.pack("<9f", *R.T.flatten()))
        write(self.pid, self.base + 0x154, p)
        for a in self.extra:
            write(self.pid, a, p)


def quat_to_matrix(w, x, y, z):
    return np.array([
        [1 - 2 * (y * y + z * z), 2 * (x * y - w * z), 2 * (x * z + w * y)],
        [2 * (x * y + w * z), 1 - 2 * (x * x + z * z), 2 * (y * z - w * x)],
        [2 * (x * z - w * y), 2 * (y * z + w * x), 1 - 2 * (x * x + y * y)],
    ])
