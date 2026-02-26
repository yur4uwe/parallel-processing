#include "aes-ctr-core.h"
#include "aes-utils.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

int aes_ctr_process_parallel_for(FILE* in_fp, FILE* out_fp, const aes_ctr_context* ctx) {
    fprintf(stderr, "[PARALLEL] Starting aes_ctr_parallel_for\n");

    if (!ctx) {
        fprintf(stderr, "Invalid context\n");
        return FAILURE;
    }

    // Get current file position - main.c has already read/written the nonce
    long current_pos = ftell(in_fp);
    if (current_pos < 0) {
        fprintf(stderr, "Failed to get file position\n");
        return FAILURE;
    }

    struct stat st;
    if (fstat(fileno(in_fp), &st) != 0) {
        fprintf(stderr, "Failed to get file size\n");
        return FAILURE;
    }

    // Calculate actual data size (excluding nonce that was already read)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wsign-conversion"
    size_t data_size = st.st_size - current_pos;
    #pragma GCC diagnostic pop

    // Empty files are valid - just return success with no processing
    if (data_size == 0) {
        fprintf(stderr, "[PARALLEL] Empty file, nothing to process\n");
        return SUCCESS;
    }

    int ec = SUCCESS;

    uint8_t* buffer = (uint8_t*)malloc(data_size);
    if (!buffer) {
        fprintf(stderr, "Input buffer allocation failed\n");
        return FAILURE;
    }

    size_t read_bytes = fread(buffer, sizeof(uint8_t), data_size, in_fp);
    if (read_bytes != data_size) {
        fprintf(stderr, "Error reading input file\n");
        free(buffer);
        return FAILURE;
    }

    uint32_t total_blocks = ceil_div(read_bytes, AES_BLOCK_SIZE);

    fprintf(stderr, "[PARALLEL] Encrypting %u blocks in-place\n", total_blocks);

    // Use core encryption with OpenMP parallelization
    #pragma omp parallel for schedule(static) shared(buffer, ctx, read_bytes)
    for (uint32_t i = 0; i < total_blocks; i++) {
        uint8_t block_buffer[AES_BLOCK_SIZE];
        size_t offset = i * AES_BLOCK_SIZE;
        size_t bytes_to_process = (offset + AES_BLOCK_SIZE <= read_bytes) 
            ? AES_BLOCK_SIZE 
            : (read_bytes - offset);

        memcpy(block_buffer, &buffer[offset], bytes_to_process);
        aes_ctr_encrypt_buffer(block_buffer, bytes_to_process, i, ctx);
        memcpy(&buffer[offset], block_buffer, bytes_to_process);
    }

    size_t written = fwrite(buffer, sizeof(uint8_t), read_bytes, out_fp);
    if (written != read_bytes) {
        fprintf(stderr, "Error writing to output file\n");
        ec = FAILURE;
    }

    memset(buffer, 0, read_bytes);
    free(buffer);

    return ec;
}
