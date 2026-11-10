# Status

As of: 2026-11-10 (version 0.2.0, VectorPie SDK 0.9.0)

## Goal

Open Hexagon on the VectorPie cabinet (Pi 5, USB-DVG, WG6100, horizontal monitor). Controls with a spinner and digital inputs, set up in the service menu (F2). 3D with hidden lines, 60 Hz where possible. A home mode as in the original and an arcade mode with coins, attract loop and so on.

## What works

- PC build and Pi build (aarch64), packages `openhexagon-<version>.vpk` and `openhexagon-<version>-personal.vpk`. Installed on the cabinet since 2026-10-18 (the personal package unpacked to `/persistent/games/openhexagon`, listed under "Other"). Self test on the Pi: 70 runs without errors; making one track on the Pi takes about 0.5 s.
- SDK 0.9.0 (2026-11-10). The cabinet menu's MUSIC VOLUME and SOUND VOLUME now apply to the game: the music plays as an SDK stream (`vp_stream_play`, fed every step from the decoded or generated track, pitch done by the feed itself), the effects as sounds. The game's own three volumes in the service menu apply on top. Checked on the PC with real audio: the stream never ran dry. Restarting a level no longer copies the rest of the track into a new SDK sound.
- Title screen (based on the owner's marquee, `Renderer::drawTitle`): "OPEN" gold to orange above "HEXAGON" in white, in the original's logo font (OpenSquare Bold; `tools/make_logo_font.py` reads the TTF and writes outlines and triangles to `src/vp/gen/logo_font.inc`), as a solid body with red-magenta depth (the same hidden lines as in the game), behind it a slowly turning tunnel of walls from orange through red to magenta. Nothing pulses (the owner did not like the pulsing). Title music: a generated track, 139.8 BPM. In home mode it is the first screen (Start, Focus or Swap: on; Back from the level select: back to it), in arcade mode the title of the attract loop (title music only with attract sound). The marquee is `package/marquee.png`.
- The original's game core in `src/oh`, stripped of SFML, Steam, Discord, ImGui, shaders and the online part. Every change is marked `VectorPie`. 240 ticks per second as in the original, four ticks per 60 Hz step.
- Lua 5.1.5 instead of LuaJIT (both are Lua 5.1). Self test: all 22 levels at all 70 difficulties run without Lua errors.
- Packs `base`, `cube`, `hypercube`, `orthoplex`, `tutorial` from the original's `_RELEASE/Packs`. `experimental` and `workshopexample` are left out.
- The original's sound effects (`.ogg` through the SDK). `blip.ogg` and `swapBlip.ogg` are missing from the GitHub sources and stay silent.
- Original music for the owner's own cabinet: `make_vpk.sh personal` packs the 21 tracks and four missing sounds from the Steam folder. With original music present, the game plays it out of the box (with the original's segments), otherwise the generated music. That package is never published.
- More levels: packs in `save/packs/` load like the built-in ones (tested with the original's example pack `workshopexample`).
- Own soundtrack (license-free hard dubstep / techno, in time with the level), `src/vp/synth.cpp` and `src/vp/music.cpp`. On out of the box (service menu, MUSIC: MADE BY THE GAME, ORIGINAL FILES or OFF).
  - Each level gets its own track (key, chord progression, bass and arpeggio patterns from the level id). From 150 BPM: halftime dubstep with a wobble bass; below: four-to-the-floor techno with a rolling bass. Form: lead-in, intro, build, drop, break, build, drop 2, outro (64 bars, looped).
  - In time: the tempo comes from the original's beat pulse, counted as the game counts (240 ticks per second, difficulty factor, start offset, in float like `updateBeatPulse`). A beat is a whole number of samples. The first pulse falls on the first beat of a part. Levels that pulse on every second beat or on eighths (Evotutorial, G-Force) get 2 or 4 beats per pulse, so the music stays between 120 and 190 BPM. The music does not get faster or higher with the difficulty as in the original; it is made at the right tempo.
  - Made at level start on a thread of its own (about 200 ms per track on the PC), already during the preview in the level select. The last four tracks stay in memory (about 20 MB each).
- Music from `save/music/` or `assets/Packs/<pack>/Music/` (setting ORIGINAL FILES), with the original's segments: the track is decoded once at level start and played from the segment's time, then the whole track loops. Pitch by difficulty as in the original.
- Graphics (`src/vp/render.cpp`, `src/vp/occlude.cpp`): the buffers that `HexagonGame::draw()` fills become outlines. The original's camera with rotation, pulse zoom, 3D skew and shake.
  - Same-colored shapes of a layer merge as in the filled original: shared edges of neighbouring walls are not drawn.
  - Later layers hide earlier ones, in the original's drawing order.
  - 3D: each shape gets a band down to the deepest 3D layer (the convex hull of the shape and the shifted shape). It hides what lies behind it. Its color is that of the original's front 3D layer.
  - Background: the side borders as spokes between sectors of different color.
  - Light styles (dark walls on a light background, the tutorial for one) are drawn inverted, otherwise the lines would be black.
  - Too dark lines are lifted: the original recolors some walls with `transformHue`, which turns grey into black. In the filled original they show against the colored background, on the tube they did not. Walls, pivot and player below 70 of 255 keep their hue at 160; without a hue they take the background's color. This affected G-Force, Slither and at times Disc-O.
  - Walls read as filled: fill lines along a wall between its long sides, service menu WALL FILL (OFF, LOW out of the box, HIGH). The hidden lines were checked: they hide only what the filled original does not show either.
  - Color tones (more color, but not too colorful): the 3D band has the wall's own color, turned 35 degrees in hue and at 80 % (not the original's strongly darkened 3D color, which was hardly visible on the tube). The fill lines are turned 18 degrees the other way. Grey styles get a light steel-blue tone for band and fill. Service menu COLOR TONES (on out of the box).
  - Within a layer only same-colored shapes merge. Of different colors, the later one hides the earlier one, as when painting in the original.
  - Line pieces are drawn in a fixed order, as the shapes are laid out; pieces that join are drawn without a jump. Until 2026-10-18 they were sorted by nearest end every frame, which made the whole picture wobble on the tube.
