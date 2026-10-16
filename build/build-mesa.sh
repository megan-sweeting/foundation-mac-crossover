#!/bin/zsh
# Build Mesa 26.2.4 (d3d12 gallium driver only) for 64-bit Windows on macOS, with the D3DMetal fixes applied.
# Needs: Homebrew mingw-w64, ninja, bison, flex, and Python >= 3.10.
# Output: out/libgallium_wgl.dll and out/opengl32.dll
set -e

HERE=${0:A:h}
REPO=${HERE:h}
WORK=${WORK:-$HERE/work}
PYTHON=${PYTHON:-python3}
MESA_VERSION=26.2.4
MESA_SHA256=bce5f7fbebb934373b86c999a064d52fb5065878dc57f287f95346648ec832e9

mkdir -p "$WORK"
cd "$WORK"

if [[ ! -d venv ]]; then
  "$PYTHON" -m venv venv
  ./venv/bin/pip install -q meson mako pyyaml packaging
fi
export PATH="$(brew --prefix bison)/bin:$(brew --prefix flex)/bin:$WORK/venv/bin:$PATH"

if [[ ! -d mesa-$MESA_VERSION ]]; then
  curl -fL -o mesa-$MESA_VERSION.tar.xz https://archive.mesa3d.org/mesa-$MESA_VERSION.tar.xz
  echo "$MESA_SHA256  mesa-$MESA_VERSION.tar.xz" | shasum -a 256 -c -
  tar -xf mesa-$MESA_VERSION.tar.xz
  (cd mesa-$MESA_VERSION && patch -p1 < "$REPO/patches/mesa-$MESA_VERSION-d3dmetal-fixes.patch")
fi

cd mesa-$MESA_VERSION
meson setup ../build --cross-file "$HERE/mingw64-cross.txt" --buildtype=release \
  -Dgallium-drivers=d3d12 -Dvulkan-drivers= -Dllvm=disabled -Dplatforms=windows \
  -Dgles1=disabled -Dgles2=disabled -Dmicrosoft-clc=disabled -Dspirv-to-dxil=false \
  -Dxmlconfig=disabled -Dzlib=disabled -Dzstd=disabled -Dvideo-codecs= \
  -Dgallium-va=disabled -Dgallium-mediafoundation=disabled -Dbuild-tests=false
ninja -C ../build

mkdir -p "$WORK/out"
x86_64-w64-mingw32-strip -o "$WORK/out/libgallium_wgl.dll" ../build/src/gallium/targets/wgl/libgallium_wgl.dll
x86_64-w64-mingw32-strip -o "$WORK/out/opengl32.dll" ../build/src/gallium/targets/libgl-gdi/opengl32.dll
echo "Built: $WORK/out"
