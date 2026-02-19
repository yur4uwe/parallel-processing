#include "consts.h"
#include "aes-utils.h"
#include <omp.h>

struct WorkBuffer {
    uint8_t data[BUFFER_SIZE * AES_BLOCK_SIZE];
    size_t read_bytes;
    uint32_t block_count;
};

static uint32_t ceil_div(size_t numerator, size_t denominator) {
    return (uint32_t)((numerator + denominator - 1) / denominator);
}

static uint32_t min_u32(uint32_t a, uint32_t b) {
    return a < b ? a : b;
}

int aes_ctr_process(const uint8_t* key, const uint8_t* nonce, FILE* in_fp, FILE* out_fp) {
    fprintf(stderr, "[PARALLEL] Starting aes_ctr_parallel\n");

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

    fprintf(stderr, "[PARALLEL] Generating key schedule\n");
    key_schedule(key, per_round_keys);
    fprintf(stderr, "[PARALLEL] Key schedule complete\n");

    uint8_t baseline_state[AES_BLOCK_SIZE];
    memset(baseline_state, 0, AES_BLOCK_SIZE);
    for (int i = 0; i < AES_NONCE_SIZE; i++) {
        baseline_state[i] = nonce[i];
    }

    struct WorkBuffer buffer;
    memset(&buffer, 0, sizeof(buffer));

    uint32_t global_counter = 0;
    uint32_t batch_base = 0;
    int local_done = 0;

    fprintf(stderr, "[PARALLEL] Entering parallel region\n");
    fflush(stderr);

    #pragma omp parallel shared(per_round_keys, baseline_state, global_counter, batch_base, buffer, ec, local_done)
    {
        while (1) {
            PDEBUG("Loop iteration starting");

            // ===== PHASE 1: MASTER READS FROM FILE =====
            #pragma omp master
            {
                PDEBUG("PHASE 1: Master reading from file");

                buffer.read_bytes = fread(buffer.data, sizeof(uint8_t), BUFFER_SIZE * AES_BLOCK_SIZE, in_fp);
                PDEBUG("Read %zu bytes", buffer.read_bytes);

                if (buffer.read_bytes == 0) {
                    PDEBUG("EOF reached, setting done flag");
                    local_done = 1;
                    buffer.block_count = 0;
                } else {
                    buffer.block_count = ceil_div(buffer.read_bytes, AES_BLOCK_SIZE);
                    batch_base = global_counter;
                    global_counter += buffer.block_count;
                    PDEBUG("Block count: %u", buffer.block_count);
                }
            }

            PDEBUG("Waiting at BARRIER 1");
            #pragma omp barrier
            PDEBUG("Passed BARRIER 1");

            if (local_done || ec != SUCCESS) {
                break;
            }

            // ===== PHASE 2: WORKERS ENCRYPT BLOCKS USING OMP FOR =====
            PDEBUG("PHASE 2: Encrypting %u blocks with omp for", buffer.block_count);

            #pragma omp for schedule(static)
            for (uint32_t i = 0; i < buffer.block_count; i++) {
                uint8_t state[AES_BLOCK_SIZE];
                uint32_t block_counter;

                memcpy(state, baseline_state, AES_BLOCK_SIZE);

                block_counter = batch_base + i;

                state[AES_BLOCK_SIZE - 4] = (block_counter >> 24) & 0xFF;
                state[AES_BLOCK_SIZE - 3] = (block_counter >> 16) & 0xFF;
                state[AES_BLOCK_SIZE - 2] = (block_counter >> 8) & 0xFF;
                state[AES_BLOCK_SIZE - 1] = block_counter & 0xFF;

                aes_block_encrypt(state, (const uint8_t*)per_round_keys);
                size_t bytes_to_xor = min_u32((uint32_t)(buffer.read_bytes - i * AES_BLOCK_SIZE), AES_BLOCK_SIZE);

                for (size_t j = 0; j < bytes_to_xor; j++) {
                    buffer.data[i * AES_BLOCK_SIZE + j] ^= state[j];
                }
            }
            PDEBUG("All blocks encrypted (implicit barrier at end of for)");

            #pragma omp barrier

            // ===== PHASE 3: MASTER WRITES TO FILE =====
            #pragma omp master
            {
                PDEBUG("PHASE 3: Master writing %zu bytes to file", buffer.read_bytes);

                size_t written = fwrite(buffer.data, sizeof(uint8_t), buffer.read_bytes, out_fp);
                PDEBUG("Written %zu bytes", written);

                if (written != buffer.read_bytes) {
                    fprintf(stderr, "Error writing to output file\n");
                    ec = FAILURE;
                }
            }

            #pragma omp barrier
        }
        PDEBUG("Exiting parallel region");
    }

    memset(per_round_keys, 0, AES_ROUND_KEY_SIZE);
    free(per_round_keys);
    memset(baseline_state, 0, AES_BLOCK_SIZE);
    return ec;
}
