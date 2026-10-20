# Light Simulation

A C++17 / SFML 3 optical playground. Create multiple colored lasers, reflect
them with mirrors, and stop them with walls. The right sidebar contains all tools.

## Build on Windows (MSYS2 UCRT64)

Install the matching compiler, GNU Make, and SFML development package once in an
MSYS2 UCRT64 terminal:

```sh
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-make mingw-w64-ucrt-x86_64-sfml
```

With `C:\msys64\ucrt64\bin` on PATH, open PowerShell in this folder:

```powershell
.\make.ps1          # Build
.\make.ps1 run      # Build and launch
.\make.ps1 test     # Optics and editor behavior tests
.\make.ps1 clean    # Remove build outputs
.\make.ps1 full     # Clean, rebuild, and launch
```

The script invokes GNU Make, which MSYS2 names `mingw32-make.exe`. You can also
run `mingw32-make run` directly. If your GNU Make is named `make`, use `make run`.
The Makefile finds SFML alongside the active compiler; a custom matching SFML 3
installation can be selected with `mingw32-make SFML_DIR=C:/path/to/SFML`.
Its `bin` folder must be on PATH when launching the executable directly.

Linux/macOS: install a C++17 compiler, GNU Make, and SFML **3** development
libraries, then run `make`, `make run`, or `make test`. SFML 2 is not supported.

## Controls

- **Laser node:** drag the tool onto the canvas, or click the tool and then the
  canvas. Drag the source to move it; drag its small aim handle to rotate the beam.
- **Colors:** green, red, blue, purple, and white. A swatch changes the selected
  laser and the color used for new lasers.
- **Mirror / wall:** select the tool and click two endpoints, or press and drag
  across the canvas. You can also drag the tool onto the canvas to set the first
  endpoint, then click the second endpoint. A preview follows the pointer.
- **Edit:** click an existing mirror or wall. Drag either endpoint to change its
  length and angle, or drag its middle to move it without changing its shape.
  Placement automatically returns to Select so the new object can be edited.
- **Delete / Backspace:** remove the selected object. The sidebar also has a
  Delete button. **Esc / right-click:** cancel placement or the current edit.
- **1 / 2 / 3 / 4:** Select, Laser, Mirror, Wall.

Beams update immediately during editing, stop at canvas edges, reflect from both
sides of mirrors, and are absorbed by walls. Reflections are capped at 64 to keep
opposing mirrors responsive. Geometry stays inside the canvas, and surfaces have
a minimum length of 8 pixels. Losing window focus cancels unfinished edits.

The UI uses a system font (Segoe UI on Windows, DejaVu Sans on Linux, Arial on
macOS). Set `LIGHTSIM_FONT` to a TrueType font path to override it.
