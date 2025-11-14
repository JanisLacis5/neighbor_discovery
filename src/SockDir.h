#include <filesystem>

namespace fs = std::filesystem;

class SockDir {
public:
    SockDir(const fs::path& path) : path_{path} {
        if (!fs::exists(path_)) {
            if (fs::create_directories(path_)) {
            }
        }
    };

private:
    fs::path path_;
};
