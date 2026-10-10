# The player's journey

Written entirely from the player's side. Every screen, hook and setting must
earn its place on this map. Read with [DESIGN_PRINCIPLES.md](DESIGN_PRINCIPLES.md)
and [PRODUCT_RULES.md](PRODUCT_RULES.md).

## Who the player is

Someone who drove the Supersonic RC car around the toy shop as a
kid in 2003. Today the only way they can play it is in a tiny browser or
emulator window that doesn't run it properly. They're now an adult on an
Apple silicon Mac, with a copy of the game (`Supersonic_RC.zip`, holding
`SupersonicRC.exe`, `game.dcr` and `GUILibrary.cct`). They may own an Xbox
or PlayStation controller and would rather use it than the keyboard.

They don't know or care what Shockwave, Director, Lingo, Havok or GDExtension
mean. They want **that** game, the way they remember it, full screen and
smooth, without a weekend of setup.

What they're secretly afraid of:

- it won't open on their Mac
- it'll be blurry, stretched or tiny
- it'll feel different: the car handles wrong, or falls through the floor
- they'll have to use the keyboard

## What the original actually is (checked under Wine)

Plan against this, not against memory:

- **Stage:** a 640×480 stage in a 4:3 window.
- **Title screen:**
  - brushed-metal frame, the chrome "Supersonic RC" logo and the LEGO logo
  - the car on a red, orange and yellow burst
  - "Click to Start"
- **Challenge select ("CHOOSE YOUR CHALLENGE"):** five metal buttons, each with
  its unlock rule:
  - Free Driving
  - Checkpoint Chase
  - Stunt Rally
  - Scavenger Hunt
  - Supersonic Challenge
- **Driving screen:**
  - "Time remaining" at the top left and "Points" at the top right
  - a yellow arrow at the bottom centre pointing to the next checkpoint
  - centred messages ("Checkpoint!", stunt names)
- **Controls:**
  - mouse for menus
  - arrow keys to drive, Space for the handbrake
  - S to mute, Esc to leave the race
  - holding Ctrl at launch unlocks every challenge
- **Missing from the original:** no options screen, no pause, no controller
  support, no saved progress between sessions.

## The journey, stage by stage

### 0. Hearing about it
- **Wants:** to know if they can play Supersonic RC on their Mac, properly.
- **Sees:** the GitHub page.
- **Expects:** a gameplay screenshot, a Download button, and one honest line:
  "Bring your own copy of Supersonic RC."
- **Delighted by:**
  - a crisp widescreen shot of the toy shop
  - "No Shockwave, no emulator, plays with a controller"
- **Dislikes:** build instructions first, jargon, "personal use only".
- **We must:** lead the README with the picture, the download and three
  steps. Developer instructions go below the fold.

### 1. Download and install
- **Does:** downloads a zip and drags Supersonic RC to Applications.
- **Expects:** a normal Mac app of a sensible size.
- **Dislikes:** the unsigned-app warning, which feels like malware.
- **We must:** show the exact Privacy & Security → "Open Anyway" step, with a
  screenshot, on the download page. The app isn't notarised.

### 2. First launch: "where's my game?"
- **Wants:** to be driving. Now.
- **Sees:** a window in the game's own style (brushed metal, chrome heading):
  "Choose your Supersonic RC files". If `Supersonic_RC.zip` is already in
  Downloads, Desktop or Documents, this step happens on its own.
- **Does:** nothing, or picks the zip or `game.dcr`, or drags it onto the
  window.
- **Expects:** it accepts what they actually have: the zip, the unzipped
  folder, or `game.dcr` alone.
- **Delighted by:**
  - drag and drop working
  - "Supersonic RC ✓" in under a second
  - the title screen appearing straight away
- **Dislikes:** errors like "manifest missing" or "DCR parse failed".
- **We must:** speak plainly:
  - Wrong file: "This isn't Supersonic RC. Choose `Supersonic_RC.zip` or
    `game.dcr`."
  - Damaged copy: "This copy of Supersonic RC is damaged. Try downloading it
    again."

### 3. Preparing (about a second)
- **Sees:** one short "Getting your game ready…" line, or nothing at all,
  because conversion takes under a second.
