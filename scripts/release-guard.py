#!/usr/bin/env python3
"""Release guard: fails if game-derived bytes would reach the repo or a download.

Nothing from LEGO Supersonic RC may be committed or shipped: not the
original files (game.dcr, GUILibrary.cct), not the converted data that
ssrc-import writes (glTF, PNG, MP3, JSON), not screenshots of the game.

Usage:
  release-guard.py --tree                 every file tracked at HEAD
  release-guard.py --staged               files staged for the next commit
  release-guard.py --commits A..B         every blob added or changed in A..B
  release-guard.py --app <dir|zip>        a built .app, its zip, or any folder
  release-guard.py --print-hash <file>    print a line for release-guard.sha256

Checks:
  - file types: Director/Shockwave movies and casts, W3D, Havok, glTF and
    audio, and any image except the allowlisted ones below
  - content: Director headers (RIFX/XFIR + MV93/MC95/FGDM/FGDC, as read by
    third_party/dirplayer-rs/vm-rust/src/director/file.rs), binary glTF
    headers and ID3 tags, anywhere inside packs and archives
  - the converter's own output files (manifest.json that names the game)
  - SHA-256 of known originals, listed in scripts/release-guard.sha256
  - local absolute paths (/Users/<name>/, /home/<name>/) in text files

Only the Python standard library is used, so it runs on macOS and Linux CI.
"""

import argparse
import hashlib
import io
import os
import re
import subprocess
import sys
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HASH_FILE = os.path.join(ROOT, "scripts", "release-guard.sha256")

# Extensions that are always game data (original or converted).
FORBIDDEN_EXT = {
    ".dcr", ".dir", ".dxr", ".cct", ".cst", ".cxt",  # Director / Shockwave
    ".w3d", ".hke",  # Shockwave 3D scene, Havok physics
    ".glb", ".gltf",  # converted scenes
    ".mp3", ".swa", ".wav", ".aif", ".aiff", ".ogg",  # audio
}
# Images are allowed only where we made them ourselves.
IMAGE_EXT = {".png", ".jpg", ".jpeg", ".bmp", ".gif", ".tga", ".webp", ".ctex", ".tif", ".tiff"}
ALLOWED_IMAGES = {
    "game/icon.png",  # the app icon, drawn for this port
}
# Converter output names. A file with one of these names that also mentions
# the game is converted data (manifest.json has "game": "LEGO Supersonic RC").
CONVERTER_JSON = {"manifest.json", "images.json", "sounds.json", "text.json", "collision.json"}

# Director movie/cast header: big-endian RIFX or little-endian XFIR, then a
# length, then the codec fourcc (byte-reversed in XFIR files).
DIRECTOR_RE = re.compile(rb"(?:RIFX[\s\S]{4}(?:MV93|MC95|FGDM|FGDC)|XFIR[\s\S]{4}(?:39VM|59CM|MDGF|CDGF))")
GLB_RE = re.compile(rb"glTF\x02\x00\x00\x00")
ID3_RE = re.compile(rb"ID3[\x02-\x04]\x00")
ABS_PATH_RE = re.compile(rb"/(?:Users|home)/(?!user/|runner/|you/|name/|<)[A-Za-z0-9_.-]+/")
# Executables and libraries are only checked at offset 0: engine code can
# hold these byte patterns. Everything else (packs, archives, data) is
# scanned end to end for embedded files.
EXECUTABLE_MAGIC = (b"\xcf\xfa\xed\xfe", b"\xce\xfa\xed\xfe", b"\xca\xfe\xba\xbe", b"\x7fELF", b"MZ")


def load_hashes():
    hashes = {}
    if os.path.exists(HASH_FILE):
        with open(HASH_FILE) as f:
            for line in f:
                line = line.split("#", 1)[0].strip()
                if line:
                    h, _, name = line.partition(" ")
                    hashes[h.lower()] = name.strip() or "known original"
    return hashes


def is_text(data):
    return b"\x00" not in data[:8192]


