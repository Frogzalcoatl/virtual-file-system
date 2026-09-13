#include <cstdlib>
#include <cstring>
#include <vfs/vfs.hpp>
#include <vfs/vfs_c.h>

vfs_t* vfs_create(
    const char* base_path,
    const char* pack_file_name,
    const char* expected_version,
    const char* assets_folder_name
) {
    try {
        auto* vfs = new VirtualFileSystem(
            base_path ? base_path : "",
            pack_file_name ? pack_file_name : "",
            expected_version ? expected_version : "",
            assets_folder_name ? assets_folder_name : ""
        );
        return reinterpret_cast<vfs_t*>(vfs);
    } catch (...) {
        return nullptr;
    }
}

void vfs_destroy(vfs_t* vfs) {
    if (vfs) {
        delete reinterpret_cast<VirtualFileSystem*>(vfs);
    }
}

void* vfs_read_file(vfs_t* vfs, const char* relative_file_path, size_t* out_size) {
    if (!vfs || !relative_file_path || !out_size) {
        if (out_size) {
            *out_size = 0;
        }
        return nullptr;
    }
    try {
        auto* cpp_vfs = reinterpret_cast<VirtualFileSystem*>(vfs);
        std::vector<std::byte> data = cpp_vfs->read_file(relative_file_path);
        if (data.empty()) {
            *out_size = 0;
            return nullptr;
        }
        void* buffer = std::malloc(data.size());
        if (!buffer) {
            *out_size = 0;
            return nullptr;
        }
        std::memcpy(buffer, data.data(), data.size());
        *out_size = data.size();
        return buffer;
    } catch (...) {
        *out_size = 0;
        return nullptr;
    }
}

void vfs_free_buffer(void* buffer) {
    std::free(buffer);
}