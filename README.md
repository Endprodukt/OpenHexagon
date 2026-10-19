# Open Hexagon für VectorPie

Open Hexagon von Vittorio Romeo ([github.com/vittorioromeo/SSVOpenHexagon](https://github.com/vittorioromeo/SSVOpenHexagon)) als Vektorspiel für den VectorPie-Automaten: Raspberry Pi 5, USB-DVG, Wells-Gardner 6100.

- Der Spielkern ist der Code des Originals: Spieler, Wände, Custom Walls, Timelines, Styles, Lua-API, Replays. Er liegt in `src/oh` und läuft mit 240 Takten pro Sekunde wie das Original.
- Die Level-Pakete des Originals (`cube`, `hypercube`, `orthoplex`, `tutorial`, dazu `base`) laufen unverändert unter Lua 5.1.
- Die Grafik ist neu, in `src/vp`: Die gefüllten Flächen des Originals werden zu Umrisslinien. Der 3D-Effekt wird als Körper mit verdeckten Kanten gezeichnet.
- Zwei Modi, im Service-Menü (F2) umschaltbar:
  - **Home** wie im Original: Level und Schwierigkeit wählen, Bestzeiten, Neustart, Replay.
  - **Arcade**: Münzen, Attract-Schleife (Titel, Epilepsie-Warnung, Bestenliste, Demo), Levelwahl auf Zeit, mehrere Leben pro Credit (der beste Versuch zählt), Continue mit Countdown, Bestenliste pro Level, Initialen.
- Steuerung mit Spinner, Tasten oder Joystick, alles parallel und im Service-Menü belegbar.

Bauen: [BUILD.md](BUILD.md). Stand und offene Punkte: [STATUS.md](STATUS.md). Weiterarbeiten: [RESUME.md](RESUME.md).

## Lizenzen

- Code von Open Hexagon, SSVUtils, SSVStart: Academic Free License 3.0 (`src/oh/LICENSE`, `vendor/SSVUtils/LICENSE`, `src/compat/SSVStart/LICENSE`).
- Soundeffekte: `assets/Assets/Open Hexagon Assets License.txt`. Sie dürfen nur mit Open Hexagon verwendet werden.
- Die Musik des Originals ist nicht im GitHub-Quelltext enthalten. Ab Werk spielt das Spiel einen selbst erzeugten Soundtrack (src/vp/synth.cpp, frei). Die `.ogg`-Dateien der Steam-Version können wahlweise in den Ordner `save/music` gelegt werden (siehe BUILD.md).
- Lua 5.1.5: MIT-Lizenz (`vendor/lua-5.1.5/COPYRIGHT`).
