#!/bin/zsh
# Install the patched Mesa into Foundation's folder inside a CrossOver bottle.
#   install.sh "<bottle name>" [folder with libgallium_wgl.dll, opengl32.dll, dxil.dll; default: dlls/]
# Quit Foundation and Steam in that bottle first. Undo with uninstall.sh.
set -e

BOTTLE=${1:?usage: install.sh "<bottle name>" [dll folder]}
DLLS=${2:-${0:A:h:h}/dlls}
B="$HOME/Library/Application Support/CrossOver/Bottles/$BOTTLE"
G="$B/drive_c/Program Files (x86)/Steam/steamapps/common/Foundation"
WINE=/Applications/CrossOver.app/Contents/SharedSupport/CrossOver/bin/wine

[[ -f "$B/cxbottle.conf" ]] || { echo "No CrossOver bottle named '$BOTTLE'"; exit 1; }
[[ -f "$G/foundation.exe" ]] || { echo "Foundation isn't installed in that bottle's Steam"; exit 1; }
for f in libgallium_wgl.dll opengl32.dll dxil.dll; do
  [[ -f "$DLLS/$f" ]] || { echo "Missing $DLLS/$f (see README for dxil.dll)"; exit 1; }
done
if pgrep -f foundation.exe >/dev/null; then echo "Quit Foundation first"; exit 1; fi

# Back up the bottle settings once, so uninstall.sh can restore them exactly.
[[ -f "$B/cxbottle.conf.before-foundation-fix" ]] || cp "$B/cxbottle.conf" "$B/cxbottle.conf.before-foundation-fix"

cp "$DLLS/libgallium_wgl.dll" "$DLLS/opengl32.dll" "$DLLS/dxil.dll" "$G/"

# Bottle environment: use Mesa's D3D12 driver (which CrossOver runs on D3DMetal) and expose OpenGL 4.3.
python3 - "$B/cxbottle.conf" <<'EOF'
import re, sys
path = sys.argv[1]
text = open(path).read()
wanted = {
    "GALLIUM_DRIVER": "d3d12",
    "MESA_GL_VERSION_OVERRIDE": "4.3COMPAT",
    "MESA_GLSL_VERSION_OVERRIDE": "430",
}
# Older versions of this fix used GDI-compatible pixel formats, which copy frames into the
# window without VSync and cause tearing. The patched Mesa presents through DXGI instead.
text = re.sub(r'^"wgl_require_gdi_compat" = ".*"\n', "", text, flags=re.M)
for key, value in wanted.items():
    line = f'"{key}" = "{value}"'
    pattern = re.compile(rf'^"{re.escape(key)}" = ".*"$', re.M)
    if pattern.search(text):
        text = pattern.sub(line, text)
    else:
        text = re.sub(r'^\[EnvironmentVariables\]\n', lambda m: m.group(0) + line + "\n", text, count=1, flags=re.M)
open(path, "w").write(text)
EOF

# Make foundation.exe load the opengl32.dll in its own folder instead of Wine's built-in one.
"$WINE" --bottle "$BOTTLE" reg add 'HKCU\Software\Wine\AppDefaults\foundation.exe\DllOverrides' \
  /v opengl32 /d native,builtin /f >/dev/null

echo "Installed. Start Foundation from Steam in the bottle as usual (or with scripts/play.sh \"$BOTTLE\")."