- The original's text (time, status lines, messages, "NEW PERSONAL BEST!", replay line) in the SDK's Asteroids stroke font, at the original's places.
- The original's particles: the player's trail and swap particles as small hexagons, PB stars as star outlines. The death flash makes all lines brighter.
- Controls (`src/vp/controls.cpp`): spinner and player 1:1 (no after-run). What the spinner turns in a 60 Hz step is spread over the four ticks and is their speed in `CPlayer::updateInputMovement` (`HexagonGame::vpSetSpin`); no limit, no after-run. Being pushed off wall edges works as with keys. Replays keep the spinner steps per tick next to them (`<replay>.ohr.spin`). Keys and joystick move at the original speed; a held key wins. Unlike the original: with the spinner one can turn faster than with keys.
- Menus with the spinner: one step (level, letter, service menu row) per 50 degrees of player turn (`MENU_STEP_DEG`).
- Level select (home and arcade) with a live preview (not from the original): after 0.1 s of rest the chosen level plays silently behind the lettering, from the saved best replay or with the autopilot. At its start it is run 2.5 s ahead unseen (`PREVIEW_FORWARD`), so walls are in the picture at once, and it is invincible, so it runs on without restarts. 2 to 6 ms per start on the PC.
- Tutorials (Baby Steps, Evotutorial; both in the original's tutorial mode, ended by `e_kill`): in arcade mode their end finishes the credit with "TUTORIAL COMPLETE" (no life lost, no table) and returns to the title. They are in the arcade level select too. In a tutorial the arcade HUD shows no best time and no lives.
- Presses are latched in `Controls` (`latched`): when `vp_run` caught up a late frame with several steps, the SDK reported the same press in each of them, and one press left the title and started a level at once. Menu sounds stay audible (`Audio::setQuiet`, `playUiSound`).
- Adjustable view (service menu, VIDEO, VIEW SIZE 100 to 135 %): 100 shows exactly 768 units of height as the original in 4:3, 133 as much width as the original on 16:9. 100 out of the box.
- The original's game settings, service menu GAME: ARCADE DIFFICULTY (PLAYER CHOOSES out of the box: the level's easiest difficulty preselected, START 2 changes it; EASIEST; NORMAL 1X), GAME SPEED 50 to 150 % (the original's timescale), ROTATION and PULSE (no rotation, no pulse), ROTATE TO START, INVINCIBLE and AUTO RESTART (home only). As in the original, best times with help on do not count; in arcade mode they go to tables of their own (`<level>_x<difficulty>_s<speed>_norot_nopulse`). Replays are kept only from runs without help.
  - ARCADE DIFFICULTY picks one of the level's own difficulties: walls faster (difficulty^0.65), shorter delays, faster rotation, and the level script may build harder patterns. GAME SPEED runs the whole game slower or faster, the clock too.
- Home mode: level select over all packs, difficulty, personal best per level and difficulty, restart, replay of the last run, back.
- Arcade mode: coins and credits through `vp_credits` with the start lamp, free play, attract loop (title 8 s, epilepsy warning 6 s, high scores 8 s, demo up to 40 s; scores and demo show no tutorials), timed level select, lives per credit (1 to 5, 3 out of the box).
  - Each attempt starts the level from its beginning, as a real first start (`startLevel`), so the music starts from its beginning too: the generated music at bar 1, the original music at the first segment of the level's JSON. (The original's restart picks a random segment; home mode keeps that.)
  - The score is the best single attempt (no sum; the HUD shows "BEST").
  - After the last life: CONTINUE? with a 10 s countdown (any other button takes 1 s off). START with a credit gives the lives again; the best time stays until no continue is taken.
  - Then the high score table per level with initials: the spinner picks a letter; START 1, Focus, Swap or Back take it; after "-" comes an arrow that goes back one letter. The entry also goes to `highscores.txt` for the cabinet menu.
