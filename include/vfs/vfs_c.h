#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle to the C++ VirtualFileSystem object
typedef struct vfs_t vfs_t;

vfs_t* vfs_create(
    const char* base_path,
    const char* pack_file_name,
    const char* expected_version,
    const char* assets_folder_name
);

void vfs_destroy(vfs_t* vfs);

void* vfs_read_file(vfs_t* vfs, const char* relative_file_path, size_t* out_size);

void vfs_free_buffer(void* buffer);

#ifdef __cplusplus
}
#endif