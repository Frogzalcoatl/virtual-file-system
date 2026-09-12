#include "SDL3/SDL_filesystem.h"
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_log.h"
#include "vfs/vfs.hpp"

int main(int argc, char* argv[]) {
    std::string mode;
    if (argc < 2) {
        SDL_Log("No mode provided, assuming \"folder\"");
        mode = "folder";
    } else {
        mode = argv[1];
    }
    if (mode != "folder" && mode != "pack") {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Unknown mode: %s", mode.c_str());
        return 1;
    }
    if (!SDL_Init(SDL_INIT_EVENTS)) {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init failed: %s", SDL_GetError());
        return 1;
    }
    const char* basePath = SDL_GetBasePath();
    if (!basePath) {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "SDL_GetBasePath returned null");
        return 1;
    }
    int returnVal = 0;
    VirtualFileSystem* vfs;
    try {
        if (mode == "folder") {
            vfs = new VirtualFileSystem(SDL_GetBasePath(), "does_not_exist.dat", "1.0.0", "assets");
        } else {
            vfs = new VirtualFileSystem(SDL_GetBasePath(), "does_not_exist.dat", "1.0.0", "assets");
        }
        const std::vector<std::byte> hello = vfs->readFile("hello.txt");
        const std::vector<std::byte> test = vfs->readFile("nested/test.txt");
        SDL_Log("%s mode: hello.txt length = %zu", mode.c_str(), hello.size());
        SDL_Log("%s mode: nested/test.txt length = %zu", mode.c_str(), test.size());
        if (hello.empty() || test.empty()) {
            SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Failed to read files from folder!");
            returnVal = 1;
        }
    } catch (std::exception& e) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Fatal Error", e.what(), NULL);
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Fatal exception caught: %s", e.what());
        returnVal = 1;
    }
    if (vfs) {
        delete vfs;
    }
    SDL_Quit();
    return returnVal;
}