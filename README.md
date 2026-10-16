# Foundation on Apple Silicon Macs (CrossOver + patched Mesa)

An unofficial way to run **Foundation** (Polymorph Games, Steam app 690830) on an Apple Silicon Mac through **CrossOver**, at full speed on the GPU.

Tested on an M4 Max with CrossOver 26.3 and Foundation 1.12.0.15: menus, UI, terrain, buildings and villagers all render, at 60 FPS with VSync on (100+ FPS without).

> Not affiliated with or supported by Polymorph Games, CodeWeavers or Apple. Use at your own risk. Back up your saves.

## Why Foundation doesn't run in CrossOver out of the box

Foundation's engine has a single renderer, written in **OpenGL 4.3**. macOS stopped updating OpenGL at **4.1**, and CrossOver passes OpenGL calls straight to macOS. The game asks for 4.3, gets 4.1, and quits with *"OpenGL version 4.3 is required"*.

Most Windows games use DirectX instead, which CrossOver translates well (D3DMetal, DXVK, DXMT). That's why big DirectX games run while Foundation doesn't.

## How this works

The game folder gets **Mesa's OpenGL-on-Direct3D 12 driver**. CrossOver then runs that Direct3D 12 on Apple's **D3DMetal**:

```
Foundation (OpenGL 4.3) → Mesa d3d12 driver → Direct3D 12 → D3DMetal → Metal
```

Two D3DMetal problems made parts of the game invisible on that path, and a third made the window blank. This repo's Mesa build works around all three:

| Symptom in game | Cause | Workaround in this build |
|---|---|---|
| No text, no 3D world, "frozen" menu | Shaders with `flat` inputs draw nothing while OpenGL's default *last-vertex* provoking convention is active (Mesa's load-at-vertex path) | Default provoking vertex changed to *first vertex* |
| No ground (sky shows through, no paths, villagers float) | `glMultiDrawElementsIndirect` with tessellation patches draws nothing once a single call holds roughly 30 or more commands | Such calls are split into chunks of 16 (`D3D12_PATCH_MDI_CHUNK`) |
| Blank white window (or, with Mesa's `wgl_require_gdi_compat` workaround, screen tearing) | For pixel formats with alpha, Mesa presents through DirectComposition, which doesn't work under Wine/CrossOver. The GDI-compatible workaround copies frames into the window without VSync. | Always present through a plain window swap chain (opt back in with `D3D12_SWAPCHAIN_ALPHA=1`), so VSync works and there's no tearing |

The build also raises Mesa's texture upload budget to 512 MB.

`tests/repro.c` is a small standalone OpenGL program that reproduces the first two problems.

## Requirements

- An Apple Silicon Mac with **CrossOver** (paid software from CodeWeavers).
- A CrossOver bottle with **Steam** and **Foundation** installed. A bottle used only for Foundation is best, because the install changes bottle-wide settings.
- **dxil.dll** from Microsoft's DirectX Shader Compiler: download the latest `dxc_*.zip` from https://github.com/microsoft/DirectXShaderCompiler/releases and take `bin/x64/dxil.dll`. It isn't included here.

## Install

1. Quit Foundation and Steam in the bottle.
2. Make a `dlls/` folder in this repo and put three files in it:
   - `libgallium_wgl.dll` and `opengl32.dll` from this repo's **Releases** page (check them against `SHA256SUMS.txt`), or from your own build (see below)
   - `dxil.dll` (see Requirements)
3. Run:
   ```bash
   scripts/install.sh "My Foundation Bottle"
   ```
4. Open Steam in the bottle and press **Play** on Foundation as usual.

   `scripts/play.sh "My Foundation Bottle"` starts the game directly instead (Steam must still be running). It can help if a Steam update ever breaks the Play button.

**In-game tip:** set *Settings → Graphics → Antialiasing* to **FXAA**. TAA makes the 3D scene look soft on this setup.

To undo everything: `scripts/uninstall.sh "My Foundation Bottle"`.

## Build Mesa yourself

```bash
brew install mingw-w64 ninja bison flex
PYTHON=python3.12 build/build-mesa.sh   # any Python >= 3.10
```

This downloads Mesa 26.2.4 from archive.mesa3d.org, checks its SHA-256, applies `patches/mesa-26.2.4-d3dmetal-fixes.patch`, builds only the d3d12 driver, and writes the DLLs to `build/work/out/`. Meson fetches Microsoft's DirectX-Headers automatically.

The DLLs need `libwinpthread-1.dll`. Foundation already ships one in its folder.

## Known limitations

- The provoking-vertex change only alters the *default*. An app that explicitly requests last-vertex mode would still hit the D3DMetal problem. Foundation doesn't.
- The custom build includes only Mesa's d3d12 driver, not the llvmpipe CPU renderer.
- Game, CrossOver or macOS updates may change things.

## Licenses

- Mesa is MIT-licensed. See `LICENSE-MESA.rst`. The release DLLs are built from the official Mesa 26.2.4 source plus `patches/`.
- Scripts, tests and documentation in this repo: MIT, see `LICENSE`.
- Foundation, CrossOver and D3DMetal belong to their respective owners. None of their files are included.
