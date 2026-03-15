#include "freq-table.h"
#include "min-heap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void count_freq(uint32_t freqs[256], FILE* fp) {
    memset(freqs, 0, 256 * sizeof(uint32_t));
    uint32_t pos = ftell(fp);
    int byte;
    while ((byte = fgetc(fp)) != EOF) {
        freqs[(uint8_t)byte]++;
    }
    fseek(fp, pos, SEEK_SET);
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
        generate_codebook(cdb, curr_node->left, curr_code, depth+1);
    }
    if (curr_node->right != NULL) {
        curr_code[depth] = '1';
        generate_codebook(cdb, curr_node->right, curr_code, depth+1);
    }
}

huffman_node* create_huffman_tree(uint32_t freqs[256]) {
    // build frequence min-heap
    min_heap* mh = malloc(sizeof(min_heap));
    mh->len = 0;
    mh->cap = 0;
    mh->heap = NULL;

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

    return mh->heap[0];
}

int encode(char codebook[256][256], FILE* in_fp, FILE* out_fp) {
    uint8_t byte_buffer = 0;
    uint8_t bit_count = 0;

    uint8_t curr_byte = 0;
    uint32_t bytes_read = 0;
    while ((bytes_read = fread(&curr_byte, 1, 1, in_fp))) {
        if (bytes_read != 1) {
            printf("Read failed exiting routine with EXIT_FAILURE");
            return EXIT_FAILURE;
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
                    return EXIT_FAILURE;
                }
                bit_count = 0;
                byte_buffer = 0;
            }

            bit_idx++;
        }
    }

    uint8_t padding;
    if (bit_count != 0) {
        padding = 8-bit_count;
        byte_buffer <<= padding;
        if (fwrite(&byte_buffer, 1, 1, out_fp) != 1) {
            printf("Failed to write last byte buffer to the output");
            return EXIT_FAILURE;
        }
    } else {
        padding = 0;
    }
    if (fwrite(&padding, 1, 1, out_fp) != 1) {
        printf("Failed to write padding to the output");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

int decode(huffman_node* root, FILE* in_fp, FILE* out_fp) {
    uint8_t padding;

    fseek(in_fp, -1, SEEK_END);
    if (fread(&padding, 1, 1, in_fp) != 1) {
        printf("Failed to read padding bits number");
        return EXIT_FAILURE;
    }

    fseek(in_fp, 0, SEEK_END);
    uint32_t file_size = ftell(in_fp);
    uint32_t encoded_bytes = file_size - 1 - (uint32_t)(sizeof(uint32_t) * 256);
    uint64_t total_bits = (uint64_t)encoded_bytes * 8 - padding;

    fseek(in_fp, sizeof(uint32_t) * 256, SEEK_SET);

    huffman_node* curr_node = root;
    uint64_t bits_processed = 0;
    uint8_t curr_byte;

    while (bits_processed < total_bits) {
        if (fread(&curr_byte, 1, 1, in_fp) != 1) {
            printf("Failed to read byte from input");
            return EXIT_FAILURE;
        }

        for (int bit_idx = 7; bit_idx >= 0 && bits_processed < total_bits; bit_idx--) {
            int bit = (curr_byte >> bit_idx) & 1;
            curr_node = bit ? curr_node->right : curr_node->left;

            if (curr_node->left == NULL && curr_node->right == NULL) {
                if (fwrite(&curr_node->symbol, 1, 1, out_fp) != 1) {
                    printf("Failed to write byte to output file");
                    return EXIT_FAILURE;
                }
                curr_node = root;
            }

            bits_processed++;
        }
    }

    return EXIT_SUCCESS;
}

int huffman_compress(FILE* in_fp, FILE* out_fp) {
    uint32_t freqs[256];
    count_freq(freqs, in_fp);

    if (swrite_table(out_fp, freqs) != EXIT_SUCCESS) {
        printf("Failed to write frequencies");
        return EXIT_FAILURE;
    }

    huffman_node* root = create_huffman_tree(freqs);

    char codebook[256][256] = {0};
    char curr_code[256];

    generate_codebook(codebook, root, curr_code, 0);

    return encode(codebook, in_fp, out_fp);
}

int huffman_decompress(FILE *in_fp, FILE *out_fp) {
    uint32_t freqs[256];
    if (sread_table(in_fp, freqs)) {
        printf("failed to read frequency table: read");
        return EXIT_FAILURE;
    }

    huffman_node* root = create_huffman_tree(freqs);

    return decode(root, in_fp, out_fp);
}
