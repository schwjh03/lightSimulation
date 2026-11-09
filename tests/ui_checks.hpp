#pragma once

#include "app.hpp"
#include <functional>
#include <fstream>

namespace lightlab {
class UiChecks {
public:
    UiChecks(App& app, bool enabled);
    void beforeFrame();
    void afterFrame();
    bool done() const;
    int result() const;

private:
    struct Step {
        const char* name;
        Pointer pointer;
        std::function<bool()> verify;
    };

    App& app;
    bool enabled;
    size_t current = 0;
    int code = 0;
    double pausedBackground = 0;
    std::vector<Step> steps;
    std::ofstream report;
};
} // namespace lightlab
