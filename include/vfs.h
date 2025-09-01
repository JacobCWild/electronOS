#ifndef VFS_H
#define VFS_H

#include <stdint.h>

// File system types
#define FS_TYPE_FAT32 1

// Structure representing a file
typedef struct {
    char name[256];
    uint32_t size;
    uint32_t start_block;
} File;

// Structure representing a directory
typedef struct {
    char name[256];
    uint32_t file_count;
    File *files;
} Directory;

// Function to initialize the virtual file system
void vfs_init(void);

// Function to mount a file system
int vfs_mount(const char *device, int fs_type);

// Function to open a file
File *vfs_open(const char *path);

// Function to read from a file
int vfs_read(File *file, void *buffer, uint32_t size);

// Function to close a file
void vfs_close(File *file);

// Function to list files in a directory
Directory *vfs_list_dir(const char *path);

// Function to unmount a file system
void vfs_unmount(const char *device);

#endif // VFS_H