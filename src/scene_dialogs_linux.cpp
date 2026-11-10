#ifndef _WIN32
#include "scene_dialogs.hpp"
#include "app.hpp"
#include <cerrno>
#include <sys/wait.h>
#include <unistd.h>

namespace lightlab {
void processSceneDialogs(App& app, void*) {
    if (!app.library)
        return;
    bool open = app.openRequested;
    bool save = app.saveRequested || app.saveAsRequested;
    app.openRequested = app.saveRequested = app.saveAsRequested = false;
    if (!open && !save)
        return;
    auto initial =
        open ? app.library->scenesDirectory().string() + "/"
             : (app.library->currentPath().empty()
                    ? app.library->scenesDirectory() / (std::string(presetName(app.scene.preset)) + ".lightscene")
                    : app.library->currentPath())
                   .string();
    int output[2];
    if (pipe(output) != 0)
        return;
    auto child = fork();
    if (child == 0) {
        close(output[0]);
        dup2(output[1], STDOUT_FILENO);
        close(output[1]);
        if (open)
            execlp("zenity", "zenity", "--file-selection", "--title=Open Light Lab scene", "--filename",
                   initial.c_str(), "--file-filter=Light Lab scenes | *.lightscene *.txt", nullptr);
        else
            execlp("zenity", "zenity", "--file-selection", "--save", "--confirm-overwrite",
                   "--title=Save Light Lab scene", "--filename", initial.c_str(), nullptr);
        _exit(127);
    }
    close(output[1]);
    std::string path;
    char buffer[1024];
    ssize_t length;
    while ((length = read(output[0], buffer, sizeof(buffer))) != 0) {
        if (length > 0)
            path.append(buffer, length);
        else if (errno != EINTR)
            break;
    }
    close(output[0]);
    if (child < 0)
        return;
    int status = 0;
    while (waitpid(child, &status, 0) < 0)
        if (errno != EINTR)
            return;
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        if (WIFEXITED(status) && WEXITSTATUS(status) == 127)
            TraceLog(LOG_WARNING, "Install zenity to use Open and Save As dialogs.");
        return;
    }
    if (!path.empty() && path.back() == '\n')
        path.pop_back();
    if (path.empty())
        return;
    if (open)
        app.openNamed(path);
    else
        app.saveNamed(path);
}
}
#endif
