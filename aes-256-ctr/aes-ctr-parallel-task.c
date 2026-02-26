#include "consts.h"
#include "aes-utils.h"
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    uint8_t* data;
    size_t size;
    uint32_t block_start;
    uint32_t block_count;
} Chunk;

int aes_ctr_process(FILE* in_fp, FILE* out_fp, const uint8_t key[AES_KEY_SIZE], const uint8_t nonce[AES_NONCE_SIZE]) {
    fprintf(stderr, "[TASK PIPELINE] Starting pipelined task-based AES-CTR encryption\n");

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

    fprintf(stderr, "[TASK PIPELINE] Generating key schedule\n");
    key_schedule(key, per_round_keys);
    fprintf(stderr, "[TASK PIPELINE] Key schedule complete\n");

    uint8_t baseline_state[AES_BLOCK_SIZE];
    memset(baseline_state, 0, AES_BLOCK_SIZE);
    for (int i = 0; i < AES_NONCE_SIZE; i++) {
        baseline_state[i] = nonce[i];
    }

    fprintf(stderr, "[TASK PIPELINE] Entering parallel region with task-based pipeline\n");
    fflush(stderr);

    uint32_t global_block_counter = 0;

    #pragma omp parallel shared(per_round_keys, baseline_state, ec, global_block_counter)
    #pragma omp single
    {
        const uint32_t CHUNKS_PER_BATCH = 16;
        const uint32_t CHUNK_SIZE_BLOCKS = BUFFER_SIZE;
        const uint32_t CHUNK_SIZE_BYTES = CHUNK_SIZE_BLOCKS * AES_BLOCK_SIZE;

        fprintf(stderr, "[TASK] Processing with batch_size=%u, chunk_size=%u blocks\n",
                CHUNKS_PER_BATCH, CHUNK_SIZE_BLOCKS);

        while (ec == SUCCESS) {
            // ===== PHASE 1: READ BATCH =====
            // Allocate batch array on stack (bounded memory)
            Chunk batch[CHUNKS_PER_BATCH];
            uint32_t chunks_read = 0;

            for (uint32_t i = 0; i < CHUNKS_PER_BATCH; i++) {
                uint8_t* read_buffer = (uint8_t*)malloc(CHUNK_SIZE_BYTES);
                if (!read_buffer) {
                    fprintf(stderr, "[TASK] Failed to allocate read buffer\n");
                    ec = FAILURE;
                    break;
                }

                size_t read_bytes = fread(read_buffer, 1, CHUNK_SIZE_BYTES, in_fp);
                if (read_bytes == 0) {
                    free(read_buffer);
                    break;
                }

                // Setup chunk metadata
                batch[i].data = read_buffer;
                batch[i].size = read_bytes;
                batch[i].block_start = global_block_counter;
                batch[i].block_count = ceil_div(read_bytes, AES_BLOCK_SIZE);

                global_block_counter += batch[i].block_count;
                chunks_read++;
            }

            if (chunks_read == 0) {
                break;  // EOF reached
            }

            fprintf(stderr, "[TASK] Read batch: %u chunks (%u total blocks)\n",
                    chunks_read, global_block_counter);

            // ===== PHASE 2: ENCRYPT IN PARALLEL =====
            // Create one task per chunk - tasks execute in parallel on worker threads
            for (uint32_t i = 0; i < chunks_read; i++) {
                Chunk* c = &batch[i];

                #pragma omp task firstprivate(c) shared(per_round_keys, baseline_state, ec)
                {
                    if (ec == SUCCESS) {
                        PDEBUG("Task encrypting chunk: blocks %u-%u (%u blocks, %zu bytes)",
                               c->block_start, c->block_start + c->block_count - 1,
                               c->block_count, c->size);

                        // Encrypt all blocks in this chunk
                        for (uint32_t j = 0; j < c->block_count; j++) {
                            uint8_t state[AES_BLOCK_SIZE];
                            uint32_t block_counter = c->block_start + j;

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
                            size_t bytes_to_xor = min_u32(c->size - offset, AES_BLOCK_SIZE);

                            for (size_t k = 0; k < bytes_to_xor; k++) {
                                c->data[offset + k] ^= state[k];
                            }
                        }

                        PDEBUG("Task completed chunk: blocks %u-%u",
                               c->block_start, c->block_start + c->block_count - 1);
                    }
                }
            }

            // ===== PHASE 3: WAIT FOR ALL ENCRYPTIONS =====
            // Explicit synchronization point - ensures all tasks complete before writing
            #pragma omp taskwait

            fprintf(stderr, "[TASK] All encryption tasks complete, writing batch\n");

            // ===== PHASE 4: WRITE RESULTS IN ORDER =====
            // Sequential write ensures correct output order
            if (ec == SUCCESS) {
                for (uint32_t i = 0; i < chunks_read; i++) {
                    size_t written = fwrite(batch[i].data, 1, batch[i].size, out_fp);
                    if (written != batch[i].size) {
                        fprintf(stderr, "[TASK] Write failed for chunk %u\n", i);
                        ec = FAILURE;
                        break;
                    }

                    PDEBUG("Wrote chunk %u: %zu bytes", i, written);
                }
            }

            // ===== PHASE 5: CLEANUP BATCH =====
            for (uint32_t i = 0; i < chunks_read; i++) {
                free(batch[i].data);
            }

            fprintf(stderr, "[TASK] Batch complete, processed %u chunks\n\n", chunks_read);
        }

        fprintf(stderr, "[TASK] All batches processed, total blocks: %u\n", global_block_counter);
    }  // End of single region - implicit barrier

    // Cleanup
    memset(per_round_keys, 0, AES_ROUND_KEY_SIZE);
    free(per_round_keys);
    memset(baseline_state, 0, AES_BLOCK_SIZE);

    fprintf(stderr, "[TASK PIPELINE] Processing complete (ec=%d)\n", ec);
    return ec;
}
