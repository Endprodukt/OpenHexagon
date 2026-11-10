# Building

The VectorPie SDK is a git submodule: after cloning, run `git submodule update --init`.

## PC (Windows, native)

Needs MSYS2 with MinGW-w64 GCC and SDL2 in `C:\msys64\mingw64`, plus CMake and Ninja.

```bash
export PATH="/c/msys64/mingw64/bin:$PATH"
cmake -S . -B build/pc -G Ninja -DCMAKE_C_COMPILER=C:/msys64/mingw64/bin/gcc.exe -DCMAKE_CXX_COMPILER=C:/msys64/mingw64/bin/g++.exe -DCMAKE_PREFIX_PATH=C:/msys64/mingw64
cmake --build build/pc
build/pc/openhexagon.exe --windowed
```

From PowerShell, put `C:\msys64\mingw64\bin` first in `PATH` too, or g++ stops without a message. The compilers are named with their full paths: with another toolchain earlier in `PATH` (the Pi cross compiler), a reconfigure would otherwise pick that one.

CMake copies `assets/` (level packs and sounds) and `package/game.ini` next to the program. Settings, best times, tables and replays are in `build/pc/save`.

Keys on a PC (AdvanceMAME's defaults): left/right arrows turn, left Ctrl = Focus, left Alt = Swap, Space = back to the level select, 1 = Start, 5 = coin, F2 = service menu, Esc = quit. The mouse is the spinner.

`VP_STATS=1` prints what a frame costs once a second (lines, jumps, lengths).

## Raspberry Pi (arm64)

As for the Tempest 2000 port: clang and lld on the PC against a copy of the cabinet's libraries (a sysroot). Its place is set in `cmake/toolchain-pi-clang.cmake` as `OH_PI_SYSROOT`; the maintainer's is `F:/Tempest 2000 Port/build/pi-sysroot`. How to make one is in the Tempest 2000 port's BUILD.md.

```bash
export PATH="<clang toolchain>/bin:/c/msys64/mingw64/bin:$PATH"
cmake -S . -B build/pi -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-pi-clang.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build/pi
```

## Package for the cabinet

After the Pi build:

```bash
sh tools/make_vpk.sh
```

This writes `build/openhexagon-<version>.vpk`: the Pi program, `game.ini`, `leds.cfg` (lamp colors, control help), the marquee and `assets/`. To install, copy it to a USB stick's `games` folder, or straight to `/persistent/games/packages/` on the cabinet, then Settings, GAMES, INSTALL GAMES. The `save` folder is kept on an update.

For your own cabinet with the Steam version's music (never to be given away):

```bash
sh tools/make_vpk.sh personal
```

This writes `build/openhexagon-<version>-personal.vpk` (about 60 MB). `tools/steam_music.sh` takes the 21 tracks and the sounds `blip.ogg`, `swapBlip.ogg`, `fanfare.ogg`, `failure.ogg` from `C:\Program Files (x86)\Steam\steamapps\common\Open Hexagon` (elsewhere: `OH_STEAM_DIR`). For the PC build: `sh tools/steam_music.sh build/pc/assets`. The Steam packs are otherwise the same as the GitHub sources (only the line endings differ).

To test without installing, copy `package/run.sh` next to the program. It ends the menu, starts the game on the vector board and brings the menu back afterwards. The frame cost is then in `run.log`.

## Music

Out of the box the game makes its own music, in time with the level (service menu, MUSIC: MADE BY THE GAME). To write samples of every level:

```bash
cd build/pc
OH_MUSIC_DEMO=60 SDL_AUDIODRIVER=dummy ./openhexagon.exe --windowed 2>&1 | grep music-demo
```

This writes 60 seconds per level as WAV to `save/music_demo/` (about 250 MB in all) and prints the tempo, the pulse spacing in game ticks and the time it took to make.

The original's music is not part of the open-source sources. With the Steam version, put its `.ogg` files (from `Packs/<pack>/Music/`) into the game's `save/music` folder; on the cabinet that is `/persistent/games/openhexagon/save/music/`. The file name must be the `file_name` in the music JSON, `callMeKatla.ogg` for one. They can also go into `assets/Packs/<pack>/Music/`, as in the original. A level whose file is missing gets the generated music.

On the cabinet the menu's MUSIC VOLUME and SOUND VOLUME apply to the game (SDK 0.9): the music plays as an SDK stream, the effects as sounds. The game's own volumes apply on top.

## Settings

`save/settings.ini` is written by the service menu (F2):

- `mode` (`home` or `arcade`), `free_play`, `lives` (1 to 5), `attract_sound`, `select_seconds`
- `volume`, `music_volume`, `effects_volume` (0 to 100; on top of the cabinet menu's volumes)
- `sensitivity` (spinner: 1.0 means 1200 counts for one turn), `spinner_reversed`
- `setup` (`cabinet` or `pc`): how the levels and the tutorial name the controls (spinner and keys always work together; an old `played_with` is ignored)
- `button_left`, `button_right`, `button_focus`, `button_swap`, `button_back` (controls as text, `+` between them)
- `arcade_difficulty` (`choose`, `easiest`, `normal`), `game_speed` (50 to 150), `rotation`, `pulse`, `rotate_to_start`, `invincible`, `auto_restart`
- `music` (`made`, `files` or `off`; without the line: `files` when original music is there, else `made`. With `files`, a level without its file gets the generated music.)
- `3d`, `3d_depth` (percent), `wall_fill` (`off`, `low`, `high`), `tints`, `spokes` (background sides as lines), `flash`, `trail`, `simplify` (draw too busy levels simpler, `no` out of the box)
- `frame_hz` (60, 50 or 40, from the next start), `screen_size` (percent), `view_size` (100 to 135: how much of the field is seen)
- `bright_walls`, `bright_3d`, `bright_background`, `bright_player`, `bright_text` (90, 80, 45, 100, 90 out of the box)

## Marquee

`package/marquee.png` is cut for the cabinet's HDMI screen (1920 x 360): the middle of the owner's marquee (2000 x 228) around the logo, otherwise `vector_aux_marquee_display_mode stretch` would squeeze it.

## More levels

Besides the built-in packs, the game loads every pack in `save/packs/<pack>/` (on the cabinet `/persistent/games/openhexagon/save/packs/`). A pack is laid out like those in `assets/Packs/`: `pack.json`, `Levels/`, `Scripts/`, `Styles/`, optionally `Music/` and `Sounds/`. The Steam version's workshop levels are in `steamapps/workshop/content/1358090/<number>/` once subscribed and can be copied in like that. The folder is kept on updates. A pack without music gets the generated music.

## Saves

In the `save` folder:

- `scores.txt`: the home mode's personal bests, as in the original per level and difficulty.
- `arcade/<level>.txt`: the high score table per level, times in hundredths of a second.
- `highscores.txt`: the best table over all levels, for the cabinet menu.
- `replays/<level>_<difficulty>x.ohr`: the best run per level, in the original's replay format, with `.ohr.spin` (the spinner steps). The attract demo and the level select's preview play them.

## Making the logo font again

```bash
python tools/make_logo_font.py
```

Reads `OpenSquare-Bold.ttf` from the original's assets and writes `src/vp/gen/logo_font.inc` (outlines and triangles of the letters of OPEN HEXAGON). Do not edit the generated file by hand.

## Self test

Plays every level at every difficulty with the autopilot, up to N seconds, and reports Lua errors:

```bash
cd build/pc
OH_SELFTEST=40 SDL_AUDIODRIVER=dummy ./openhexagon.exe --windowed 2>&1 | grep selftest
```

The last line must say `0 with errors`. The `selftest-color` lines give each level's darkest wall color before lifting (0 to 255) and whether the style is drawn inverted.

Test help: `OH_LEVEL=<part of the level id>` starts that level at once, `OH_LEVEL=babysteps` for one. Careful, such runs save best times.

## Screenshots

`tools/snap.ps1 -Out <path> [-Keys ...]` starts the game, sends keys (virtual key codes in hex, `+`/`-` to hold/release, `wNNN` to wait, `snap` for a picture) and saves the window as PNG. Start it from `build/pc`, or the game does not find its `save` folder.
