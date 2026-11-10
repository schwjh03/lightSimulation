# Light Lab 2D

A native C++17 optics sandbox for Windows and Linux. Aim a laser, draw mirrors and walls,
and place glass prisms to refract and disperse light. Includes a dark optical
bench, metal laser housings, glowing beams, glass faces, and editable hardware.
Measure spectra with detectors, compare materials, and divide light with beam splitters.
Uses the bundled raylib 5.5 library.

## Start

This repository contains source code. Follow **Build and Test** below to build
and launch it on 64-bit Windows, or use the **Native Linux** instructions below.
Generated executables are not committed.
After building, double-click `LightLab2D.exe` in this folder to run it again.
The build script also writes a copy to `build/LightLab2D.exe`; the built app
needs no separate raylib installation or downloaded runtime DLLs.
The **Scene** group in the left panel contains **Room**, **Relay**, **Blank**,
**Prism demo**, and **Splitter lab** together. Prism demo includes a spectrum
detector; Splitter lab includes a 50/50 splitter and two detectors.
Current work is preserved before switching scenes, and Undo can restore it.

## Saving and reopening scenes

- **Save / Ctrl+S:** save the active named scene. The first save asks for a name.
- **Save as / Ctrl+Shift+S:** create a separate scene file or choose a new location.
- **Open / Ctrl+O:** browse saved `.lightscene` files and reopen one later.
- The default folder is `%LOCALAPPDATA%\LightLab2D\scenes`. Each scene has its own
  file. The active name appears in the title/status bar; `*` indicates unsaved edits.
- Before a starter or saved scene replaces the workspace, edited unnamed work is
  automatically saved with a unique timestamped name. Changes to a named scene are
  saved back to that scene. If preserving the work fails, switching is cancelled.
- Normal exit preserves current work and restores the active scene next time.
- The old single-file save is imported as **Recovered scene** on the first start.
  `scene.txt` remains a recovery file, separate from your named scene collection.
- Cancelling Open or Save As leaves the workspace intact. Named saves use an
  atomic file replacement, keeping the previous file if replacement fails.

## Controls

- **Select:** drag the light, a surface, or either endpoint of a surface.
- **Add laser (L):** choose the placement tool, then click the canvas. Drag while
  placing to set the initial direction. Add up to 16 independent lasers.
- The sidebar lists your lasers and their colors. Click a laser on the canvas or
  in this list to open its own movable inspector. Scroll the list if it is long.
- Each laser has its own color, direction, spread, ray count, reflection limit,
  Laser/Point mode, and sweep setting. You can delete individual lasers and Undo
  placement, edits, or deletion. A scene can also contain no lasers.
- Clicking any object opens its own movable desktop inspector: laser, prism,
  splitter, mirror, wall, or detector. Drag its title bar outside the app or onto
  another monitor. Several inspectors can stay open while you edit the workspace.
  Closing an inspector keeps the object; click it again or use the selection
  area's **Open** button to reopen it. Changes from inspector controls support Undo.
- Drag the small dot in front of a beam source to aim it.
- **Mirror / Wall:** drag on the canvas to add a reflective or absorbing segment.
- **Prism (P):** click to place a triangular glass prism, or drag from its center
  to one corner to choose its size and orientation. Placement returns to Select.
- Click a prism and drag its body to move it. Drag any of its three corner handles
  to resize and rotate it. Delete, Undo, and Save all work with prisms.
- Select a prism to open its object inspector: change its **apex angle**, rotation,
  size, and material (**flint**, **crown**, or an illustrative **water model**).
- **Splitter (S):** draw a segment, then select it to adjust reflected/transmitted
  power with a slider or the 25%, 50%, and 75% presets. Its endpoints and body can
  be dragged like a mirror. The ideal splitter divides power without absorption.
- **Detector (D):** draw a receiving segment. It absorbs incoming light from both
  sides. Its badge reports received power as a percentage of the source. Select
  it to open a separate native Windows window with the live spectrum, relative
  band heights, and peak or balanced bands. Drag its title bar anywhere on the
  desktop, including outside the main application or onto another monitor, and
  resize it using the window borders. Multiple detectors can stay open at once
  while you edit other objects. Closing a readings window leaves its detector
  intact; click that detector again or use **Open** in the selection area to
  reopen it. Window positions/sizes are remembered for the current app session.
  Hover a spectrum bar to read that band's wavelength and share of source power.
