#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vfs/vfs.hpp>

void VirtualFileSystem::validate_header(VFS_Header& header, std::string_view expected_version) {
    if (header.magic != PACK_MAGIC) {
        if (use_folder_) {
            return;
        }
        throw std::runtime_error("Invalid pack file magic\n" + pack_file_path_.filename().string());
    }
    std::string_view header_version(header.version, sizeof(header.version));
    size_t null_pos = header_version.find('\0');
    if (null_pos != std::string_view::npos) {
        header_version = header_version.substr(0, null_pos);
    }
    if (std::string_view(header_version) != expected_version) {
        if (use_folder_) {
            return;
        }
        std::string error = "Invalid pack file version:\nExpected \"";
        error += expected_version;
        error += "\"\nFound \"";
        error += header_version;
        error += "\"\n";
        error += pack_file_path_.filename().string();
        throw std::runtime_error(error);
    }
}

void VirtualFileSystem::constructor_default(std::string_view expected_version) {
    std::filesystem::path assets_path = std::filesystem::path(base_path_) / assets_folder_name_;
    if (std::filesystem::exists(assets_path) && std::filesystem::is_directory(assets_path)) {
        use_folder_ = true;
    }
    std::ifstream pack_file(pack_file_path_, std::ios::binary);
    if (!pack_file.is_open()) {
        if (use_folder_) {
            return;
        }
        throw std::runtime_error("Missing pack file\n" + pack_file_path_.filename().string());
    }
    VFS_Header header{};
    pack_file.read(reinterpret_cast<char*>(&header), sizeof(VFS_Header));
    if (!pack_file) {
        if (use_folder_) {
            return;
        }
        throw std::runtime_error(
            "Failed to read header from pack file\n" + pack_file_path_.filename().string()
        );
    }
    validate_header(header, expected_version); // Will throw error on failure
    entries_.resize(header.file_count);
    if (header.file_count > 0) {
        pack_file.read(
            reinterpret_cast<char*>(entries_.data()),
            static_cast<long long>(entries_.size() * sizeof(VFS_Entry))
        );
        if (!pack_file) {
            if (use_folder_) {
                entries_.clear();
                return;
            }
            throw std::runtime_error(
                "Failed to read VFS entries_ from " + pack_file_path_.filename().string()
            );
        }
    }
    std::cout << "Initialized VFS for file: \"" << pack_file_path_.filename().string() << "\""
              << std::endl;
}

void VirtualFileSystem::constructor_sdl(std::string_view expected_version) {
#ifndef VFS_USE_SDL
    constructor_default(expectedVersion);
#else
#ifdef SDL_PLATFORM_ANDROID
    // No good way to check this for android
    use_folder = true;
#else
    std::filesystem::path assets_path = std::filesystem::path(base_path_) / assets_folder_name_;
    SDL_PathInfo path_info;
    bool get_path_result = SDL_GetPathInfo(assets_path.string().c_str(), &path_info);
    if (get_path_result && path_info.type == SDL_PATHTYPE_DIRECTORY) {
        use_folder_ = true;
    }
#endif
    pack_file_io_ = SDL_IOFromFile(pack_file_path_.string().c_str(), "rb");
    if (!pack_file_io_) {
        if (use_folder_) {
            return;
        }
        throw std::runtime_error(
            "Missing pack file: " + pack_file_path_.filename().string() +
            "\nAssets folder with name \"" + std::string(assets_folder_name_) +
            "\" was also not found."
        );
    }
    VFS_Header header{};
    SDL_ReadIO(pack_file_io_, &header, sizeof(VFS_Header));
    validate_header(header, expected_version); // Will throw error on failure
    entries_.resize(header.file_count);
    if (header.file_count > 0) {
        SDL_ReadIO(pack_file_io_, entries_.data(), entries_.size() * sizeof(VFS_Entry));
    }
    SDL_Log("Initialized SDL VFS for file \"%s\"", pack_file_path_.filename().string().c_str());
#endif
}

