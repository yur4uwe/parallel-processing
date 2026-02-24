#include "consts.h"
#include "aes-utils.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

int aes_ctr_process(FILE* in_fp, FILE* out_fp, const uint8_t key[AES_KEY_SIZE], const uint8_t nonce[AES_NONCE_SIZE]) {
    fprintf(stderr, "[PARALLEL] Starting aes_ctr_parallel\n");

    if (!key || !nonce) {
        fprintf(stderr, "Invalid key or nonce\n");
        return FAILURE;
    }

    struct stat st;
    if (fstat(fileno(in_fp), &st) != 0) {
        fprintf(stderr, "Failed to get file size\n");
        return FAILURE;
    }

    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wsign-conversion"
    uint32_t file_size = st.st_size;
    #pragma GCC diagnostic pop
    int ec = SUCCESS;

    uint8_t* per_round_keys = (uint8_t*)malloc(AES_ROUND_KEY_SIZE);
    if (!per_round_keys) {
        fprintf(stderr, "Memory allocation failed\n");
        return FAILURE;
    }

    fprintf(stderr, "[PARALLEL] Generating key schedule\n");
    key_schedule(key, per_round_keys);
    fprintf(stderr, "[PARALLEL] Key schedule complete\n");

    uint8_t baseline_state[AES_BLOCK_SIZE];
    memset(baseline_state, 0, AES_BLOCK_SIZE);
    for (int i = 0; i < AES_NONCE_SIZE; i++) {
        baseline_state[i] = nonce[i];
    }

    uint8_t* buffer = (uint8_t*)malloc((size_t)file_size);
    if (!buffer) {
        fprintf(stderr, "Input buffer allocation failed\n");
        memset(per_round_keys, 0, AES_ROUND_KEY_SIZE);
        free(per_round_keys);
        return FAILURE;
    }

    size_t read_bytes = fread(buffer, sizeof(uint8_t), (size_t)file_size, in_fp);
    if (read_bytes == 0) {
        fprintf(stderr, "Error reading input file\n");
        free(buffer);
        memset(per_round_keys, 0, AES_ROUND_KEY_SIZE);
        free(per_round_keys);
        return FAILURE;
    }

    uint32_t total_blocks = ceil_div(read_bytes, AES_BLOCK_SIZE);

    fprintf(stderr, "[PARALLEL] Encrypting %u blocks in-place\n", total_blocks);

    #pragma omp parallel for schedule(static) shared(buffer, per_round_keys, baseline_state, read_bytes)
    for (uint32_t i = 0; i < total_blocks; i++) {
        uint8_t state[AES_BLOCK_SIZE];
        uint32_t block_counter = i;

        memcpy(state, baseline_state, AES_BLOCK_SIZE);

        state[AES_BLOCK_SIZE - 4] = (block_counter >> 24) & 0xFF;
        state[AES_BLOCK_SIZE - 3] = (block_counter >> 16) & 0xFF;
        state[AES_BLOCK_SIZE - 2] = (block_counter >> 8) & 0xFF;
        state[AES_BLOCK_SIZE - 1] = block_counter & 0xFF;

        aes_block_encrypt(state, (const uint8_t*)per_round_keys);

        size_t bytes_to_xor = min_u32((uint32_t)(read_bytes - i * AES_BLOCK_SIZE), AES_BLOCK_SIZE);
        for (size_t j = 0; j < bytes_to_xor; j++) {
            buffer[i * AES_BLOCK_SIZE + j] ^= state[j];
        }
    }

    size_t written = fwrite(buffer, sizeof(uint8_t), read_bytes, out_fp);
    if (written != read_bytes) {
        fprintf(stderr, "Error writing to output file\n");
        ec = FAILURE;
    }

    memset(buffer, 0, read_bytes);
    free(buffer);

    memset(per_round_keys, 0, AES_ROUND_KEY_SIZE);
    free(per_round_keys);
    memset(baseline_state, 0, AES_BLOCK_SIZE);
    return ec;
}
