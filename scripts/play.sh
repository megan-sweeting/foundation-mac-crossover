#!/bin/zsh
# Start Foundation directly, without Steam's Play button (Steam must still be running).
#   play.sh "<bottle name>"
# Steam must already be running (and signed in) in the same bottle.
BOTTLE=${1:?usage: play.sh "<bottle name>"}
B="$HOME/Library/Application Support/CrossOver/Bottles/$BOTTLE"
G="$B/drive_c/Program Files (x86)/Steam/steamapps/common/Foundation"
WINE=/Applications/CrossOver.app/Contents/SharedSupport/CrossOver/bin/wine

# steam_appid.txt lets the game start without relaunching through Steam; it's removed again after startup.
printf 690830 > "$G/steam_appid.txt"
cd "$G"
"$WINE" --bottle "$BOTTLE" --workdir "$G" 'C:\Program Files (x86)\Steam\steamapps\common\Foundation\foundation.exe' >/dev/null 2>&1 &
sleep 20
rm -f "$G/steam_appid.txt"
echo "Foundation started."
