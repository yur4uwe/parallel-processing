#include "consts.h"
#include "aes-utils.h"
#include <omp.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int aes_ctr_process(FILE* in_fp, FILE* out_fp, const uint8_t key[AES_KEY_SIZE], const uint8_t nonce[AES_NONCE_SIZE]) {
    fprintf(stderr, "[PARALLEL] Starting aes_ctr_parallel\n");

    if (!key || !nonce) {
        fprintf(stderr, "Invalid key or nonce\n");
        return FAILURE;
    }

    // Get current file position - main.c has already read/written the nonce
    // For encryption: out_fp is at position 12 (after nonce write)
    // For decryption: in_fp is at position 12 (after nonce read)
    long in_offset = ftell(in_fp);
    long out_offset = ftell(out_fp);

    if (in_offset < 0 || out_offset < 0) {
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
    size_t data_size = st.st_size - in_offset;
    #pragma GCC diagnostic pop

    // Empty files are valid
    if (data_size == 0) {
        fprintf(stderr, "[PARALLEL] Empty file, nothing to process\n");
        return SUCCESS;
    }

    int ec = SUCCESS;

    uint8_t* per_round_keys = (uint8_t*)malloc(AES_ROUND_KEY_SIZE);
    if (!per_round_keys) {
        fprintf(stderr, "Memory allocation failed\n");
        return FAILURE;
    }

    key_schedule(key, per_round_keys);

    uint8_t baseline_state[AES_BLOCK_SIZE];
    memset(baseline_state, 0, AES_BLOCK_SIZE);
    for (int i = 0; i < AES_NONCE_SIZE; i++) {
        baseline_state[i] = nonce[i];
    }

    fprintf(stderr, "[PARALLEL] Entering parallel region\n");
    fflush(stderr);

    uint32_t total_blocks = ceil_div(data_size, AES_BLOCK_SIZE);
    uint8_t num_threads = omp_get_max_threads();
    uint32_t blocks_per_thread = ceil_div(total_blocks, num_threads);

    int in_fd = fileno(in_fp);
    int out_fd = fileno(out_fp);

    PDEBUG("About to read from file %d", in_fd);

    #pragma omp parallel shared(per_round_keys, baseline_state, ec, in_offset, out_offset) firstprivate(in_fd, out_fd)
    {
        int tid = omp_get_thread_num();
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wsign-conversion"
        uint32_t block_start = tid * blocks_per_thread;
        uint32_t block_end = min_u32(total_blocks, (tid + 1) * blocks_per_thread);
        #pragma GCC diagnostic pop

        if (block_start >= total_blocks)
            goto thread_exit;

        // Calculate offset relative to data start (after nonce)
        uint32_t bytes_offset = block_start * AES_BLOCK_SIZE;
        uint32_t bytes_to_read = (block_end - block_start) * AES_BLOCK_SIZE;

        PDEBUG("About to allocate %u bytes", bytes_to_read);

        uint8_t* thread_buffer = (uint8_t*)malloc(bytes_to_read);
        if (thread_buffer == NULL) {
            #pragma omp atomic write
            ec = FAILURE;
            goto thread_exit;
        }

        PDEBUG("About to read %u bytes from %d file", bytes_to_read, in_fd);

        // Add in_offset to skip over nonce that was already read
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wsign-conversion"
        ssize_t bytes_read = pread(in_fd, thread_buffer, bytes_to_read, in_offset + bytes_offset);
        #pragma GCC diagnostic pop
        if (bytes_read < 0) {
            fprintf(stderr, "[THREAD %d] pread failed\n", tid);
            #pragma omp atomic write
            ec = FAILURE;
            free(thread_buffer);
            goto thread_exit;
        }

        PDEBUG("Read %ld bytes from %d file, starting encryption", bytes_read, in_fd);

        // encryption will be here
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wsign-conversion"
        uint32_t blocks_to_encrypt = ceil_div(bytes_read, AES_BLOCK_SIZE);
        #pragma GCC diagnostic pop
        for (uint32_t i = 0; i < blocks_to_encrypt; i++) {
            uint8_t state[AES_BLOCK_SIZE];
            uint32_t block_counter = block_start + i;

            memcpy(state, baseline_state, AES_BLOCK_SIZE);
            state[AES_BLOCK_SIZE - 4] = (block_counter >> 24) & 0xFF;
            state[AES_BLOCK_SIZE - 3] = (block_counter >> 16) & 0xFF;
            state[AES_BLOCK_SIZE - 2] = (block_counter >> 8) & 0xFF;
            state[AES_BLOCK_SIZE - 1] = block_counter & 0xFF;

            aes_block_encrypt(state, (const uint8_t*)per_round_keys);
            size_t bytes_to_xor = AES_BLOCK_SIZE;
            #pragma GCC diagnostic push
            #pragma GCC diagnostic ignored "-Wsign-conversion"
            if ((size_t)bytes_read - i * AES_BLOCK_SIZE < AES_BLOCK_SIZE) {
                bytes_to_xor = bytes_read - i * AES_BLOCK_SIZE;
            }
            #pragma GCC diagnostic pop
            for (size_t j = 0; j < bytes_to_xor; j++) {
                thread_buffer[i * AES_BLOCK_SIZE + j] ^= state[j];
            }
        }

        PDEBUG("About to write to file %d", out_fd);

        // pwrite is atomic and thread-safe for non-overlapping regions
        // Add out_offset to write after nonce that was already written
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wsign-conversion"
        ssize_t written = pwrite(out_fd, thread_buffer, bytes_read, out_offset + bytes_offset);
        #pragma GCC diagnostic pop
        if (written < 0) {
            fprintf(stderr, "[THREAD %d] pwrite failed\n", tid);
            #pragma omp atomic write
            ec = FAILURE;
            free(thread_buffer);
            goto thread_exit;
        }

        PDEBUG("Wrote %ld bytes to %d file", written, out_fd);

        free(thread_buffer);

    thread_exit:
        (void)0;
    }

    memset(per_round_keys, 0, AES_ROUND_KEY_SIZE);
    free(per_round_keys);
    memset(baseline_state, 0, AES_BLOCK_SIZE);
    return ec;
}
