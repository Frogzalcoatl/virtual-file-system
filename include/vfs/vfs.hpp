#pragma once
#include <filesystem>
#include <vector>
#include <vfs/packer.hpp>


#ifdef VFS_USE_SDL
#include <SDL3/SDL.h>
#endif

class VirtualFileSystem {
  private:
    const std::filesystem::path basePath;
    std::filesystem::path packFilePath;
    std::string_view assetsFolderName;
    std::vector<VFS_Entry> entries;
    bool useFolder = false;

#ifdef VFS_USE_SDL
    SDL_IOStream* packFileIO = nullptr;
#endif

    // throws std::runtime_error on invalid header.
    void validateHeader(VFS_Header& header, std::string_view expectedVersion);

    void constructorDefault(std::string_view expectedVersion);

    void constructorSDL(std::string_view expectedVersion);

    std::vector<std::byte> readFileDefault(std::string_view relativeFilePath);

    std::vector<std::byte> readFileSDL(std::string_view relativeFilePath);

  public:
    VirtualFileSystem(
        const std::filesystem::path& basePath,
        const std::filesystem::path& packFileName,
        std::string_view expectedVersion,
        std::string_view assetsFolderName
    );

    ~VirtualFileSystem();

    // Prevent copying
    VirtualFileSystem(const VirtualFileSystem&) = delete;
    VirtualFileSystem& operator=(const VirtualFileSystem&) = delete;

    // Allow moving (suggestion from AI)
    VirtualFileSystem(VirtualFileSystem&& other) noexcept;
    VirtualFileSystem& operator=(VirtualFileSystem&& other) noexcept;

    // Should be path.generic_string() with forward slashes
    std::vector<std::byte> readFile(std::string_view relativeFilePath);
};