class Guard:
    def __init__(self):
        self.hashes = load_hashes()
        self.problems = []
        self.checked = 0

    def fail(self, where, why):
        self.problems.append(f"{where}: {why}")

    def check(self, path, data, where=None):
        """path: repo-relative or archive-relative path; data: file bytes."""
        where = where or path
        self.checked += 1
        norm = path.replace("\\", "/").lstrip("./")
        name = os.path.basename(norm).lower()
        ext = os.path.splitext(name)[1]
        if ext in FORBIDDEN_EXT:
            self.fail(where, f"game data file type ({ext})")
        if ext in IMAGE_EXT and not any(norm.endswith(a) for a in ALLOWED_IMAGES):
            if not (ext == ".ctex" and "/icon.png-" in norm):
                self.fail(where, "image not on the allowlist (converted art or a screenshot?)")
        digest = hashlib.sha256(data).hexdigest()
        if digest in self.hashes:
            self.fail(where, f"matches a known original ({self.hashes[digest]})")
        head = data[:16]
        if DIRECTOR_RE.match(head):
            self.fail(where, "Director/Shockwave file header")
        elif GLB_RE.match(head):
            self.fail(where, "binary glTF header")
        elif ID3_RE.match(head):
            self.fail(where, "MP3 (ID3) header")
        is_zip = data[:4] == b"PK\x03\x04"
        if not is_zip and not data.startswith(EXECUTABLE_MAGIC):
            for rx, what in ((DIRECTOR_RE, "Director/Shockwave data"), (GLB_RE, "binary glTF"), (ID3_RE, "MP3 audio")):
                m = rx.search(data, 1)
                if m:
                    self.fail(where, f"embedded {what} at byte {m.start()}")
        if name in CONVERTER_JSON and b"Supersonic" in data:
            self.fail(where, "converted game data (ssrc-import output)")
        if is_text(data):
            m = ABS_PATH_RE.search(data)
            if m:
                self.fail(where, f"local absolute path {m.group().decode(errors='replace')}")
        if is_zip:
            self.check_zip(data, where)  # members, not the compressed bytes

    def check_zip(self, data, where):
        try:
            z = zipfile.ZipFile(io.BytesIO(data))
        except zipfile.BadZipFile:
            return
        for info in z.infolist():
            if info.is_dir():
                continue
            self.check(info.filename, z.read(info), f"{where}!/{info.filename}")

    def report(self, what):
        if self.problems:
            print(f"release guard: FAILED ({what}, {self.checked} files checked)")
            for p in self.problems:
                print("  " + p)
            print("Nothing game-derived may be committed or shipped. See docs/PRODUCT_RULES.md.")
            return 1
        print(f"release guard: ok ({what}, {self.checked} files checked)")
        return 0


def git(*args, binary=False):
    out = subprocess.run(["git", *args], cwd=ROOT, check=True, capture_output=True).stdout
    return out if binary else out.decode()


def blob(sha):
    return git("cat-file", "blob", sha, binary=True)


def scan_tree(g):
    for line in git("ls-tree", "-r", "-z", "HEAD").split("\0"):
        if not line:
            continue
        meta, path = line.split("\t", 1)
        mode, kind, sha = meta.split()
        if kind == "blob":
            g.check(path, blob(sha))


def scan_staged(g):
    for line in git("diff", "--cached", "--name-only", "--diff-filter=ACMR", "-z").split("\0"):
        if not line:
            continue
        entry = git("ls-files", "-s", "-z", "--", line).split("\0")[0]
        if not entry:
            continue
        meta, path = entry.split("\t", 1)
        mode, sha, _stage = meta.split()
        if mode != "160000":
            g.check(path, blob(sha), f"(staged) {path}")


def scan_commits(g, rng):
    if re.fullmatch(r"0+\.\..*", rng):  # new branch push: base is all zeros
        rng = rng.split("..", 1)[1]
    try:
        commits = git("rev-list", "--reverse", rng).split()
    except subprocess.CalledProcessError:
        # The base is gone (force push) or unknown: check all history of the tip.
        rng = rng.split("..", 1)[-1]
        commits = git("rev-list", "--reverse", rng).split()
    seen = set()
    for c in commits:
        out = git("diff-tree", "-r", "--root", "--no-commit-id", "-z", "--diff-filter=ACMR", c)
        fields = out.split("\0")
        i = 0
        while i < len(fields) - 1:
            meta = fields[i]
            if not meta.startswith(":"):
                i += 1
                continue
            parts = meta[1:].split()
            new_mode, new_sha, status = parts[1], parts[3], parts[4]
            path = fields[i + 1]
            i += 3 if status[0] in "RC" else 2
            if status[0] in "RC":
                path = fields[i - 1]
            if new_mode == "160000" or new_sha in seen:
                continue
            seen.add(new_sha)
            g.check(path, blob(new_sha), f"{c[:9]} {path}")
    return len(commits)


def scan_path(g, target):
    if os.path.isfile(target):
        with open(target, "rb") as f:
            g.check(os.path.basename(target), f.read(), target)
        return
    for dirpath, dirnames, filenames in os.walk(target):
        for fn in filenames:
            p = os.path.join(dirpath, fn)
            if os.path.islink(p):
                continue
            rel = os.path.relpath(p, target)
            with open(p, "rb") as f:
                g.check(rel, f.read(), rel)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    mode = ap.add_mutually_exclusive_group(required=True)
    mode.add_argument("--tree", action="store_true")
    mode.add_argument("--staged", action="store_true")
    mode.add_argument("--commits", metavar="A..B")
    mode.add_argument("--app", metavar="PATH")
    mode.add_argument("--print-hash", metavar="FILE")
    a = ap.parse_args()

    if a.print_hash:
        with open(a.print_hash, "rb") as f:
            print(hashlib.sha256(f.read()).hexdigest(), os.path.basename(a.print_hash))
        return 0
    g = Guard()
    if a.tree:
        scan_tree(g)
        return g.report("tracked files at HEAD")
    if a.staged:
        scan_staged(g)
        return g.report("staged files")
    if a.commits:
        n = scan_commits(g, a.commits)
        return g.report(f"{n} commits in {a.commits}")
    scan_path(g, a.app)
    return g.report(a.app)


if __name__ == "__main__":
    sys.exit(main())
