# Continuing the work

## Layout

- `reference/SSVOpenHexagon/`: a clone of vittorioromeo/SSVOpenHexagon with the submodules SSVUtils, SSVStart, vrm_pp. Read only, not in git.
- `vendor/VectorPie/`: submodule, Goldfinch2/VectorPie (SDK 0.9.0, 2026-11-10). The SDK is in `sdk/`, its documentation in `sdk/docs/`.
- `vendor/lua-5.1.5/`: Lua from lua.org. `vendor/SSVUtils/`, `vendor/vrm_pp/`: the original's header libraries.
- `src/oh/`: the original's game core (`include/SSVOpenHexagon/...`, `src/...`, same file names). Every difference is commented `VectorPie`. Written anew, with the same interfaces:
  - `Global/Config.*`: only the getters the core needs, with the original's defaults.
  - `Global/Assets.*`: the original's loading functions (`loadPackData`, `loadPackAssets_*`) taken over unchanged, without textures, fonts, shaders, profiles. Best times in `save/scores.txt`.
  - `Global/Audio.*`: on `vp_audio`. Effects as SDK sounds; music decoded with the SDK's stb_vorbis (or generated) and fed to an SDK stream from the segment's time (`feed`, called by `Audio::update` every step).
  - `Core/HGGraphics.cpp`: `draw()` fills the buffers as the original and keeps the 3D layers in `vpLayers3D`.
  - `Core/HGVectorPie.cpp`: what GameState and the input callbacks did (`tick`, `setInputs`, `requestRestart` ...), plus `vpDegPerTick` and the demo autopilot.
  - `Core/Steam.hpp`, `Discord.hpp`, `HexagonClient.hpp`: empty stand-ins; the pointers are always `nullptr`.
- `src/compat/`: small stand-ins for SFML types (`Vector2`, `Color`, `Vertex`, `View`, `Sprite`, `Text`, `Shader`) and SSVStart (`Camera`, `GameWindow`, `Vector2.hpp` as a copy).
- `src/vp/`: the VectorPie side.
  - `main.cpp`: `vp_run`, 60 Hz update, frame rate from the settings.
  - `app.cpp`: modes home menu, home game, title, warning, high scores, demo, level select, play, continue, game over, initials, error. Arcade score: `A.best`, the best single attempt over all continues of a credit; each life restarts with `restartFromStart` (a first start). Live preview of the level select (`updatePreview`). Self test `OH_SELFTEST`. Autopilot runs always with `startLevel(..., true)`, or it saves best times.
  - `render.cpp`: the game's buffers to lines, text, particles, menu background, `ui::` helpers (384 x 288, y down), `ui::wrap`. `LOOKS`: levels slimmed by hand.
  - `occlude.cpp`: hidden lines in the plane (layers, union within a layer, grid).
  - `controls.cpp`: keys and spinner; the spinner 1:1.
  - `service.cpp`: the service menu, after the Tempest 2000 port's `src/service.c`. Row hints through the `hints` list in `serviceDraw`.
  - `settings.cpp`: `save/settings.ini`.
  - `synth.cpp`: the synthesizer (kick, snare, clap, hats, crash, bass with wobble or envelope, pad, lead with delay, riser, sidechain, limiter), one track from a `TrackSpec`. Envelopes multiply instead of `exp` per sample; only that is fast enough for the Pi. Built with `-O3 -ffast-math`.
  - `music.cpp`: tempo and phase from the beat pulse (`beatSync`), a cache with a thread (`musicFor`), the start point (`musicStartFrame`). Hooked up through `HexagonGame::vpUseMadeMusic`, `vpOnStart` (in `start()`, before `resumeMusic`) and `Audio::playGenerated`.
  - `hints.cpp`: the text filter for the levels' control hints (`HexagonGame::vpTextFilter`), the swap hint, and extra scripts `assets/VectorPie/<level>.lua` (`vpAfterLevelScript`, with the Lua variable `vp_cabinet`; none at the moment). A new message naming keys in a pack has to be added there, or the PC text shows.
- `assets/`: `Packs/` and `Assets/` (sounds) from the original's `_RELEASE`. The game changes into this folder at start, as the original into its game folder.
- `package/`: `game.ini`, `leds.cfg`, `run.sh`, `marquee.png`. `tools/make_vpk.sh` builds the package, `tools/snap.ps1` takes screenshots.

## Conventions

- Everything in the repository is in English: README, STATUS, RESUME, BUILD, code comments, texts on screen.
- Change the core in `src/oh` as little as needed and mark each change `VectorPie`. New functions go into `HGVectorPie.cpp` or `src/vp`, not into the original files.
- Do not guess game values: they come from the original (Config defaults, level JSON, Lua). What is invented (arcade, line drawing) is listed in STATUS.md under the limits.
- Never re-sort lines per frame (by shortest jump, say): a changing order makes the picture wobble on the tube. The order must stay the same from frame to frame.
- START 1 + START 2 together is the cabinet's exit and must trigger nothing. The service menu opens with F2 only.
- Run the self test after changes to the core (BUILD.md).
- Simpler-drawing steps (`_lod` in `render.cpp`, only with `simplify`) must never drop the walls' fill lines. A level too busy for the board goes into `LOOKS` instead.
- Long texts are wrapped with `ui::wrap`, never cut.
- The personal package (Steam music) is never published. A GitHub release carries the normal `.vpk` only.
- Python scripts with `\n` in strings: do not send them through the Bash as a heredoc (this machine's Bash turns `\\n` into a real line break). Write the script to a file, then run it.

## Pointers into the original

- `src/SSVOpenHexagon/Core/HGUpdate.cpp`: `update`, pulse, beat pulse, rotation, walls, input.
- `Core/HGGraphics.cpp`: drawing order and 3D layers (the model for `render.cpp`).
- `Core/HGScripting.cpp` and `Core/LuaScripting.cpp`: the Lua API.
- `Core/MenuGame.cpp`: the original's menu (not ported).
- `Global/Config.cpp`: defaults (`X_LINKEDVALUES`).

## Next steps

1. Feedback from the cabinet: SDK 0.9 volumes (menu MUSIC and SOUND VOLUME), the music stream, Pointless (walls with fill lines), continue flow, warning screen, Arcadia slimmed (does it still flicker?).
2. The open tasks in STATUS.md.
