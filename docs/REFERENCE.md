# Running the original game as the reference

The trusted reference is the original Windows projector, `SupersonicRC.exe`
(Shockwave 10), running `game.dcr` under Wine. It renders the real Director
8.5 Shockwave 3D and Havok output, which no reimplementation can claim.

## Setup (Ubuntu 24.04 container)

```sh
sudo dpkg --add-architecture i386 && sudo apt-get update
sudo apt-get install -y wine64 wine32:i386 xdotool imagemagick xvfb
```

Put `SupersonicRC.exe`, `game.dcr` and `GUILibrary.cct` in one folder
(never in the repo).

```sh
export WINEPREFIX=$PWD/wineprefix WINEDEBUG=-all DISPLAY=:77
Xvfb :77 -screen 0 1024x768x24 &
(cd orig && setsid wine SupersonicRC.exe &)
sleep 18
import -window root shot.png                                       # full screen
convert shot.png -crop 640x480+192+144 +repage stage.png           # the 640x480 stage
```

The projector runs full screen with the stage centred at (192,144) on a
1024×768 display.

## Driving it

- Title: click anywhere (`xdotool mousemove 512 400 click 1`).
- Level select: buttons are at stage x 30–321. Their centres in screen
  coordinates:

  | Button | Screen y |
  |---|---|
  | Free Driving | 239 |
  | Checkpoint Chase | 322 |
  | Stunt Rally | 406 |
  | Scavenger Hunt | 490 |
  | Supersonic Challenge | 573 |

  Move the mouse onto a button and pause before clicking; the GUI library
  needs the rollover first.
- Help screen: click anywhere. Gameplay starts about 10–12 s later.
- Drive: `xdotool keydown Up` / `keyup Up`, and likewise Left, Right, Down
  and space.
- Holding Ctrl while the projector starts unlocks all modes. Release it
  with `xdotool keyup Control_L` before clicking, or clicks won't register.
- **Never send Escape** to clear modifiers: it ends the projector.
- Entering Scavenger Hunt after a Ctrl launch closed the projector under
  Wine once. It was not reproduced; retry before trusting it.

## Captures already taken (session 1)

These were sampled from the real build at 640×480 against the port's
Original lighting (Free Driving, car at rest):

| Region | Original | Port |
|---|---|---|
| Floor | 118,119,156 | 118,120,154 |
| Wall | 205,205,198 | 199,198,196 |
| Ceiling | 70,71,91 | 70,71,92 |

A full-throttle run from the start (Up held 4.5 s) passes the first
checkpoint at about 3 s in both.

Screenshots are not committed (they are game-derived). Re-capture them.
