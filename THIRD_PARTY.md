# Third-Party Software

## raylib 5.5

- Author: Ramon Santamaria and contributors.
- Project: https://www.raylib.com/
- Release: https://github.com/raysan5/raylib/releases/tag/5.5
- License: zlib/libpng; the full notice is in `vendor/raylib/LICENSE`.
- Included files: raylib and raymath headers, and the Windows x64 static raylib library.

raylib supplies the native window, input, rendering, and `CheckCollisionLines`.
raymath supplies vector operations including `Vector2Reflect`.

The app loads Segoe UI from the Windows system font directory, or DejaVu Sans
from Linux system font directories, at runtime. No font files are redistributed.
If unavailable, it falls back to raylib's built-in font.
