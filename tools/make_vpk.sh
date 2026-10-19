#!/bin/sh
# make_vpk.sh - the game as a VectorPie package: build/openhexagon-<version>.vpk
#
# Run from the project folder after the Pi build (BUILD.md). The package holds
# the Pi program, game.ini, leds.cfg and assets/ (the level packs and the
# sounds). The cabinet's menu installs it from a USB drive's games/ folder or
# from /persistent/games/packages/ (Settings -> GAMES -> INSTALL GAMES).
# save/ (settings, best times, tables, replays, music) is kept on an update.
#
#   sh tools/make_vpk.sh            build/openhexagon-<version>.vpk
#   sh tools/make_vpk.sh personal   build/openhexagon-<version>-personal.vpk,
#                                   with the Steam version's music (tools/
#                                   steam_music.sh): for the owner's cabinet
#                                   only, never to be given away
set -eu
edition=${1:-full}
root=$(cd "$(dirname "$0")/.." && pwd)
build="$root/build/pi"
stage="$root/build/pkg/openhexagon"
[ -f "$build/openhexagon" ] || { echo "make_vpk: no Pi build in $build" >&2; exit 1; }
rm -rf "$(dirname "$stage")"
mkdir -p "$stage"
cp "$build/openhexagon" "$root/package/game.ini" "$root/package/leds.cfg" "$stage/"
cp -r "$root/assets" "$stage/assets"
if [ "$edition" = personal ]; then
    sh "$root/tools/steam_music.sh" "$stage/assets"
fi
[ -f "$root/package/marquee.png" ] && cp "$root/package/marquee.png" "$stage/"
version=$(sed -n 's/^version[[:space:]]*=[[:space:]]*\([0-9.]*\).*/\1/p' "$stage/game.ini" | head -n 1)
out=$(mktemp -d)
sh "$root/vendor/VectorPie/sdk/tools/vp-pack/vp-pack.sh" "$stage" "$out"
vpk="$root/build/openhexagon-$version.vpk"
[ "$edition" = personal ] && vpk="$root/build/openhexagon-$version-personal.vpk"
mv -f "$out/openhexagon-$version.vpk" "$vpk"
rmdir "$out"

# Built on Windows the program in the archive is not marked as one: the file
# system there has no such mark for vp-pack's chmod to set. Mark it here.
python - "$vpk" <<'PY'
import io, sys, tarfile
path = sys.argv[1]
out = io.BytesIO()
with tarfile.open(path, "r:gz") as old, tarfile.open(fileobj=out, mode="w:gz") as new:
    for member in old.getmembers():
        if member.name == "openhexagon/openhexagon":
            member.mode = 0o755
        new.addfile(member, old.extractfile(member) if member.isfile() else None)
open(path, "wb").write(out.getvalue())
PY
echo "make_vpk: $vpk"
