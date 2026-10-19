#!/bin/sh
# steam_music.sh <assets folder> - the Steam version's music and the sounds
# the GitHub release lacks, copied into an assets folder (the package's or
# build/pc/assets). For the owner's own cabinet only: the music is licensed
# to the Steam version and must not go into anything given away.
#
# OH_STEAM_DIR: the Steam game folder, else the default one.
set -eu
dest=${1:?usage: steam_music.sh <assets folder>}
steam=${OH_STEAM_DIR:-"/c/Program Files (x86)/Steam/steamapps/common/Open Hexagon"}
[ -d "$steam/Packs" ] || { echo "steam_music: no Open Hexagon in $steam" >&2; exit 1; }
n=0
for pack in cube hypercube orthoplex tutorial; do
    for dir in Music Sounds; do
        [ -d "$steam/Packs/$pack/$dir" ] || continue
        mkdir -p "$dest/Packs/$pack/$dir"
        for f in "$steam/Packs/$pack/$dir"/*.ogg; do
            [ -f "$f" ] || continue
            cp "$f" "$dest/Packs/$pack/$dir/"
            n=$((n + 1))
        done
    done
done
for f in blip.ogg swapBlip.ogg; do
    [ -f "$steam/Assets/$f" ] && cp "$steam/Assets/$f" "$dest/Assets/" && n=$((n + 1))
done
echo "steam_music: $n files into $dest"
