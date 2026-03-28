#include "arena.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

Arena* new_arena(size_t size) {
    uint8_t* arena_buffer = malloc(size);

    Arena* a = malloc(sizeof(Arena));
    a->buffer = arena_buffer;
    a->capacity = size;
    a->offset = 0;

    return a;
}
void* arena_alloc(Arena* a, size_t size) {
    size_t aligned = (a->offset + 7) & ~7;
    if (aligned + size > a->capacity) {  // ← also move check after alignment
        return NULL;
    }
    a->offset = aligned + size;
    return (void*)(a->buffer + aligned);  // ← return aligned, not updated offset
}

void free_arena(Arena* a) {
    if (a == NULL) return;
    if (a->buffer) free(a->buffer);
    free(a);
}