VirtualFileSystem::VirtualFileSystem(
    const std::filesystem::path& base_path,
    const std::filesystem::path& pack_file_name,
    std::string_view expected_version,
    std::string_view assets_folder_name
)
    : base_path_(base_path), pack_file_path_(std::filesystem::path(base_path) / pack_file_name),
      assets_folder_name_(assets_folder_name) {
#ifdef VFS_USE_SDL
    constructor_sdl(expected_version);
#else
    constructor_default(expectedVersion);
#endif
}

VirtualFileSystem::~VirtualFileSystem() {
#ifdef VFS_USE_SDL
    if (pack_file_io_) {
        SDL_CloseIO(pack_file_io_);
        pack_file_io_ = nullptr;
    }
#endif
}

std::vector<std::byte> VirtualFileSystem::read_file_default(std::string_view relative_file_path) {
    if (use_folder_) {
        std::filesystem::path full_file_path =
            base_path_ / assets_folder_name_ / relative_file_path;
        if (std::filesystem::exists(full_file_path) &&
            std::filesystem::is_regular_file(full_file_path)) {
            // std::ios::ate starts the stream pointer at the end position to immediately get total
            // file size.
            std::ifstream file(full_file_path, std::ios::binary | std::ios::ate);
            if (file.is_open()) {
                std::streamsize size = file.tellg();
                file.seekg(0, std::ios::beg);
                std::vector<std::byte> buffer(static_cast<size_t>(size));
                if (file.read(reinterpret_cast<char*>(buffer.data()), size)) {
                    return buffer;
                }
            }
        }
    }
    uint64_t hash = string_hash(relative_file_path);
    auto iterator = std::lower_bound(
        entries_.begin(), entries_.end(), hash, [](const VFS_Entry& entry, uint64_t h) {
            return entry.id < h;
        }
    );
    if (iterator != entries_.end() && iterator->id == hash) {
        std::ifstream pack_file(pack_file_path_, std::ios::binary);
        if (pack_file.is_open()) {
            pack_file.seekg(static_cast<off_t>(iterator->offset), std::ios::beg);
            std::vector<std::byte> buffer(iterator->size);
            if (pack_file.read(
                    reinterpret_cast<char*>(buffer.data()),
                    static_cast<std::streamsize>(iterator->size)
                )) {
                return buffer;
            }
        }
    }
    return {};
}

std::vector<std::byte> VirtualFileSystem::read_file_sdl(std::string_view relative_file_path) {
#ifndef VFS_USE_SDL
    return readFileDefault(relativeFilePath);
#else
#ifdef SDL_PLATFORM_ANDROID
    // basePath and assets_folder_name_ do not need to be included on android.
    std::filesystem::path full_file_path = relativeFilePath;
#else
    std::filesystem::path full_file_path = base_path_ / assets_folder_name_ / relative_file_path;
#endif
    if (use_folder_) {
        SDL_IOStream* io = SDL_IOFromFile(full_file_path.string().c_str(), "rb");
        if (io) {
            Sint64 ioSize = SDL_GetIOSize(io);
            if (ioSize >= 0) {
                std::vector<std::byte> buffer(static_cast<size_t>(ioSize));
                SDL_ReadIO(io, buffer.data(), static_cast<size_t>(ioSize));
                SDL_CloseIO(io);
                return buffer;
            }
            SDL_CloseIO(io);
        }
    }
    if (!pack_file_io_) {
        return {};
    }
    uint64_t hash = string_hash(relative_file_path);
    auto iterator = std::lower_bound(
        entries_.begin(), entries_.end(), hash, [](const VFS_Entry& entry, uint64_t h) {
            return entry.id < h;
        }
    );
    if (iterator != entries_.end() && iterator->id == hash) {
        if (SDL_SeekIO(pack_file_io_, static_cast<Sint64>(iterator->offset), SDL_IO_SEEK_SET) < 0) {
            return {};
        }
        std::vector<std::byte> buffer(iterator->size);
        size_t bytes_read = SDL_ReadIO(pack_file_io_, buffer.data(), iterator->size);
        if (bytes_read != iterator->size) {
            return {};
        }
        return buffer;
    }
    return {};
#endif
}

std::vector<std::byte> VirtualFileSystem::read_file(std::string_view relative_file_path) {
#ifdef VFS_USE_SDL
    return read_file_sdl(relative_file_path);
#else
    return read_file_default(relativeFilePath);
#endif
}