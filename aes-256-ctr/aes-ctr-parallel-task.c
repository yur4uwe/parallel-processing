#include "consts.h"
#include "aes-utils.h"
#include <omp.h>

// ===== PIPELINED TASK-BASED APPROACH WITH TASK DEPENDENCIES =====
// Data structure for a work unit containing one encrypted block
typedef struct {
    uint8_t data[AES_BLOCK_SIZE];           // Encrypted block data
    uint32_t block_counter;                 // Block position counter
    size_t data_size;                       // Actual data size (may be < AES_BLOCK_SIZE for last block)
    int is_ready;                           // Flag: block is encrypted and ready to write
} EncryptedBlock;

// Queue structure to manage ordered output
typedef struct {
    EncryptedBlock* blocks;                 // Array of encrypted blocks
    uint32_t total_blocks;                  // Total blocks in batch
    uint32_t blocks_encrypted;              // Count of encrypted blocks
} OutputBatch;

static uint32_t ceil_div(size_t numerator, size_t denominator) {
    return (uint32_t)((numerator + denominator - 1) / denominator);
}

static uint32_t min_u32(uint32_t a, uint32_t b) {
    return a < b ? a : b;
}

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

    #pragma omp parallel shared(per_round_keys, baseline_state, global_block_counter, ec)
    #pragma omp single nowait
    {
        // ===== READER TASK: Sequentially read file and create encryption tasks =====
        uint8_t read_buffer[BUFFER_SIZE * AES_BLOCK_SIZE];

        PDEBUG("READER: Starting sequential read loop");

        while (ec == SUCCESS) {
            size_t read_bytes = fread(read_buffer, sizeof(uint8_t), BUFFER_SIZE * AES_BLOCK_SIZE, in_fp);

            if (read_bytes == 0) {
                PDEBUG("READER: EOF reached, total blocks: %u", global_block_counter);
                break;
            }

            uint32_t blocks_in_batch = ceil_div((uint32_t)read_bytes, AES_BLOCK_SIZE);
            PDEBUG("READER: Read %zu bytes into %u blocks", read_bytes, blocks_in_batch);

            // Allocate temporary batch storage for encrypted blocks
            OutputBatch batch;
            batch.blocks = (EncryptedBlock*)malloc(blocks_in_batch * sizeof(EncryptedBlock));
            batch.total_blocks = blocks_in_batch;
            batch.blocks_encrypted = 0;

            if (!batch.blocks) {
                fprintf(stderr, "Memory allocation failed for batch\n");
                ec = FAILURE;
                break;
            }

            // ===== CREATE ENCRYPTION TASKS =====
            // For each block in the batch, create an encryption task
            // Tasks can run in parallel
            for (uint32_t i = 0; i < blocks_in_batch; i++) {
                uint32_t current_block_idx = global_block_counter + i;
                size_t bytes_to_process = min_u32((uint32_t)(read_bytes - i * AES_BLOCK_SIZE), AES_BLOCK_SIZE);

                // Capture block data locally for the task
                uint8_t local_plaintext[AES_BLOCK_SIZE];
                memset(local_plaintext, 0, AES_BLOCK_SIZE);
                memcpy(local_plaintext, &read_buffer[i * AES_BLOCK_SIZE], bytes_to_process);

                // Create encryption task
                #pragma omp task shared(per_round_keys, baseline_state, batch, ec) firstprivate(current_block_idx, bytes_to_process, local_plaintext)
                {
                    if (ec == SUCCESS) {
                        PDEBUG("ENCRYPTOR: Starting encryption of block %u (size: %zu bytes)", current_block_idx, bytes_to_process);

                        // Generate keystream block
                        uint8_t state[AES_BLOCK_SIZE];
                        memcpy(state, baseline_state, AES_BLOCK_SIZE);

                        // Set counter value in state (last 4 bytes)
                        state[AES_BLOCK_SIZE - 4] = (current_block_idx >> 24) & 0xFF;
                        state[AES_BLOCK_SIZE - 3] = (current_block_idx >> 16) & 0xFF;
                        state[AES_BLOCK_SIZE - 2] = (current_block_idx >> 8) & 0xFF;
                        state[AES_BLOCK_SIZE - 1] = current_block_idx & 0xFF;

                        // Encrypt the keystream
                        aes_block_encrypt(state, (const uint8_t*)per_round_keys);

                        // Prepare output: copy keystream and XOR with plaintext
                        memcpy(batch.blocks[i].data, state, AES_BLOCK_SIZE);
                        batch.blocks[i].block_counter = current_block_idx;
                        batch.blocks[i].data_size = bytes_to_process;

                        for (size_t j = 0; j < bytes_to_process; j++) {
                            batch.blocks[i].data[j] ^= local_plaintext[j];
                        }

                        batch.blocks[i].is_ready = 1;

                        #pragma omp atomic
                        batch.blocks_encrypted++;

                        PDEBUG("ENCRYPTOR: Block %u encrypted and marked ready", current_block_idx);
                    }
                }
            }

            // ===== WAIT FOR ENCRYPTION TASKS TO COMPLETE =====
            // All encryption tasks for this batch must complete before writing
            #pragma omp taskwait

            // ===== WRITE ENCRYPTED BLOCKS IN ORDER =====
            if (ec == SUCCESS) {
                PDEBUG("WRITER: Batch complete, writing %u encrypted blocks to file", batch.total_blocks);

                for (uint32_t i = 0; i < batch.total_blocks; i++) {
                    if (batch.blocks[i].is_ready) {
                        size_t written = fwrite(
                            batch.blocks[i].data,
                            sizeof(uint8_t),
                            batch.blocks[i].data_size,
                            out_fp
                        );

                        if (written != batch.blocks[i].data_size) {
                            fprintf(stderr, "Error writing to output file\n");
                            ec = FAILURE;
                            break;
                        }

                        PDEBUG("WRITER: Wrote block %u (%zu bytes) to output",
                               batch.blocks[i].block_counter,
                               batch.blocks[i].data_size);
                    }
                }
            }

            // Cleanup batch
            free(batch.blocks);

            global_block_counter += blocks_in_batch;
        }

        PDEBUG("READER: Exiting read loop, all batches processed");

    } // End of #pragma omp single - implicit barrier here

    // Cleanup
    memset(per_round_keys, 0, AES_ROUND_KEY_SIZE);
    free(per_round_keys);
    memset(baseline_state, 0, AES_BLOCK_SIZE);

    fprintf(stderr, "[TASK PIPELINE] Processing complete (ec=%d)\n", ec);
    return ec;
}
