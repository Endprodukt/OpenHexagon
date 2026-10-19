# Bauen

## PC (Windows, nativ)

Voraussetzung: MSYS2 mit MinGW-w64-GCC und SDL2 unter `C:\msys64\mingw64`, dazu CMake und Ninja.

```bash
export PATH="/c/msys64/mingw64/bin:$PATH"
cmake -S . -B build/pc -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_PREFIX_PATH=C:/msys64/mingw64
cmake --build build/pc
build/pc/openhexagon.exe --windowed
```

CMake kopiert `assets/` (Level-Pakete und Sounds) und `package/game.ini` neben das Programm. Einstellungen, Bestzeiten, Tabellen und Replays liegen in `build/pc/save`.

Tasten am PC (die Vorgaben von AdvanceMAME): Pfeile links/rechts drehen, Linke Strg = Focus, Linke Alt = Swap, Leertaste = zurück zur Levelwahl, 1 = Start, 5 = Münze, F2 = Service-Menü, Esc = Ende. Die Maus ist der Spinner.

`VP_STATS=1` gibt einmal pro Sekunde die Kosten eines Bildes aus (Linien, Sprünge, Weglängen).

## Raspberry Pi (arm64)

Wie beim Tempest-2000-Port: clang und lld vom PC gegen die Kopie der Bibliotheken des Automaten. Die Sysroot wird von dort mitbenutzt (`F:/Tempest 2000 Port/build/pi-sysroot`, in `cmake/toolchain-pi-clang.cmake` als `OH_PI_SYSROOT` einstellbar). Wie sie angelegt wird, steht in der BUILD.md des Tempest-Ports.

```bash
export PATH="/c/Users/ocare/.local/share/retcomm/toolchains/cmake-clang-v1/latest/bin:/c/msys64/mingw64/bin:$PATH"
cmake -S . -B build/pi -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-pi-clang.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build/pi
```

## Paket für den Automaten

Nach dem Pi-Build:

```bash
sh tools/make_vpk.sh
```

Für den eigenen Automaten mit der Musik der Steam-Version (der Nutzer hat sie seit 2026-10-18; nur für ihn, nicht weitergeben):

```bash
sh tools/make_vpk.sh personal
```

Das schreibt `build/openhexagon-<version>-personal.vpk` (rund 60 MB). `tools/steam_music.sh` holt dafür die 21 Musikstücke und die Sounds `blip.ogg`, `swapBlip.ogg`, `fanfare.ogg`, `failure.ogg` aus `C:\Program Files (x86)\Steam\steamapps\common\Open Hexagon` (anderer Ort: `OH_STEAM_DIR`). Für den PC-Build: `sh tools/steam_music.sh build/pc/assets`. Die Steam-Pakete sind sonst gleich mit dem GitHub-Stand (nur die Zeilenenden unterscheiden sich).

Das normale Paket `build/openhexagon-<version>.vpk` enthält Pi-Programm, `game.ini`, `leds.cfg` (Lampenfarben, Bedienhilfe) und `assets/`. Installieren: auf einen USB-Stick in den Ordner `games` kopieren, oder direkt nach `/persistent/games/packages/` auf dem Automaten. Danach im Menü Settings, GAMES, INSTALL GAMES. Der Ordner `save` bleibt bei einem Update erhalten.

Zum Testen ohne Installation kann `package/run.sh` neben das Programm kopiert werden. Es beendet das Menü, startet das Spiel auf dem Vektor-Board und holt das Menü danach zurück. Die Bildkosten stehen dann in `run.log`.

## Musik

Ab Werk erzeugt das Spiel seine Musik selbst, im Takt mit dem Level (Service-Menü, MUSIC: MADE BY THE GAME). Hörproben aller Level schreibt:

```bash
cd build/pc
OH_MUSIC_DEMO=60 SDL_AUDIODRIVER=dummy ./openhexagon.exe --windowed 2>&1 | grep music-demo
```

Das legt je Level 60 Sekunden als WAV nach `save/music_demo/` (zusammen rund 250 MB) und nennt Tempo, Pulse-Abstand in Spieltakten und die Zeit für das Erzeugen.

Die Musik des Originals gehört nicht zum Open-Source-Teil. Wer die Steam-Version hat, legt deren `.ogg`-Dateien (aus `Packs/<pack>/Music/`) in den Ordner `save/music` des Spiels. Auf dem Automaten ist das `/persistent/games/openhexagon/save/music/`. Der Dateiname muss dem `file_name` in der Musik-JSON entsprechen, zum Beispiel `callMeKatla.ogg`. Alternativ können sie in `assets/Packs/<pack>/Music/` liegen, wie im Original. Fehlt ein Stück, läuft das Level ohne Musik.

## Einstellungen

`save/settings.ini` wird vom Service-Menü (F2) geschrieben:

