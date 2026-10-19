# Weiterarbeiten

## Aufbau

- `reference/SSVOpenHexagon/`: Klon von vittorioromeo/SSVOpenHexagon mit den Submodulen SSVUtils, SSVStart, vrm_pp. Nur lesen.
- `vendor/VectorPie/`: Klon von Goldfinch2/VectorPie (Stand 2026-10-15). Das SDK liegt in `sdk/`, die Doku in `sdk/docs/`.
- `vendor/lua-5.1.5/`: Lua von lua.org. `vendor/SSVUtils/`, `vendor/vrm_pp/`: Header-Bibliotheken des Originals.
- `src/oh/`: der Spielkern des Originals (`include/SSVOpenHexagon/...`, `src/...`, gleiche Dateinamen). Jede Abweichung ist mit `VectorPie` kommentiert. Neu geschrieben, mit denselben Schnittstellen:
  - `Global/Config.*`: nur die Getter, die der Kern braucht, mit den Standardwerten des Originals.
  - `Global/Assets.*`: die Ladefunktionen des Originals (`loadPackData`, `loadPackAssets_*`) unverändert übernommen, ohne Texturen, Schriften, Shader, Profile. Bestzeiten in `save/scores.txt`.
  - `Global/Audio.*`: über `vp_audio`, Musik mit Segment-Start (dekodiert mit dem stb_vorbis des SDK).
  - `Core/HGGraphics.cpp`: `draw()` füllt die Puffer wie das Original und merkt sich die 3D-Ebenen in `vpLayers3D`.
  - `Core/HGVectorPie.cpp`: was GameState und die Eingabe-Callbacks taten (`tick`, `setInputs`, `requestRestart` …), dazu `vpDegPerTick` und der Demo-Autopilot.
  - `Core/Steam.hpp`, `Discord.hpp`, `HexagonClient.hpp`: leere Platzhalter, die Zeiger sind immer `nullptr`.
- `src/compat/`: kleine Nachbauten von SFML-Typen (`Vector2`, `Color`, `Vertex`, `View`, `Sprite`, `Text`, `Shader`) und SSVStart (`Camera`, `GameWindow`, `Vector2.hpp` als Kopie).
- `src/vp/`: VectorPie-Seite.
  - `main.cpp`: `vp_run`, 60 Hz Update, Bildrate aus den Einstellungen.
  - `app.cpp`: Zustände Home-Menü, Home-Spiel, Titel, Warnung, Bestenliste, Demo, Levelwahl, Spiel, Continue, Game Over, Initialen, Fehler. Arcade-Punkte: `A.best`, der beste einzelne Versuch über alle Continues eines Credits. Live-Vorschau der Levelwahl (`updatePreview`). Selbsttest `OH_SELFTEST`. Läufe des Piloten immer mit `startLevel(..., true)`, sonst speichert er Bestzeiten.
  - `render.cpp`: Puffer des Spiels zu Linien, Text, Partikel, Menü-Hintergrund, `ui::`-Helfer (384 x 288, y nach unten).
  - `occlude.cpp`: verdeckte Linien in der Ebene (Ebenen, Vereinigung gleicher Ebene, Raster, Sortierung der Stücke).
  - `controls.cpp`: Tasten und Spinner nach advmame.rc-Namen; Spinner als Schritte mit Originalgeschwindigkeit.
  - `service.cpp`: Service-Menü, nach `src/service.c` des Tempest-2000-Ports.
  - `settings.cpp`: `save/settings.ini`.
  - `synth.cpp`: der Synthesizer (Kick, Snare, Clap, Hats, Crash, Bass mit Wobble oder Hüllkurve, Pad, Lead mit Delay, Riser, Sidechain, Limiter), ein Stück nach `TrackSpec`. Hüllkurven multiplizieren statt `exp` pro Sample; nur so ist er schnell genug für den Pi. Datei mit `-O3 -ffast-math`.
  - `music.cpp`: Tempo und Phase aus dem Beat-Pulse (`beatSync`), Zwischenspeicher mit Thread (`musicFor`), Startpunkt (`musicStartFrame`). Angebunden über `HexagonGame::vpUseMadeMusic` und `vpOnStart` (in `start()`, vor `resumeMusic`) und `Audio::playGenerated`.
  - `hints.cpp`: Textfilter für die Steuerungshinweise der Level (`HexagonGame::vpTextFilter`) und Zusatzskripte `assets/VectorPie/<level>.lua` (`vpAfterLevelScript`, mit der Lua-Variablen `vp_cabinet`; zurzeit keine Datei). Eine neue Meldung mit Tastennamen in einem Paket muss dort eingetragen werden, sonst erscheint der PC-Text.
  - Lange Texte mit `ui::wrap` (render.cpp) umbrechen, nicht abschneiden; Hinweise im Service-Menü über die `hints`-Liste in `serviceDraw`.