- **Glass reflections:** in a prism's inspector, toggle partial Fresnel reflections
  at all glass interfaces. Total internal reflection still works when disabled.
- **Grid:** show or hide the animated dot grid. The dots always move while the grid is visible.
- **Laser color:** choose among yellow, red, green, cyan, purple, white, and blue
  in that laser's inspector. The color label identifies the chosen spectrum.
- **Prism demo:** a narrow white beam, a glass prism, and a detector, ready to experiment with.
- **Room / Relay / Blank / Prism demo / Splitter lab:** choose a starter scene.
- **Laser / Point:** in a laser's inspector, switch between a beam and light emitted in all directions.
  Zero spread gives a sharp laser; increasing spread produces a fan of rays.
- Adjust direction, spread, ray count, reflections, color, and sweep in each laser's inspector.
- **Sweep** animates only that laser's direction. **Surface normals** shows sample hit normals.
- Select any object, then use the trash icon or Delete to remove it.
- Use the labeled **Open**, **Save**, and **Save as** buttons in the top toolbar.

| Key | Action |
| --- | --- |
| V / M / B / P / S / D | Select / Mirror / Wall / Prism / Splitter / Detector |
| L | Add laser |
| Ctrl+Z | Undo, up to 40 edits |
| Ctrl+S | Save |
| Ctrl+Shift+S | Save as a separate scene |
| Ctrl+O | Open a saved scene |
| Delete / Backspace | Delete selected object, including a laser |
| G / N | Toggle grid / surface normals |
| Space | Toggle the selected laser's sweep |
| Escape | Cancel drawing and return to Select |
| F11 | Toggle borderless fullscreen |

Named scenes default to `%LOCALAPPDATA%\LightLab2D\scenes`, not the program folder.
You can also choose another folder in the file dialog. Recovery/session information
lives in `%LOCALAPPDATA%\LightLab2D`.
The scene supports 16 lasers, 64 segments (mirrors, walls, splitters, or detectors),
16 prisms, and up to 240 initial ray directions / 12 reflections per laser. Version 4
saves preserve all lasers and their independent settings. Existing version 1, 2,
and 3 scenes still load with their original single laser. Tests and screenshot sessions do not
change your saved scene.

## Build and Test

The current app targets **64-bit Windows** and uses native Windows inspector
windows. Build it with the **MSYS2 UCRT64** C++17 compiler and GNU Make.
The raylib headers, license, and Windows x64 static library are included in
`vendor`; SFML is no longer required.

