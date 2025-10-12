#include "platform.hpp"
#include <cstdlib>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace platform {

static fs::path home_dir() {
#if defined(_WIN32)
    const char* userprofile = std::getenv("USERPROFILE");
    if (userprofile) return fs::path(userprofile);
    const char* home = std::getenv("HOME");
    if (home) return fs::path(home);
    return fs::current_path();
#else
    const char* home = std::getenv("HOME");
    if (home) return fs::path(home);
    return fs::current_path();
#endif
}

fs::path resolve_config_path() {
    // Respect override env var first
    if (const char* env = std::getenv("ALMUSLIM_CONFIG")) {
        return fs::path(env);
    }

#if defined(_WIN32)
    fs::path base = home_dir();
    fs::path p = base / ".al-muslim" / "config.toml";
    return p;
#else
    fs::path base = home_dir();
    fs::path p = base / ".al-muslim" / "config.toml";
    return p;
#endif
}

} // namespace platform
