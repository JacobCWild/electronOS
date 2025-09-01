#ifndef FAT32_H
#define FAT32_H

#include <stdint.h>

#define FAT32_SECTOR_SIZE 512
#define FAT32_MAX_FILENAME_LENGTH 255

typedef struct {
    uint8_t filename[FAT32_MAX_FILENAME_LENGTH];
    uint32_t size;
    uint32_t first_cluster;
} fat32_file_entry_t;

void fat32_init(void);
fat32_file_entry_t* fat32_open(const char* filename);
void fat32_read(fat32_file_entry_t* file, void* buffer, uint32_t size);
void fat32_close(fat32_file_entry_t* file);
uint32_t fat32_get_file_size(fat32_file_entry_t* file);

#endif // FAT32_H