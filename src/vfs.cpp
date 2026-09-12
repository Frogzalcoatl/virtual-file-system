#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vfs/vfs.hpp>

void VirtualFileSystem::validateHeader(VFS_Header& header, std::string_view expectedVersion) {
    if (header.magic != PackMagic) {
        if (useFolder) {
            return;
        }
        throw std::runtime_error("Invalid pack file magic\n" + packFilePath.filename().string());
    }
    std::string_view headerVersion(header.version, sizeof(header.version));
    size_t nullPos = headerVersion.find('\0');
    if (nullPos != std::string_view::npos) {
        headerVersion = headerVersion.substr(0, nullPos);
    }
    if (std::string_view(headerVersion) != expectedVersion) {
        if (useFolder) {
            return;
        }
        std::string error = "Invalid pack file version:\nExpected \"";
        error += expectedVersion;
        error += "\"\nFound \"";
        error += headerVersion;
        error += "\"\n";
        error += packFilePath.filename().string();
        throw std::runtime_error(error);
    }
}

void VirtualFileSystem::constructorDefault(std::string_view expectedVersion) {
    std::filesystem::path assetsPath = std::filesystem::path(basePath) / assetsFolderName;
    if (std::filesystem::exists(assetsPath) && std::filesystem::is_directory(assetsPath)) {
        useFolder = true;
    }
    std::ifstream packFile(packFilePath, std::ios::binary);
    if (!packFile.is_open()) {
        if (useFolder) {
            return;
        }
        throw std::runtime_error("Missing pack file\n" + packFilePath.filename().string());
    }
    VFS_Header header{};
    packFile.read(reinterpret_cast<char*>(&header), sizeof(VFS_Header));
    if (!packFile) {
        if (useFolder) {
            return;
        }
        throw std::runtime_error(
            "Failed to read header from pack file\n" + packFilePath.filename().string()
        );
    }
    validateHeader(header, expectedVersion); // Will throw error on failure
    entries.resize(header.fileCount);
    if (header.fileCount > 0) {
        packFile.read(
            reinterpret_cast<char*>(entries.data()),
            static_cast<long long>(entries.size() * sizeof(VFS_Entry))
        );
        if (!packFile) {
            if (useFolder) {
                entries.clear();
                return;
            }
            throw std::runtime_error(
                "Failed to read VFS entries from " + packFilePath.filename().string()
            );
        }
    }
    std::cout << "Initialized VFS for file: \"" << packFilePath.filename().string() << "\""
              << std::endl;
}

void VirtualFileSystem::constructorSDL(std::string_view expectedVersion) {
#ifndef VFS_USE_SDL
    constructorDefault(expectedVersion);
#else
#ifdef SDL_PLATFORM_ANDROID
    // No good way to check this for android
    useFolder = true;
#else
    std::filesystem::path assetsPath = std::filesystem::path(basePath) / assetsFolderName;
    SDL_PathInfo pathInfo;
    bool getPathResult = SDL_GetPathInfo(assetsPath.string().c_str(), &pathInfo);
    if (getPathResult && pathInfo.type == SDL_PATHTYPE_DIRECTORY) {
        useFolder = true;
    }
#endif
    packFileIO = SDL_IOFromFile(packFilePath.string().c_str(), "rb");
    if (!packFileIO) {
        if (useFolder) {
            return;
        }
        throw std::runtime_error(
            "Missing pack file: " + packFilePath.filename().string() +
            "\nAssets folder with name \"" + std::string(assetsFolderName) +
            "\" was also not found."
        );
    }
    VFS_Header header{};
    SDL_ReadIO(packFileIO, &header, sizeof(VFS_Header));
    validateHeader(header, expectedVersion); // Will throw error on failure
    entries.resize(header.fileCount);
    if (header.fileCount > 0) {
        SDL_ReadIO(packFileIO, entries.data(), entries.size() * sizeof(VFS_Entry));
    }
    SDL_Log("Initialized SDL VFS for file \"%s\"", packFilePath.filename().string().c_str());
#endif
}

VirtualFileSystem::VirtualFileSystem(
    const std::filesystem::path& basePath,
    const std::filesystem::path& packFileName,
    std::string_view expectedVersion,
    std::string_view assetsFolderName
)
    : basePath(basePath), packFilePath(std::filesystem::path(basePath) / packFileName),
      assetsFolderName(assetsFolderName) {
#ifdef VFS_USE_SDL
    constructorSDL(expectedVersion);
#else
    constructorDefault(expectedVersion);
#endif
}

VirtualFileSystem::~VirtualFileSystem() {
#ifdef VFS_USE_SDL
    if (packFileIO) {
        SDL_CloseIO(packFileIO);
        packFileIO = nullptr;
    }
#endif
}

std::vector<std::byte> VirtualFileSystem::readFileDefault(std::string_view relativeFilePath) {
    if (useFolder) {
        std::filesystem::path fullFilePath = basePath / assetsFolderName / relativeFilePath;
        if (std::filesystem::exists(fullFilePath) &&
            std::filesystem::is_regular_file(fullFilePath)) {
            // std::ios::ate starts the stream pointer at the end position to immediately get total
            // file size.
            std::ifstream file(fullFilePath, std::ios::binary | std::ios::ate);
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
    uint64_t hash = stringHash(relativeFilePath);
    auto iterator = std::lower_bound(
        entries.begin(), entries.end(), hash, [](const VFS_Entry& entry, uint64_t h) {
            return entry.id < h;
        }
    );
    if (iterator != entries.end() && iterator->id == hash) {
        std::ifstream packFile(packFilePath, std::ios::binary);
        if (packFile.is_open()) {
            packFile.seekg(static_cast<off_t>(iterator->offset), std::ios::beg);
            std::vector<std::byte> buffer(iterator->size);
            if (packFile.read(
                    reinterpret_cast<char*>(buffer.data()),
                    static_cast<std::streamsize>(iterator->size)
                )) {
                return buffer;
            }
        }
    }
    return {};
}

std::vector<std::byte> VirtualFileSystem::readFileSDL(std::string_view relativeFilePath) {
#ifndef VFS_USE_SDL
    return readFileDefault(relativeFilePath);
#else
#ifdef SDL_PLATFORM_ANDROID
    // basePath and assetsFolderName do not need to be included on android.
    std::filesystem::path fullFilePath = relativeFilePath;
#else
    std::filesystem::path fullFilePath = basePath / assetsFolderName / relativeFilePath;
#endif
    if (useFolder) {
        SDL_IOStream* io = SDL_IOFromFile(fullFilePath.string().c_str(), "rb");
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
    if (!packFileIO) {
        return {};
    }
    uint64_t hash = stringHash(relativeFilePath);
    auto iterator = std::lower_bound(
        entries.begin(), entries.end(), hash, [](const VFS_Entry& entry, uint64_t h) {
            return entry.id < h;
        }
    );
    if (iterator != entries.end() && iterator->id == hash) {
        if (SDL_SeekIO(packFileIO, static_cast<Sint64>(iterator->offset), SDL_IO_SEEK_SET) < 0) {
            return {};
        }
        std::vector<std::byte> buffer(iterator->size);
        size_t bytesRead = SDL_ReadIO(packFileIO, buffer.data(), iterator->size);
        if (bytesRead != iterator->size) {
            return {};
        }
        return buffer;
    }
    return {};
#endif
}

std::vector<std::byte> VirtualFileSystem::readFile(std::string_view relativeFilePath) {
#ifdef VFS_USE_SDL
    return readFileSDL(relativeFilePath);
#else
    return readFileDefault(relativeFilePath);
#endif
}