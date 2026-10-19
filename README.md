# Open Hexagon for VectorPie

Open Hexagon by Vittorio Romeo ([github.com/vittorioromeo/SSVOpenHexagon](https://github.com/vittorioromeo/SSVOpenHexagon)) as a vector game for the VectorPie cabinet: Raspberry Pi 5, USB-DVG, Wells-Gardner 6100.

- The game core is the original's code: player, walls, custom walls, timelines, styles, Lua API, replays. It lives in `src/oh` and runs at 240 ticks per second like the original.
- The original's level packs (`cube`, `hypercube`, `orthoplex`, `tutorial`, plus `base`) run unchanged under Lua 5.1.
- The graphics are new, in `src/vp`: the original's filled areas become outlines. The 3D effect is drawn as a solid body with hidden edges.
- Two modes, switchable in the service menu (F2):
  - **Home** as in the original: pick level and difficulty, best times, restart, replay.
  - **Arcade**: coins, attract loop (title, epilepsy warning, high scores, demo), timed level select, several lives per credit (the best attempt counts), continue with a countdown, high score table per level, initials.
- Controls with spinner, keys or joystick, all in parallel and bindable in the service menu.

Building: [BUILD.md](BUILD.md). Status and open points: [STATUS.md](STATUS.md). Continuing the work: [RESUME.md](RESUME.md).

## Licenses

- Code of Open Hexagon, SSVUtils, SSVStart: Academic Free License 3.0 (`src/oh/LICENSE`, `vendor/SSVUtils/LICENSE`, `src/compat/SSVStart/LICENSE`).
- Sound effects: `assets/Assets/Open Hexagon Assets License.txt`. They may only be used with Open Hexagon.
- The original's music is not part of the source on GitHub. Out of the box the game plays a soundtrack it generates itself (src/vp/synth.cpp, free). The Steam version's `.ogg` files can optionally be put into the game's `save/music` folder (see BUILD.md).
- Lua 5.1.5: MIT license (`vendor/lua-5.1.5/COPYRIGHT`).