- **Dislikes:** a frozen window, or having to do it again next time.
- **We must:**
  - copy the player's original files into the user folder
  - convert once and never ask again
  - re-convert silently when an update changes the converter

### 4. First boot
- **Expects:** the title screen they remember, then the challenge select.
- **Delighted by:**
  - a sharp picture filling the window, the 4:3 menus never stretched
  - a controller that just works: A or ✕ starts, the D-pad moves between
    challenges, with matching button prompts
  - the engine noise and sounds as they were
- **Dislikes:**
  - a tiny window or blur
  - mouse only
  - a controller that does nothing on the menus
- **We must:** auto-detect the controller, make every menu controller-first,
  and screenshot exactly this moment every milestone.

### 5. Driving
- **Wants:** it to feel exactly as they remember:
  - the same car, toy shop and dinosaur
  - the same ramps, stairs and shelves
  - the same jumps and stunts
- **Delighted by:**
  - smooth high frame rates
  - analogue throttle on the triggers and analogue steering, if they feel right
  - rumble on crashes and landings
- **Dislikes:**
  - anything that looks wrong (a black dinosaur, wrong textures)
  - anything that behaves wrong: falling through stairs or ramps, getting
    stuck in shelves, a car that handles differently
  - input lag
- **We must:** compare every model, texture and surface against the original
  under Wine (the baseline in [PRODUCT_RULES.md](PRODUCT_RULES.md)) before
  calling the world done.

### 6. Pausing and settings
- **Does:** presses Esc, or Start or Options on the controller, mid-race.
- **Expects:** the race freezes and a few clear choices appear:
  - Resume
  - Restart
  - Settings
  - Quit to challenges

  Back resumes.
- **Delighted by:**
  - the menu looks like the game's metal panels
  - a picture change shows live behind the menu
  - it remembers next time
- **Dislikes:**
  - a developer panel full of words like "MSAA" or "physics substeps"
  - having to use the Mac menu bar
  - a button press leaking into the race when the menu closes
- **Note:** the original has no pause, and Esc leaves the race. Pause is a
  modern addition the player expects, so it's on by default. "Esc leaves
  the race" stays available as an option.

### 7. Settings from the menus
- **Expects:** a Settings item on the title screen and on the challenge
  select. The original has no options screen, so this is the one new door
  on its menus.
- **Delighted by:** a few plain choices in the game's style, remembered
  across launches:
  - Picture: window or full screen, sharpness
  - Sound: effects volume, mute
  - Controls: controller layout, analogue pedals on or off, rumble
- **Dislikes:** losing the original look, or a settings screen that doesn't
  feel like Supersonic RC.
- **We must:** return to the screen they came from on Back.

### 8. Progress
- **Expects:** unlocked challenges stay unlocked. In 2003 they reset every
  time the game was closed.
- **Delighted by:** it just works.
- **Dislikes:** lost unlocks, or file management.

### 9. Coming back tomorrow
- **Does:** double-clicks Supersonic RC.
- **Expects:** the title screen. No launcher, no setup, no questions.
- **Dislikes:**
  - a macOS permission prompt about Downloads
  - "Can't find your game" because they tidied Downloads
- **We must:** play from the copy kept in the user folder.

### 10. When things go wrong
| Situation | What the player should see |
|---|---|
| Controller unplugged mid-race | Race pauses: "Controller disconnected. Reconnect it, or press Esc to use the keyboard." |
| Window loses focus | Race pauses, sound fades. It resumes where they left it. |
| Car falls out of the world | Never happens. If it does, the original's pick-up puts the car back on the surface above (pickupManager). It must never land inside scenery. |
| App updated | A short "Updating Supersonic RC…" line, never the full setup again. |
| Game files deleted | "Supersonic RC needs your game files again." [Choose files] |

## What they don't care about

Shockwave, Lingo, W3D, Havok, Jolt, GDExtension, renderers by name, MSAA
levels, physics substeps, environment variables. These stay invisible, or
behind plain words ("Sharp", "Smooth", "Original").

Mods, faster cars, new modes and AI racers may delight some players later,
as clearly labelled extras that are off by default. They never belong in
the core journey.

## Journey acceptance (screenshot every milestone)

First launch → files found → title → challenge select (with controller) →
race → pause → change a setting → resume → finish or quit → Settings from
the title → Back → quit → relaunch straight to the title.