- `mode` (`home` oder `arcade`), `free_play`, `lives` (1 bis 5), `attract_sound`, `select_seconds`
- `volume`, `music_volume`, `effects_volume` (0 bis 100)
- `sensitivity` (Spinner: 1.0 heißt 1200 Zählschritte für eine Runde), `spinner_reversed`
- `setup` (`cabinet` oder `pc`): wie die Level und das Tutorial die Steuerung nennen (Spinner und Tasten gehen immer beide; ein altes `played_with` wird überlesen)
- `button_left`, `button_right`, `button_focus`, `button_swap`, `button_back` (advmame.rc-Namen)
- `arcade_difficulty` (`choose`, `easiest`, `normal`), `game_speed` (50 bis 150), `rotation`, `pulse`, `rotate_to_start`, `invincible`, `auto_restart`
- `music` (`made`, `files` oder `off`; fehlt der Eintrag: `files`, wenn Original-Musik da ist, sonst `made`. Mit `files` bekommt ein Level ohne seine Datei die erzeugte Musik.)
- `3d`, `3d_depth` (Prozent), `wall_fill` (`off`, `low`, `high`), `tints`, `spokes` (Hintergrund-Seiten als Linien), `flash`, `trail`, `simplify` (zu volle Level einfacher zeichnen, ab Werk `no`)
- `frame_hz` (60, 50 oder 40, gilt ab dem nächsten Start), `screen_size` (Prozent), `view_size` (100 bis 135: wie viel vom Spielfeld zu sehen ist)
- `bright_walls`, `bright_3d`, `bright_background`, `bright_player`, `bright_text` (ab Werk 90, 80, 45, 100, 90: die Werte des Automaten vom 2026-10-19)

## Marquee

`package/marquee.png` ist auf den HDMI-Schirm des Automaten zugeschnitten (1920 x 360): aus dem Marquee des Nutzers (2000 x 228) die Mitte um das Logo, sonst würde es mit `vector_aux_marquee_display_mode stretch` gequetscht.

## Weitere Level

Neben den eingebauten Paketen lädt das Spiel jedes Paket aus `save/packs/<paket>/` (auf dem Automaten `/persistent/games/openhexagon/save/packs/`). Ein Paket hat denselben Aufbau wie die in `assets/Packs/`: `pack.json`, `Levels/`, `Scripts/`, `Styles/`, wahlweise `Music/` und `Sounds/`. Workshop-Level der Steam-Version liegen nach dem Abonnieren unter `steamapps/workshop/content/1358090/<nummer>/` und können so hineinkopiert werden. Der Ordner bleibt bei Updates erhalten. Ein Paket ohne Musik bekommt die erzeugte.

## Spielstände

Im Ordner `save`:

- `scores.txt`: die persönlichen Bestzeiten des Home-Modus, wie im Original je Level und Schwierigkeit.
- `arcade/<level>.txt`: die Bestenliste je Level, Zeiten in Hundertstelsekunden.
- `highscores.txt`: die beste Liste über alle Level, für die Anzeige im Menü des Automaten.
- `replays/<level>_<schwierigkeit>x.ohr`: der beste Lauf je Level, im Replay-Format des Originals. Das Demo im Attract-Modus und die Vorschau der Levelwahl spielen sie ab.

## Logo-Schrift neu erzeugen

```bash
python tools/make_logo_font.py
```

Liest `OpenSquare-Bold.ttf` aus den Assets des Originals und schreibt `src/vp/gen/logo_font.inc` (Konturen und Dreiecke der Buchstaben von OPEN HEXAGON). Die erzeugte Datei nicht von Hand ändern.

## Selbsttest

Spielt jedes Level in jeder Schwierigkeit mit dem Autopiloten, bis zu N Sekunden, und meldet Lua-Fehler:

```bash
cd build/pc
OH_SELFTEST=40 SDL_AUDIODRIVER=dummy ./openhexagon.exe --windowed 2>&1 | grep selftest
```

Die letzte Zeile muss `0 with errors` melden. Die Zeilen `selftest-color` nennen je Level die dunkelste Wandfarbe vor dem Anheben (0 bis 255) und ob der Style invertiert gezeichnet wird.

Testhilfe: `OH_LEVEL=<Teil der Level-Kennung>` startet gleich dieses Level, zum Beispiel `OH_LEVEL=babysteps`. Achtung, solche Läufe speichern Bestzeiten.

## Bildschirmfoto

`tools/snap.ps1 -Out <pfad> [-Keys ...]` startet das Spiel, schickt Tasten (virtuelle Tastencodes in Hex, `+`/`-` für halten/loslassen, `wNNN` warten, `snap` Bild) und speichert das Fenster als PNG. Aus `build/pc` heraus starten, sonst findet das Spiel seinen `save`-Ordner nicht.
