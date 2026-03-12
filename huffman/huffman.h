#pragma once

#include <stdio.h>
#include <stdint.h>

typedef struct huffman_node {
    uint8_t symbol;
    uint32_t freq;

    struct huffman_node* left;
    struct huffman_node* right;
} huffman_node;

int huffman_compress(FILE* in_fp, FILE* out_fp);
int huffman_decompress(FILE* in_fp, FILE* out_fp);
