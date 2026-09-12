#pragma once
#include <cstdint>
#include <string_view> // Required for std::string_view

// Packer and VirtualFileSystem is heavily AI inspired.
// Would not have come up with this idea on my own.
// Many new concepts learned please let me know if AI led me astray.

// WVFS backwards. (W Virtual File System) [hopefully itll be a W]
#define PackMagic 0x53465657

constexpr uint64_t stringHash(std::string_view str) {
    uint64_t hash = 14695981039346656037ULL;
    for (char c : str) {
        hash ^= static_cast<unsigned char>(c);
        hash *= 1099511628211ULL;
    }
    return hash;
}

#pragma pack(push, 1) // Force 1-byte alignment (no padding) in the below structs
struct VFS_Header {
    uint32_t magic;
    char version[16];
    uint32_t fileCount;
    uint32_t _pad;
};

struct VFS_Entry {
    uint64_t id;
    uint64_t offset;
    uint64_t size;
};
#pragma pack(pop)