1. Install [Git for Windows](https://git-scm.com/downloads/win) and
   [MSYS2](https://www.msys2.org/) if needed.
2. Open the **MSYS2 UCRT64** terminal and install the tools:

   ```sh
   pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-make
   ```

3. In PowerShell, clone the `test` branch and start the app:

   ```powershell
   git clone --branch test https://github.com/schwjh03/lightSimulation.git
   cd lightSimulation
   powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\make.ps1
   ```

The wrapper and build script find tools on PATH, then try the standard MSYS2
location `C:\msys64\ucrt64\bin`. For a custom MSYS2 location, add its `ucrt64\bin`
folder to PATH first. The execution policy option above applies only to that
PowerShell process. No permanent policy change or administrator rights are needed
to build. Package details: [GCC](https://packages.msys2.org/packages/mingw-w64-ucrt-x86_64-gcc)
and [GNU Make](https://packages.msys2.org/packages/mingw-w64-ucrt-x86_64-make).

With the tool folder on PATH, you can also run GNU Make directly from this folder.
MSYS2 calls it `mingw32-make`; installations named `make` use the same targets.

```powershell
mingw32-make                 # Build and launch
mingw32-make build           # Build only
mingw32-make test            # Build and run all checks
mingw32-make ARGS=--prism    # Build and launch the prism demo
```

### Native Linux

Install a C++17 compiler, Make, CMake, curl, pkg-config, OpenGL and X11 development
packages, plus Zenity for Open / Save As dialogs. On Debian/Ubuntu:

```sh
sudo apt install build-essential cmake curl pkg-config libgl1-mesa-dev libx11-dev libxrandr-dev libxi-dev libxcursor-dev libxinerama-dev zenity
make build                 # Build ./LightLab2D
make                       # Build and launch natively
make ARGS=--prism          # Launch the prism demo
make test-core             # Optics and scene tests; no display required
make test                  # Also run the rendered UI checks; requires a desktop
```

The Makefile uses an installed raylib from `pkg-config` when available. Otherwise,
it downloads raylib 5.5 on the first build and builds a native static library under
`build/`; this first build needs internet access. The bundled Windows archive is
only used on Windows. Compiler and linker flags can be overridden as usual.
Linux inspectors use independent X11 windows with clickable controls and live
detector spectra. An X11 desktop or XWayland is required. The Win32-specific
window checks run only on Windows; Linux runs X11 inspector lifecycle checks
alongside the shared UI checks.
Scenes and session data live under `$XDG_DATA_HOME/LightLab2D`, defaulting to
`~/.local/share/LightLab2D`. Open and Save As launch Zenity; cancelling keeps the
current scene intact.

On Windows, the Makefile uses the build script below. Close the app before rebuilding its
executable; the test target leaves the published executable alone so it can stay open.
You can also call the script directly:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Test
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Test -NoPublish
```

The test command checks optical geometry and save/load, then drives the actual
rendered UI with simulated pointer input. It writes `build/ui-checks.txt`.
The tests cover Snell's law, wavelength-dependent bending, total internal
reflection, spectral mixtures, mirror/wall interactions, old and new save files,
and prism placement, moving, sizing, rotation, deletion, and undo through the UI.
They also check detector power normalization, cascaded splitter conservation,
Fresnel reflections, material/apex controls, and animated-grid visibility.
Native object window checks cover desktop movement, negative monitor coordinates,
resizing, live updates, close/reopen, multiple windows, deletion, Undo, and editing
all object types. Named-scene tests cover independent files, auto-save on switching,
Save As copies, reopening, migration, invalid files, and restoring save destinations.
Their report is written to `build/detector-window-checks.txt`.

For reproducible previews without loading or replacing your saved scene:

```powershell
.\LightLab2D.exe --prism --capture build/prism.png
.\LightLab2D.exe --prism --color 4 --capture build/purple.png
.\LightLab2D.exe --prism --small --capture build/small-prism.png
.\LightLab2D.exe --prism --inspect-detector --capture build/detector.png
.\LightLab2D.exe --splitter --inspect-splitter --capture build/splitter.png
.\LightLab2D.exe --multi --inspect-laser --capture build/multiple-lasers.png
```

`--color` accepts the swatch index (0 yellow, 1 red, 2 green, 3 cyan, 4 purple,
5 white, 6 blue). `--prism` starts an isolated demo session; close it without
changing the normal saved scene.
`--splitter` selects the splitter lab. `--inspect-prism`, `--inspect-detector`,
and `--inspect-splitter` open the corresponding object inspector for previews.
`--multi` creates an isolated three-laser preview; `--inspect-laser` opens the first laser's inspector.
Inspector captures also write a `.object.bmp` companion image for the separate window.
`--background-time 10` starts the moving dots ten seconds into their animation.

## Source Map

| File | Responsibility |
| --- | --- |
| `src/main.cpp` | Entry point; hands control to the window runner. |
| `src/runtime.cpp` | Opens the window, scales mouse coordinates, runs frames, and cleans up. |
| `src/scene.hpp` | Plain data types for the light, surfaces, prisms, presets, and scene. |
| `src/scene.cpp` | Creates starter scenes and validates scene files when loading. |
| `src/optics.hpp` | Describes a traced ray segment and the tracing API. |
| `src/optics.cpp` | Wavelength spectra, glass dispersion, Snell refraction, and nearest-hit tracing. |
| `src/app.hpp` | Declares application state and editing operations. |
| `src/app.cpp` | Handles dragging, drawing, keyboard input, undo, and saving. |
| `src/ui.hpp` / `src/ui.cpp` | Small reusable buttons, sliders, checkboxes, fonts, and tooltips. |
| `src/renderer.cpp` | Draws the toolbar, inspector, optical canvas, and status bar. |
| `src/detector_windows.cpp` | Independent native inspectors for every object type, editable controls, and detector spectra. |
| `src/scene_library.cpp` | Named scene files, protected scene switching, recovery migration, and save contexts. |
| `src/scene_dialogs.cpp` | Native Open / Save As file dialogs. |
| `src/native_windows.hpp` | Isolates Windows declarations from raylib's API names. |
| `tests/optics_tests.cpp` | Deterministic reflection and scene-file tests. |
| `tests/ui_checks.hpp` / `tests/ui_checks.cpp` | Scripted interaction checks through the UI. |
| `tests/detector_window_checks.cpp` | Tests actual desktop detector windows and their lifecycle. |
| `tests/scene_library_tests.cpp` | Tests named saves, reopening, migration, recovery protection, and scene switching. |
| `build.ps1` | Compiles and links the program; optionally runs the tests. |
| `Makefile` | Builds and launches by default; also provides build and test targets. |
| `make.ps1` | Finds GNU Make on PATH or in the standard MSYS2 installation. |

## How a Frame Works

1. `runtime.cpp` reads the mouse and converts window coordinates to the logical
   1280 x 800 layout. The layout scales uniformly when you resize the window.
2. `App::update` processes editing input and advances the sweep and background animations.
3. `App::render` draws the controls, which apply changes to the scene.
4. `traceScene` emits directions and the selected source's wavelength bands.
   `traceRay` finds the nearest segment, prism face, or canvas boundary.
5. Walls, detectors, and boundaries stop a ray. Mirrors reflect it. Splitters spawn
   reflected and transmitted branches. A prism refracts each
   wavelength at entry and exit using Snell's law and a wavelength-dependent
   glass index, with optional unpolarized Fresnel reflection branches. Above the
   critical angle, glass reflects all power internally.
6. The ray advances a tiny distance past the hit to avoid hitting the same surface
   repeatedly through floating-point rounding. Mirror and internal reflections
   share the reflection limit; ordinary transmission has its own 64-step limit.
   When prisms overlap, the last placed prism determines the medium in the overlap.
7. The renderer draws one unsplit input beam, the transmitted spectral components,
   transparent prisms, metal laser and mirror hardware, and editing handles.
8. Detector readings sum arriving spectral power before display gain is applied.
   Each laser contributes equal emitted power. Detector percentages are relative
   to the combined emitted power of all lasers, and spectra sum arriving bands.

## Spectral model

Flint and crown glass use the Sellmeier coefficients for **SCHOTT N-SF11** and
**N-BK7**. The water option uses an approximate Cauchy model. Refraction angles
depend on wavelength, material, and the prism's actual apex geometry. Sources contain:

| Source | Wavelengths in the simulation |
| --- | --- |
| Yellow / red / green / blue laser | A single band at 589 / 650 / 532 / 460 nm |
| Cyan mixture | Blue (485 nm) + green (532 nm) |
| Purple mixture | Violet (440 nm) + red (650 nm) |
| White light | 15 equal-weight samples, 420-700 nm |

A prism separates wavelengths already present; it does not turn a red laser
into a rainbow. Cyan and purple are explicitly modeled mixtures. A display
color alone does not uniquely determine a real light source's spectrum.

Reference: [SCHOTT N-SF11 glass data](https://us.shop.schott.com/advanced-optics/en/Optical-Glass/N-SF11/c/glass-N-SF11),
[SCHOTT N-BK7 glass data](https://us.shop.schott.com/advanced-optics/en/Optical-Glass/SCHOTT-N-BK7/c/glass-SCHOTT%20N-BK7%C2%AE),
[Fresnel equations](https://www.rp-photonics.com/fresnel_equations.html),
[UCSB prism dispersion demonstration](https://web.physics.ucsb.edu/~lecturedemonstrations/Composer/Pages/80.21.html).

This is sampled geometric optics: displayed RGB hues, beam glow, and brightness
are illustrative and use display gain for legibility. Mirror reflection loses 18%
power; glass interfaces divide power using the unpolarized Fresnel equations when
enabled. Detector values use the underlying power, not displayed brightness.
The solver prunes branches below 0.05% of an individual wavelength's initial power,
limits each wavelength/direction to 256 segments, and bounds the scene to about
120,000 segments. Dense setups can undercount weak or truncated paths. The model
does not simulate diffraction, interference, or evolving polarization. Mirrors
reflect on both sides; detectors and splitters also work from either side.

For an easy code-reading order, start with `scene.hpp`, then `optics.cpp`, then
`app.cpp`. The window and drawing code are separate from the optical calculations.
