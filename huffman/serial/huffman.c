#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../consts.h"
#include "../min-heap.h"
#include "freq-table.h"

void count_freq(uint32_t freqs[256], FILE* fp) {
    memset(freqs, 0, 256 * sizeof(uint32_t));
    uint32_t pos = ftell(fp);
    int byte;
    while ((byte = fgetc(fp)) != EOF) {
        freqs[(uint8_t)byte]++;
    }
    fseek(fp, pos, SEEK_SET);
}

void generate_codebook(char cdb[256][256], huffman_node* curr_node,
                       char curr_code[256], uint8_t depth) {
    // leaf node encountered
    if (curr_node->left == NULL && curr_node->right == NULL) {
        curr_code[depth] = '\0';
        strcpy(cdb[curr_node->symbol], curr_code);
        return;
    }

    if (curr_node->left != NULL) {
        curr_code[depth] = '0';
        generate_codebook(cdb, curr_node->left, curr_code, depth + 1);
    }
    if (curr_node->right != NULL) {
        curr_code[depth] = '1';
        generate_codebook(cdb, curr_node->right, curr_code, depth + 1);
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
        parent->symbol = 0;  // meaningless for non-leaf nodes
        parent->freq = left->freq + right->freq;
        parent->left = left;
        parent->right = right;

        insert(mh, parent);
    }

    return mh->heap[0];
}

int encode(char codebook[256][256], FILE* in_fp, FILE* out_fp) {
    int ec = EXIT_SUCCESS;

    // 1. Calculate chunk count
    uint32_t current_in_pos = ftell(in_fp);
    fseek(in_fp, 0, SEEK_END);
    uint32_t file_size = ftell(in_fp);
    fseek(in_fp, current_in_pos, SEEK_SET);

    uint32_t chunk_num = (file_size + CHUNK_SIZE - 1) / CHUNK_SIZE;
    if (file_size == 0) chunk_num = 0;

    // 2. Write Chunk Count to out_fp
    if (fwrite(&chunk_num, sizeof(uint32_t), 1, out_fp) != 1) {
        return EXIT_FAILURE;
    }

    if (chunk_num == 0) return EXIT_SUCCESS;

    // 3. Reserve space for Chunk Size Table
    uint32_t index_table_pos = ftell(out_fp);
    uint32_t* chunks_index = malloc(sizeof(uint32_t) * chunk_num);
    if (!chunks_index) return EXIT_FAILURE;
    fseek(out_fp, chunk_num * sizeof(uint32_t), SEEK_CUR);

    uint8_t chunk_buf[CHUNK_SIZE];
    uint32_t chunk_idx = 0;

    // 4. Process Chunks
    size_t bytes_read;
    while ((bytes_read = fread(chunk_buf, 1, CHUNK_SIZE, in_fp)) > 0) {
        uint8_t byte_buffer = 0;
        uint8_t bit_count = 0;

        uint32_t chunk_start_pos = ftell(out_fp);

        // Skip 1 byte for padding info (to be written after chunk is
        // compressed)
        fseek(out_fp, 1, SEEK_CUR);

        for (size_t i = 0; i < bytes_read; i++) {
            char* encoded = codebook[chunk_buf[i]];
            for (int bit_idx = 0; encoded[bit_idx] != '\0'; bit_idx++) {
                byte_buffer = (byte_buffer << 1) | (encoded[bit_idx] - '0');
                bit_count++;

                if (bit_count == 8) {
                    if (fwrite(&byte_buffer, 1, 1, out_fp) != 1) {
                        printf("Failed to write byte buffer to the output\n");
                        ec = EXIT_FAILURE;
                        goto exit;
                    }
                    bit_count = 0;
                    byte_buffer = 0;
                }
            }
        }

        // Flush remaining bits
        uint8_t padding = 0;
        if (bit_count > 0) {
            padding = 8 - bit_count;
            byte_buffer <<= padding;
            if (fwrite(&byte_buffer, 1, 1, out_fp) != 1) {
                printf("Failed to write last byte buffer to the output\n");
                ec = EXIT_FAILURE;
                goto exit;
            }
        }

        uint32_t chunk_end_pos = ftell(out_fp);

        // Write padding byte at the start of this chunk
        fseek(out_fp, chunk_start_pos, SEEK_SET);
        if (fwrite(&padding, 1, 1, out_fp) != 1) {
            printf("Failed to write padding to the output\n");
            ec = EXIT_FAILURE;
            goto exit;
        }

        // Record compressed size (including padding byte)
        chunks_index[chunk_idx++] = chunk_end_pos - chunk_start_pos;

        // Resume at end of bitstream for next chunk
        fseek(out_fp, chunk_end_pos, SEEK_SET);
    }

    // 5. Write the Chunk Size Table
    fseek(out_fp, index_table_pos, SEEK_SET);
    if (fwrite(chunks_index, sizeof(uint32_t), chunk_num, out_fp) !=
        chunk_num) {
        printf("failed to write chunking index to file\n");
        ec = EXIT_FAILURE;
    }

exit:
    free(chunks_index);
    return ec;
}

