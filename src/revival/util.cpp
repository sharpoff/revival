#include "util.h"
#include <unistd.h>

#define MAX_PATH 100

namespace util
{
    std::filesystem::path getProjectRoot()
    {
        // TODO: add windows support
        char path[MAX_PATH];
        ssize_t count = readlink("/proc/self/exe", path, 100);
        if (count > 0) {
            auto exePath = std::filesystem::path(path);
            // TODO: add checks if parent path exist
            return exePath.parent_path().parent_path();
        }
        return "";
    }
} // namespace util