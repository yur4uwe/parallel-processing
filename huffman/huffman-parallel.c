#include "huffman-node.h"
#include "min-heap.h"
#include <stdint.h>
#include <stdlib.h>
#include <mpi.h>
#include <string.h>

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

void count_freqs(uint32_t freqs[256], uint8_t* buf, int chunk_size) {
    memset(freqs, 0, 256 * sizeof(uint32_t));

    for (uint32_t i = 0; i < chunk_size; i++) {
        freqs[buf[i]]++;
    }
}

int worker_encode(char codebook[256][256], uint8_t* buf, int chunk_size) {
    int ec = EXIT_SUCCESS;

    uint8_t byte_buffer = 0;
    uint8_t bit_count = 0;
    for (uint32_t i = 0; i < chunk_size; i++) {
        char* encoded = codebook[buf[i]];
        int bit_idx = 0;

        while (encoded[bit_idx] != '\0') {
            char bit = encoded[bit_idx];

            byte_buffer <<= 1;
            if (bit == '1') {
                byte_buffer |= 1;
            }
            bit_count++;

            if (bit_count == 8) {
                // Send the byte
            }

            bit_idx++;
        }
    }

    return EXIT_FAILURE;
}

int huffman_master() {
    return EXIT_FAILURE;
}

int huffman_worker() {
    return EXIT_FAILURE;
}

int huffman_compress(MPI_File in_fp, MPI_File out_fp, int size, int rank) {
    MPI_Offset file_size;
    MPI_File_get_size(in_fp, &file_size);

    int is_master = rank == 0;

    int ec = EXIT_SUCCESS;

    uint8_t* buf;
    uint32_t local_freqs[256];
    if (is_master) {
        buf = NULL;
        memset(local_freqs, 0, 256 * sizeof(uint32_t));
    } else {
        int worker_size = size - 1; // remove master, it will unfortunately wait
        int worker_rank = rank - 1; // same
    
        uint32_t chunk_size = file_size / worker_size;
        uint32_t remainder = file_size % worker_size;

        MPI_Offset process_offset = worker_rank * chunk_size;
        if (worker_rank == worker_size - 1) {
            chunk_size += remainder;
        }

        buf = malloc(chunk_size);
        if (MPI_File_read_at(
            in_fp, 
            process_offset, 
            buf, 
            chunk_size, 
            MPI_BYTE, 
            MPI_STATUS_IGNORE
        ) != MPI_SUCCESS) {
            ec = EXIT_FAILURE;
        }

        count_freqs(local_freqs, buf, chunk_size);
    }

    uint32_t global_freqs[256];
    if (MPI_Reduce(local_freqs, global_freqs, 256, MPI_UINT32_T, MPI_SUM, 0, MPI_COMM_WORLD) != MPI_SUCCESS) {
        return EXIT_FAILURE;
    }

    if (is_master) {
        MPI_File_write(out_fp, global_freqs, 256, MPI_UINT32_T, MPI_STATUS_IGNORE);
    }

    char codebook[256][256] = {0};
    if (is_master) {
        if (MPI_File_write(out_fp, global_freqs, 256, MPI_UINT32_T, MPI_STATUS_IGNORE) != MPI_SUCCESS) {
            return EXIT_FAILURE;
        }

        huffman_node* root = create_huffman_tree(global_freqs);

        char curr_code[256];
        generate_codebook(codebook, root, curr_code, 0);
    }

    if (MPI_Bcast(codebook, 256 * 256, MPI_CHAR, 0, MPI_COMM_WORLD) != MPI_SUCCESS) {
        return EXIT_FAILURE;
    }
    
    if (is_master) {
        // workers can only work with the buffers they read for counting without a reread
        // thus master can only recieve whole buffer
    } else {
        // ec = encode(codebook, in_fp, out_fp, chunk_size, process_offset);
    }

    free(buf);
    return ec;
}

int huffman_decompress(MPI_File in_fp, MPI_File out_fp) {
    return EXIT_FAILURE;
}

