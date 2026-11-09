#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
// Keep the Windows declarations from colliding with raylib's public C API.
#define Rectangle Win32Rectangle
#define CloseWindow Win32CloseWindow
#define ShowCursor Win32ShowCursor
#include <windows.h>
#include <dwmapi.h>
#undef Rectangle
#undef CloseWindow
#undef ShowCursor
#undef DrawText
#undef DrawTextEx
#undef LoadImage
#undef PlaySound