- `assets/`: `Packs/` und `Assets/` (Sounds) aus `_RELEASE` des Originals. Das Spiel wechselt beim Start in diesen Ordner, wie das Original in seinen Spielordner.
- `package/`: `game.ini`, `leds.cfg`, `run.sh`. `tools/make_vpk.sh` baut das Paket, `tools/snap.ps1` macht Bildschirmfotos.

## Konventionen

- README, STATUS, RESUME, BUILD auf Deutsch, Kommentare im Code auf Englisch.
- Am Kern in `src/oh` nur so wenig wie nötig ändern, jede Änderung mit `VectorPie` markieren. Neue Funktionen nicht in die Originaldateien schreiben, sondern in `HGVectorPie.cpp` oder `src/vp`.
- Spielwerte nicht schätzen: Sie kommen aus dem Original (Config-Standardwerte, Level-JSON, Lua). Was erfunden ist (Arcade, Darstellung als Linien), steht in STATUS.md unter den Einschränkungen.
- Linien nie pro Bild umsortieren (etwa nach kürzestem Sprungweg): Eine wechselnde Reihenfolge lässt das Bild auf der Röhre wackeln. Die Reihenfolge muss von Bild zu Bild gleich bleiben.
- Eingaben nur über advmame.rc-Namen. START 1 + START 2 zusammen ist am Automaten Exit und darf nichts auslösen. Das Service-Menü öffnet nur F2.
- Nach Änderungen am Kern den Selbsttest laufen lassen (BUILD.md).
- Sparstufen (`_lod` in `render.cpp`) dürfen die Füllstriche der Wände nie wegnehmen: Ohne sie ist eine breite Wand ein leerer Umriss und wird nicht als Gefahr erkannt.
- PC-Build aus der PowerShell nur mit `C:/msys64/mingw64/bin` vorne im PATH, sonst bricht g++ ohne Meldung ab.
- Python-Skripte mit `\n` in Strings nicht als Heredoc über die Bash schicken: Die Bash dieses Rechners macht aus `\\n` einen echten Zeilenumbruch. Skripte in eine Datei schreiben und dann ausführen.

## Wegweiser im Original

- `src/SSVOpenHexagon/Core/HGUpdate.cpp`: `update`, Pulse, Beat-Pulse, Rotation, Wände, Eingabe.
- `Core/HGGraphics.cpp`: Zeichenreihenfolge und 3D-Ebenen (Vorlage für `render.cpp`).
- `Core/HGScripting.cpp` und `Core/LuaScripting.cpp`: die Lua-API.
- `Core/MenuGame.cpp`: das Menü des Originals (nicht portiert).
- `Global/Config.cpp`: Standardwerte (`X_LINKEDVALUES`).

## Nächste Schritte

1. Rückmeldung vom Automaten: Pointless (Wände mit Füllstrichen), Continue-Ablauf, Warnbildschirm, Arcadia verschlankt (`LOOKS` in `render.cpp`: Balken als eine Linie, kein 3D, keine Strahlen; flimmert es noch?). Weitere zu volle Level dort eintragen statt Sparstufen..
2. Offene Aufgaben aus STATUS.md.
