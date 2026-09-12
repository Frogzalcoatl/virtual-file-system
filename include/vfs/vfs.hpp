#pragma once
#include <filesystem>
#include <vector>
#include <vfs/packer.hpp>

#ifdef VFS_USE_SDL
#include <SDL3/SDL.h>
#endif

class VirtualFileSystem {
  private:
    const std::filesystem::path base_path_;
    std::filesystem::path pack_file_path_;
    std::string_view assets_folder_name_;
    std::vector<VFS_Entry> entries_;
    bool use_folder_ = false;

#ifdef VFS_USE_SDL
    SDL_IOStream* pack_file_io_ = nullptr;
#endif

    // throws std::runtime_error on invalid header.
    void validate_header(VFS_Header& header, std::string_view expected_version);

    void constructor_default(std::string_view expected_version);

    void constructor_sdl(std::string_view expected_version);

    std::vector<std::byte> read_file_default(std::string_view relative_file_path);

    std::vector<std::byte> read_file_sdl(std::string_view relative_file_path);

  public:
    VirtualFileSystem(
        const std::filesystem::path& base_path,
        const std::filesystem::path& pack_file_name,
        std::string_view expected_version,
        std::string_view assets_folder_name
    );

    ~VirtualFileSystem();

    // Prevent copying
    VirtualFileSystem(const VirtualFileSystem&) = delete;
    VirtualFileSystem& operator=(const VirtualFileSystem&) = delete;

    // Allow moving (suggestion from AI)
    VirtualFileSystem(VirtualFileSystem&& other) noexcept;
    VirtualFileSystem& operator=(VirtualFileSystem&& other) noexcept;

    // Should be path.generic_string() with forward slashes
    std::vector<std::byte> read_file(std::string_view relative_file_path);
};