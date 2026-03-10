#pragma once

#include "huffman.h"
#include <stdint.h>

typedef struct {
    huffman_node** heap;
    uint32_t len;
    uint32_t cap;
} min_heap;

void insert(min_heap* ref, huffman_node* node);
huffman_node* pop(min_heap* ref);
