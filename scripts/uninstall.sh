#!/bin/zsh
# Remove the patched Mesa from Foundation's folder and restore the bottle settings.
#   uninstall.sh "<bottle name>"
set -e

BOTTLE=${1:?usage: uninstall.sh "<bottle name>"}
B="$HOME/Library/Application Support/CrossOver/Bottles/$BOTTLE"
G="$B/drive_c/Program Files (x86)/Steam/steamapps/common/Foundation"
WINE=/Applications/CrossOver.app/Contents/SharedSupport/CrossOver/bin/wine

if pgrep -f foundation.exe >/dev/null; then echo "Quit Foundation first"; exit 1; fi

rm -f "$G/libgallium_wgl.dll" "$G/opengl32.dll" "$G/dxil.dll" "$G/steam_appid.txt"
if [[ -f "$B/cxbottle.conf.before-foundation-fix" ]]; then
  mv "$B/cxbottle.conf.before-foundation-fix" "$B/cxbottle.conf"
fi
"$WINE" --bottle "$BOTTLE" reg delete 'HKCU\Software\Wine\AppDefaults\foundation.exe\DllOverrides' /v opengl32 /f >/dev/null 2>&1 || true

echo "Removed. If Steam reports missing files, use Steam's 'Verify integrity of game files'."
