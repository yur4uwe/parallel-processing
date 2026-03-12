#include "huffman.h"
#include "consts.h"
#include "min-heap.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void count_freq(uint32_t freqs[256], FILE* fp) {
    memset(freqs, 0, 256 * sizeof(uint32_t));
    int byte;
    while ((byte = fgetc(fp)) != EOF) {
        freqs[(uint8_t)byte]++;
    }
}

void generate_codebook(char cdb[256][256], huffman_node* curr_node, char curr_code[256], uint8_t depth) {
    // leaf node encountered
    if (curr_node->left == NULL && curr_node->right == NULL) {
        curr_code[depth] = '\0';
        strcpy(cdb[curr_node->symbol], curr_code);
        return;
    }

    if (curr_node->left != NULL) {
        curr_code[depth] = '0';
        generate_codebook(cdb, curr_node, curr_code, depth+1);
    }
    if (curr_node->right != NULL) {
        curr_code[depth] = '1';
        generate_codebook(cdb, curr_node, curr_code, depth+1);
    }
}

void create_codebook(uint32_t freqs[256], char codebook[256][256]) {
    // build frequence min-heap
    min_heap* mh = malloc(sizeof(min_heap));
    for (int i = 0; i < 256; i++) {
        if (freqs[i] == 0) {
            continue;
        }

        huffman_node* node = malloc(sizeof(huffman_node));
        node->freq = freqs[i];
        node->symbol = (uint8_t)i;
        node->left = NULL;
        node->right = NULL;

        insert(mh, node);
    }

    // build huffman tree
    while (mh->len != 1) {
        huffman_node* left = pop(mh);
        huffman_node* right = pop(mh);

        huffman_node* parent = malloc(sizeof(huffman_node));
        parent->symbol = 0; // meaningless for non-leaf nodes
        parent->freq = left->freq + right->freq;
        parent->left = left;
        parent->right = right;

        insert(mh, parent);
    }

    char curr_code[256];
    generate_codebook(codebook, mh->heap[0], curr_code, 0);
}

int encode(char codebook[256][256], FILE* in_fp, FILE* out_fp) {
    uint8_t byte_buffer = 0;
    uint8_t bit_count = 0;

    uint8_t curr_byte = 0;
    uint32_t bytes_read = 0;
    while ((bytes_read = fread(&curr_byte, 1, 1, in_fp))) {
        if (bytes_read != 1) {
           return FAILURE;
        }

        char* encoded = codebook[curr_byte];
        int bit_idx = 0;
        while (encoded[bit_idx] != '\0') {
            char bit = encoded[bit_idx];

            byte_buffer <<= 1;
            if (bit == '1') {
                byte_buffer |= 1;
            }
            bit_count++;

            if (bit_count == 8) { // on full byte, flush
                int written = fwrite(&byte_buffer, 1, 1, out_fp);
                if (written != 1) {
                    printf("Failed to write byte buffer to the output");
                    return FAILURE;
                }
                bit_count = 0;
                byte_buffer = 0;
            }

            bit_idx++;
        }
    }

    if (bit_count != 0) {
        uint8_t padding = 8-bit_count;
        byte_buffer <<= padding;
        int written = fwrite(&byte_buffer, 1, 1, out_fp);
        if (written != 1) {
            printf("Failed to write last byte buffer to the output");
            return FAILURE;
        }
        written = fwrite(&padding, 1, 1, out_fp);
        if (written != 1) {
            printf("Failed to write padding to the output");
            return FAILURE;
        }
    }

    return SUCCESS;
}

int decode(char codebook[256][256], FILE* in_fp, FILE* out_fp) {
    return FAILURE;
}

int huffman_compress(FILE* in_fp, FILE* out_fp) {
    uint32_t freqs[256];
    count_freq(freqs, in_fp);

    uint32_t written = fwrite(freqs, sizeof(uint32_t), 256, out_fp);
    if (written != 256 * sizeof(uint32_t)) {
        printf("Failed to write frequencies to the file instead written %d bytes", written);
        return FAILURE;
    }

    char codebook[256][256] = {0};
    create_codebook(freqs, codebook);

    return encode(codebook, in_fp, out_fp);
}

int huffman_decompress(FILE *in_fp, FILE *out_fp) {
    uint32_t freqs[256];
    uint32_t read = fread(freqs, sizeof(uint32_t), 256, in_fp);
    if (read != sizeof(uint32_t) * 256) {
        printf("failed to read frequency table");
        return FAILURE;
    }

    char codebook[256][256] = {0};
    create_codebook(freqs, codebook);

    return decode(codebook, in_fp, out_fp);
}