- Levels with swap (`l_setSwapEnabled`, not the tutorials) show "USE THE SWAP BUTTON TO MASTER THIS LEVEL" at the bottom for 4 s at the credit's first start, or at the start in home mode (on a PC with the key; `swapHint` in hints.cpp).
- Demo in the attract loop: plays a saved best replay (the original's format). Without one, a simple autopilot plays. Autopilot runs (demo, preview, self test) save no best times (`vpDisableScores`).
- Control words of the levels: service menu, CONTROLS, SETUP (CABINET or PC). Spinner and keys always work together. The scripts' messages go through a text filter (`src/vp/hints.cpp`): on the cabinet "the focus button" and "the swap button" instead of "left shift" and "space", on a PC the bound key (AdvanceMAME defaults, "left ctrl" for one). The tutorial names both for turning ("turn the spinner or use left/right", on a PC "move the mouse or use ..."). Levels that need to do something differently can have `assets/VectorPie/<level>.lua` (run after the original's script; none at the moment). Affected: Baby Steps, Evotutorial, Slither, and the hints after a death. The packs' Lua files stay unchanged.
- Text sits on its letters: the SDK counts the spacing after the last letter, centered text was half a space too far left. `strokeText` in render.cpp corrects it. In arcade mode there are no restart hints after a death (play goes on by itself). Behind game over and initials the game is dimmed to 35 % and drawn without its texts.
- Texts: service menu hints are wrapped (`ui::wrap`, at most 300 wide) and sit above the green help lines. Level names too wide for one line are set in two smaller ones (level select, high scores, initials). In the game, the levels' messages always sit below the status lines on the left; a line far too wide is wrapped, one just too wide is set a little smaller.
- Service menu (F2), laid out and used like the Tempest 2000 port's: game mode, free play, lives, select time, attract sound, music source, three volumes; page CONTROLS with a spinner test (one spinner turn should take the player once around), sensitivity, direction, five buttons; page VIDEO with 3D on/off, 3D depth, wall fill, color tones, background sides, death flash, trail, simpler when busy, frame rate, screen size, view size, five brightnesses; page GAME. SAVE writes `save/settings.ini`.
- Bindings as in all of the owner's vector games (`src/vp/vpbind.[ch]`, a copy of the module from the Tempest 2000 port): all inputs read directly (keys `key_...`, joystick `joyN_buttonM` / `_hatH_up` / `_axisA_neg`, mouse buttons `mouse_left`), several joined with `+`; old advmame.rc names are converted on loading. Service menu: START/Enter on the row, the first control replaces, each further one adds (up to 4), Delete = standard, START 2 = none, left/right = none/standard; START/COIN/exit/F2/Esc/Enter/Delete cannot be bound. All menus use fixed inputs only.
- `leds.cfg` with lamp colors and the control help (spinner, Focus, Swap).
- Video defaults are the values set on the owner's cabinet (2026-10-19): brightness walls 90, 3D 80, background 45, player 100, text 90.

