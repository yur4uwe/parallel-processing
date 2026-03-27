#pragma once

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t* buffer;
    size_t capacity;
    size_t offset;
} Arena;

Arena* new_arena(size_t size);
void* arena_alloc(Arena* a, size_t size);
void free_arena(Arena* a);