int decode(huffman_node* root, FILE* in_fp, FILE* out_fp) {
    uint32_t chunk_count;
    if (fread(&chunk_count, sizeof(uint32_t), 1, in_fp) != 1) {
        printf("failed to read amount of chunks\n");
        return EXIT_FAILURE;
    }

    if (chunk_count == 0) return EXIT_SUCCESS;

    int ec = EXIT_SUCCESS;
    uint32_t* index_table = malloc(chunk_count * sizeof(uint32_t));
    if (!index_table) return EXIT_FAILURE;

    if (fread(index_table, sizeof(uint32_t), chunk_count, in_fp) !=
        chunk_count) {
        printf("failed to read chunk index\n");
        ec = EXIT_FAILURE;
        goto exit;
    }

    uint8_t* chunk_data = NULL;

    for (uint32_t i = 0; i < chunk_count; i++) {
        uint32_t total_chunk_size = index_table[i];
        if (total_chunk_size == 0) continue;

        uint8_t chunk_padding = 0;
        if (fread(&chunk_padding, 1, 1, in_fp) != 1) {
            printf("failed to read padding for chunk %u\n", i);
            ec = EXIT_FAILURE;
            goto exit;
        }

        uint32_t bitstream_size = total_chunk_size - 1;
        chunk_data = malloc(bitstream_size);
        if (!chunk_data) {
            ec = EXIT_FAILURE;
            goto exit;
        }

        if (fread(chunk_data, 1, bitstream_size, in_fp) != bitstream_size) {
            printf("failed to read chunk %u data\n", i);
            ec = EXIT_FAILURE;
            goto exit;
        }

        huffman_node* curr_node = root;
        for (uint32_t j = 0; j < bitstream_size; j++) {
            uint8_t curr_byte = chunk_data[j];

            for (int bit_idx = 7; bit_idx >= 0; bit_idx--) {
                if (j == bitstream_size - 1 && bit_idx < (int)chunk_padding) {
                    break;
                }

                int bit = (curr_byte >> bit_idx) & 1;
                curr_node = bit ? curr_node->right : curr_node->left;

                if (curr_node->left == NULL && curr_node->right == NULL) {
                    if (fwrite(&curr_node->symbol, 1, 1, out_fp) != 1) {
                        printf("Failed to write byte to output file\n");
                        ec = EXIT_FAILURE;
                        goto exit;
                    }
                    curr_node = root;
                }
            }
        }
        free(chunk_data);
        chunk_data = NULL;
    }

exit:
    free(index_table);
    if (chunk_data) free(chunk_data);
    return ec;
}

int huffman_compress(FILE* in_fp, FILE* out_fp) {
    uint32_t freqs[256];
    count_freq(freqs, in_fp);

    if (write_table(out_fp, freqs) != EXIT_SUCCESS) {
        printf("Failed to write frequencies");
        return EXIT_FAILURE;
    }

    huffman_node* root = create_huffman_tree(freqs);

    char codebook[256][256] = {0};
    char curr_code[256];

    generate_codebook(codebook, root, curr_code, 0);

    return encode(codebook, in_fp, out_fp);
}

int huffman_decompress(FILE* in_fp, FILE* out_fp) {
    uint32_t freqs[256] = {0};
    if (read_table(in_fp, freqs)) {
        printf("failed to read frequency table: read");
        return EXIT_FAILURE;
    }

    huffman_node* root = create_huffman_tree(freqs);

    return decode(root, in_fp, out_fp);
}
