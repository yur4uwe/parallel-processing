#include "consts.h"
#include "aes-utils.h"
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#define PIPELINE_DEPTH 8
#define CHUNK_SIZE_BLOCKS 1024
#define CHUNK_SIZE_BYTES (CHUNK_SIZE_BLOCKS * AES_BLOCK_SIZE)

int aes_ctr_process(FILE* in_fp, FILE* out_fp, const uint8_t key[AES_KEY_SIZE], const uint8_t nonce[AES_NONCE_SIZE]) {
    fprintf(stderr, "[PIPELINED I/O] Starting pread/pwrite task-based AES-CTR encryption\n");

    if (!key || !nonce) {
        fprintf(stderr, "Invalid key or nonce\n");
        return FAILURE;
    }

    int ec = SUCCESS;

    uint8_t* per_round_keys = (uint8_t*)malloc(AES_ROUND_KEY_SIZE);
    if (!per_round_keys) {
        fprintf(stderr, "Memory allocation failed\n");
        return FAILURE;
    }

    fprintf(stderr, "[PIPELINED I/O] Generating key schedule\n");
    key_schedule(key, per_round_keys);

    uint8_t baseline_state[AES_BLOCK_SIZE];
    memset(baseline_state, 0, AES_BLOCK_SIZE);
    for (int i = 0; i < AES_NONCE_SIZE; i++) {
        baseline_state[i] = nonce[i];
    }

    int in_fd = fileno(in_fp);
    int out_fd = fileno(out_fp);

    off_t in_offset = ftell(in_fp);
    off_t out_offset = ftell(out_fp);

    struct stat in_stat;
    if (fstat(in_fd, &in_stat) != 0) {
        perror("fstat");
        free(per_round_keys);
        return FAILURE;
    }

    size_t data_size = in_stat.st_size - in_offset;
    if (data_size == 0) {
        fprintf(stderr, "[PIPELINED I/O] No data to process\n");
        free(per_round_keys);
        return SUCCESS;
    }

    size_t num_chunks = (data_size + CHUNK_SIZE_BYTES - 1) / CHUNK_SIZE_BYTES;

    fprintf(stderr, "[PIPELINED I/O] File size: %zu bytes, %zu chunks of %u bytes\n",
            data_size, num_chunks, CHUNK_SIZE_BYTES);
    fprintf(stderr, "[PIPELINED I/O] Pipeline depth: %d (memory footprint: %d KB)\n",
            PIPELINE_DEPTH, (PIPELINE_DEPTH * CHUNK_SIZE_BYTES) / 1024);

    // Allocate circular buffer array
    uint8_t** buffers = (uint8_t**)malloc(PIPELINE_DEPTH * sizeof(uint8_t*));
    if (!buffers) {
        fprintf(stderr, "Failed to allocate buffer array\n");
        free(per_round_keys);
        return FAILURE;
    }

    for (int i = 0; i < PIPELINE_DEPTH; i++) {
        buffers[i] = (uint8_t*)malloc(CHUNK_SIZE_BYTES);
        if (!buffers[i]) {
            fprintf(stderr, "Failed to allocate buffer %d\n", i);
            for (int j = 0; j < i; j++) {
                free(buffers[j]);
            }
            free(buffers);
            free(per_round_keys);
            return FAILURE;
        }
    }

    // Array to track actual bytes read per chunk
    size_t* chunk_sizes = (size_t*)malloc(num_chunks * sizeof(size_t));
    if (!chunk_sizes) {
        fprintf(stderr, "Failed to allocate chunk_sizes array\n");
        for (int i = 0; i < PIPELINE_DEPTH; i++) {
            free(buffers[i]);
        }
        free(buffers);
        free(per_round_keys);
        return FAILURE;
    }

    fprintf(stderr, "[PIPELINED I/O] Entering parallel region\n");
    fflush(stderr);

    double start_time = omp_get_wtime();

    #pragma omp parallel shared(buffers, per_round_keys, baseline_state, ec, chunk_sizes)
    #pragma omp single
    {
        fprintf(stderr, "[PIPELINED I/O] Starting pipeline with %d threads\n",
            omp_get_num_threads());

        for (size_t chunk = 0; chunk < num_chunks; chunk++) {
            size_t buf_idx = chunk % PIPELINE_DEPTH;
            off_t chunk_offset = in_offset + (chunk * CHUNK_SIZE_BYTES);
            size_t chunk_size = (chunk == num_chunks - 1)
                ? (data_size - chunk * CHUNK_SIZE_BYTES)
                : CHUNK_SIZE_BYTES;

            uint32_t block_start = chunk * CHUNK_SIZE_BLOCKS;
            uint32_t block_count = (chunk_size + AES_BLOCK_SIZE - 1) / AES_BLOCK_SIZE;

            // STAGE 1: READ TASK
            #pragma omp task if(ec == SUCCESS) depend(out: buffers[buf_idx]) \
                             firstprivate(chunk, buf_idx, chunk_offset, chunk_size)
            {
                ssize_t bytes_read = pread(in_fd, buffers[buf_idx], chunk_size, chunk_offset);

                if (bytes_read < 0) {
                    perror("pread");
                    #pragma omp atomic write
                    ec = FAILURE;
                } else if ((size_t)bytes_read != chunk_size) {
                    PDEBUG("[READ] Warning: read %zd bytes, expected %zu\n",
                            bytes_read, chunk_size);
                    chunk_sizes[chunk] = bytes_read;
                } else {
                    chunk_sizes[chunk] = chunk_size;
                }

                PDEBUG("[READ] Chunk %zu: read %zu bytes from offset %ld",
                       chunk, chunk_sizes[chunk], chunk_offset);
            }

            // STAGE 2: ENCRYPT TASK
            #pragma omp task if(ec == SUCCESS) depend(inout: buffers[buf_idx]) \
                             firstprivate(chunk, buf_idx, block_start, block_count) \
                             shared(per_round_keys, baseline_state, ec, chunk_sizes)
            {
                size_t bytes_to_process = chunk_sizes[chunk];

                PDEBUG("[ENCRYPT] Chunk %zu: processing %u blocks (%zu bytes)",
                       chunk, block_count, bytes_to_process);

                for (uint32_t j = 0; j < block_count; j++) {
                    uint8_t state[AES_BLOCK_SIZE];
                    uint32_t block_counter = block_start + j;

                    // Initialize counter state
                    memcpy(state, baseline_state, AES_BLOCK_SIZE);
                    state[AES_BLOCK_SIZE - 4] = (block_counter >> 24) & 0xFF;
                    state[AES_BLOCK_SIZE - 3] = (block_counter >> 16) & 0xFF;
                    state[AES_BLOCK_SIZE - 2] = (block_counter >> 8) & 0xFF;
                    state[AES_BLOCK_SIZE - 1] = block_counter & 0xFF;

                    // Generate keystream
                    aes_block_encrypt(state, (const uint8_t*)per_round_keys);

                    // XOR with plaintext
                    size_t offset = j * AES_BLOCK_SIZE;
                    size_t bytes_in_block = (offset + AES_BLOCK_SIZE <= bytes_to_process)
                        ? AES_BLOCK_SIZE
                        : (bytes_to_process - offset);

                    for (size_t k = 0; k < bytes_in_block; k++) {
                        buffers[buf_idx][offset + k] ^= state[k];
                    }
                }

                PDEBUG("[ENCRYPT] Chunk %zu: complete", chunk);
            }

            // STAGE 3: WRITE TASK
            #pragma omp task if(ec == SUCCESS) depend(in: buffers[buf_idx]) \
                             firstprivate(chunk, buf_idx, chunk_offset) \
                             shared(ec, chunk_sizes)
            {
                size_t bytes_to_write = chunk_sizes[chunk];
                off_t write_offset = out_offset + (chunk * CHUNK_SIZE_BYTES);

                ssize_t bytes_written = pwrite(out_fd, buffers[buf_idx],
                                               bytes_to_write, write_offset);

                if (bytes_written < 0) {
                    perror("pwrite");
                    #pragma omp atomic write
                    ec = FAILURE;
                } else if ((size_t)bytes_written != bytes_to_write) {
                    fprintf(stderr, "[WRITE] Error: wrote %zd bytes, expected %zu\n",
                            bytes_written, bytes_to_write);
                    #pragma omp atomic write
                    ec = FAILURE;
                }

                PDEBUG("[WRITE] Chunk %zu: wrote %zu bytes to offset %ld",
                       chunk, bytes_to_write, write_offset);
            }
        }

        PDEBUG("[PIPELINED I/O] All tasks spawned, waiting for completion...\n");
    }  // End of single region

    // Update file positions
    fseek(in_fp, in_offset + data_size, SEEK_SET);
    fseek(out_fp, out_offset + data_size, SEEK_SET);

    // Cleanup
    for (int i = 0; i < PIPELINE_DEPTH; i++) {
        free(buffers[i]);
    }
    free(buffers);
    free(chunk_sizes);

    memset(per_round_keys, 0, AES_ROUND_KEY_SIZE);
    free(per_round_keys);
    memset(baseline_state, 0, AES_BLOCK_SIZE);

    return ec;
}
