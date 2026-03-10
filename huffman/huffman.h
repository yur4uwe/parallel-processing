#pragma once

#include <stdint.h>

typedef struct huffman_node {
    uint8_t symbol;
    uint32_t freq;

    struct huffman_node* left;
    struct huffman_node* right;
} huffman_node;
