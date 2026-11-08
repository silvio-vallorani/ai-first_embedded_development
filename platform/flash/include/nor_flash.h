#ifndef AIED_NOR_FLASH_H
#define AIED_NOR_FLASH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    bool (*read)(void *context, uint32_t address, void *destination, size_t length);
    bool (*program)(void *context, uint32_t address, const void *source, size_t length);
    bool (*erase_sector)(void *context, uint32_t address);
    bool (*is_ready)(void *context);
    void *context;
    uint32_t capacity_bytes;
    uint32_t page_size_bytes;
    uint32_t erase_sector_size_bytes;
} aied_nor_flash_t;

#endif
