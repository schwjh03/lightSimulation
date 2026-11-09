#include "native_windows.hpp"
#include <commdlg.h>
#include "scene_dialogs.hpp"
#include "app.hpp"

namespace lightlab {
void processSceneDialogs(App& app, void* owner) {
    if (!app.library)
        return;
    bool open = app.openRequested;
    bool save = app.saveRequested || app.saveAsRequested;
    app.openRequested = app.saveRequested = app.saveAsRequested = false;
    if (!open && !save)
        return;
    wchar_t file[32768]{};
    if (!open) {
        auto filename = app.library->currentPath().empty()
                            ? std::filesystem::u8path(std::string(presetName(app.scene.preset)) + ".lightscene")
                            : app.library->currentPath().filename();
        wcsncpy_s(file, filename.c_str(), _TRUNCATE);
    }
    auto directory = app.library->scenesDirectory().wstring();
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = static_cast<HWND>(owner);
    dialog.lpstrFile = file;
    dialog.nMaxFile = 32768;
    dialog.lpstrFilter = L"Light Lab scenes (*.lightscene;*.txt)\0*.lightscene;*.txt\0All files\0*.*\0";
    dialog.lpstrInitialDir = directory.c_str();
    dialog.lpstrDefExt = L"lightscene";
    dialog.lpstrTitle = open ? L"Open saved Light Lab scene" : L"Save Light Lab scene as";
    dialog.Flags =
        OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST | (open ? OFN_FILEMUSTEXIST : OFN_OVERWRITEPROMPT);
    BOOL chosen = open ? GetOpenFileNameW(&dialog) : GetSaveFileNameW(&dialog);
    if (!chosen)
        return; // Cancelling does not replace or discard the scene.
    auto path = std::filesystem::path(file).u8string();
    if (open)
        app.openNamed(path);
    else
        app.saveNamed(path);
}
}
