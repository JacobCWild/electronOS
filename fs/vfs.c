#include "vfs.h"

#include <stddef.h>
#include <stdint.h>

#define MAX_FILES 128
#define MAX_FILENAME_LENGTH 255

typedef struct {
    char name[MAX_FILENAME_LENGTH];
    uint32_t size;
    uint32_t inode;
} File;

typedef struct {
    File files[MAX_FILES];
    uint32_t file_count;
} VirtualFileSystem;

static VirtualFileSystem vfs;

void vfs_init() {
    vfs.file_count = 0;
}

int vfs_create_file(const char *name, uint32_t size) {
    if (vfs.file_count >= MAX_FILES) {
        return -1; // VFS is full
    }
    for (uint32_t i = 0; i < vfs.file_count; i++) {
        if (strcmp(vfs.files[i].name, name) == 0) {
            return -2; // File already exists
        }
    }
    strncpy(vfs.files[vfs.file_count].name, name, MAX_FILENAME_LENGTH);
    vfs.files[vfs.file_count].size = size;
    vfs.files[vfs.file_count].inode = vfs.file_count; // Simple inode assignment
    vfs.file_count++;
    return 0; // Success
}

File *vfs_get_file(const char *name) {
    for (uint32_t i = 0; i < vfs.file_count; i++) {
        if (strcmp(vfs.files[i].name, name) == 0) {
            return &vfs.files[i];
        }
    }
    return NULL; // File not found
}

int vfs_delete_file(const char *name) {
    for (uint32_t i = 0; i < vfs.file_count; i++) {
        if (strcmp(vfs.files[i].name, name) == 0) {
            // Shift files down to remove the deleted file
            for (uint32_t j = i; j < vfs.file_count - 1; j++) {
                vfs.files[j] = vfs.files[j + 1];
            }
            vfs.file_count--;
            return 0; // Success
        }
    }
    return -1; // File not found
}

uint32_t vfs_get_file_count() {
    return vfs.file_count;
}