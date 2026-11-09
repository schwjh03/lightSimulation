#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace lightlab {
class App;

class DetectorWindows {
public:
    DetectorWindows(void* owner, bool hidden);
    ~DetectorWindows();
    DetectorWindows(const DetectorWindows&) = delete;
    DetectorWindows& operator=(const DetectorWindows&) = delete;
    void update(App& app);
    void closeAll();
    std::size_t count() const;
    void* nativeHandle(std::uint64_t id) const;
    float displayedPower(std::uint64_t id) const;
    bool captureFirst(const std::string& path) const;
    bool captureWindow(std::uint64_t id, const std::string& path) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
} // namespace lightlab
