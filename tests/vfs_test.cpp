#include "SDL3/SDL_filesystem.h"
#include "SDL3/SDL_init.h"
#include "vfs/vfs.hpp"

int main() {
    int returnVal = 0;
    try {
        if (!SDL_Init(SDL_INIT_EVENTS)) {
            SDL_Log("Hello");
            return 1;
        }
        VirtualFileSystem vfs(SDL_GetBasePath(), "test.dat", "1.0.0", "assets");
        const std::vector<std::byte> hello = vfs.readFile("hello.txt");
        const std::vector<std::byte> test = vfs.readFile("nested/test.txt");
        SDL_Log("Length of hello: %zu", hello.size());
        SDL_Log("Length of test: %zu", test.size());
    } catch (std::exception& e) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Fatal Error", e.what(), NULL);
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Fatal exception caught: %s", e.what());
        returnVal = 1;
    }
    SDL_Quit();
    return returnVal;
}