## Frame cost (PC window, `VP_STATS=1`)

- Simpler drawing for levels the board cannot draw in time, only with the option SIMPLER WHEN BUSY (service menu VIDEO, `simplify`, off out of the box: every level is first shown in full). A frame over 110,000 lit units (`LIT_BUDGET`) moves on one step for the rest of the level: 1 no background spokes, 2 only a circle around the middle (0.8 of the height), 3 no 3D sides on the walls. The walls' fill lines are never dropped: a wide wall without them is an empty outline and is not seen as a danger (Pointless). Never back within the same level, or the picture would switch back and forth. Whether 110,000 is the right limit for the WG6100 has to be seen on the cabinet.
- Arcadia is slimmed by hand instead (`LOOKS` in `render.cpp`): no background spokes, no 3D sides, each thin bar as one line along its middle (it still hides as the whole bar). Peak about 100,000, average about 55,000, instead of 420,000 / 205,000.
- Menu: about 350 lines, 35,000 units lit, 30,000 of jumps.
- Game: 50,000 to 110,000 units lit. Peaks come after a death from the big PB text and the stars.
- Whether the WG6100 draws that cleanly at 60 Hz is open. 50 and 40 Hz can be set in the service menu, background sides and 3D can be switched off. The live preview costs as much as the game; a larger view shows more walls and costs more.

## Limits and differences from the original

- Not from the original (made for the cabinet): the soundtrack, fill lines and color tones, replaced control texts, the lifting of dark lines, the whole arcade mode (coins, lives, tables, attract, demo autopilot), the service menu, inverted light styles, the line drawing (background spokes, a 3D band instead of 15 filled copies), the trail four times as bright as in the original (alpha 35 would not show on the tube), swap particles twice as bright.
- Left out: shaders (the `shdr_*` functions exist and do nothing), the original's key icons and level info box (textures), online leaderboards, profiles, workshop, Lua console, screenshots, `u_isKeyPressed` and `u_isMouseButtonPressed` (always false).
- The original's main menu (`MenuGame.cpp`, profiles, options, favourites, pack switching) is not ported. The level select is a page of its own with the same details (pack, name, author, description, difficulty, best time) and the live preview.
- The music's phase matches the game tick exactly. The sound's output latency (SDL buffer, about 20 ms) is not compensated; check on the cabinet. A pitch change reaches the music up to 0.2 s late (the stream is written that far ahead).
- The original music is not in the GitHub sources (`*.ogg` in the original's `.gitignore`): 21 tracks by BOSSFIGHT, Dunderpatrullen and Fantomenk, only in the Steam version.
- The hints after a death read "PRESS START TO RESTART" and "PRESS SWAP TO REPLAY" (the cabinet's buttons instead of the original's keys).
- The demo autopilot is weak (usually dies after 2 to 4 seconds). Once people have played, the demo shows real best replays.
- With the original music, a level's first start decodes the track on the main thread (up to 100 ms on the PC, longer on the Pi): a short hitch. It takes about 40 MB while the level runs.
- The frame rate (60/50/40 Hz) applies from the next program start.
- Not yet tested on the cabinet: frame cost on the tube, spinner direction and sensitivity, lamps, F2 without a window, sound levels with the menu's volumes (SDK 0.9).

## Open tasks

- Seen 2026-10-18 in the attract demo (a replay, yellow style, level not identified): two thin green lines from the ends of a wall straight down to the bottom of the picture. Find the level, and whether a wall with a far corner is drawn like that.

1. Test on the cabinet: frame rate and cost, spinner calibration (spinner test in the service menu), buttons, music and effect volumes with the menu's settings.
2. Improve the demo autopilot or ship some replays.
3. If the frame cost is too high: smaller in-game text, fewer PB stars, drop walls outside the picture earlier.
4. Decode the original music off the main thread.

## Releases

- `v0.1.0` (2026-10-19): `openhexagon-0.1.0.vpk`.
- `v0.2.0` (2026-11-10): SDK 0.9.0, the cabinet menu's volumes, music as a stream; arcade rework (best attempt, continue), epilepsy warning, swap hint, wrapped texts, initials with any button and a back arrow.
- Only the normal package goes to GitHub. The personal package with the Steam music never does.
