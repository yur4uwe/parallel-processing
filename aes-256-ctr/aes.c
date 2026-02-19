#include "consts.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>

// Macro to get thread number - works with and without OpenMP
#ifdef _OPENMP
#define GET_THREAD_NUM() omp_get_thread_num()
#else
#define GET_THREAD_NUM() 0
#endif

#define DEBUG(...) fprintf(stderr, __VA_ARGS__); \
    fflush(stderr);

// Color codes for different threads
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[0;31m"
#define COLOR_GREEN   "\033[0;32m"
#define COLOR_BLUE    "\033[0;34m"
#define COLOR_MAGENTA "\033[0;35m"
#define COLOR_CYAN    "\033[0;36m"
#define COLOR_YELLOW  "\033[0;33m"

// Function to get color code for current thread
static const char* get_thread_color(int tid) {
    switch (tid) {
        case 0: return COLOR_RED;
        case 1: return COLOR_GREEN;
        case 2: return COLOR_BLUE;
        case 3: return COLOR_MAGENTA;
        case 4: return COLOR_CYAN;
        case 5: return COLOR_YELLOW;
        default: return COLOR_RESET;
    }
}

// Parallel Debug macro - automatically includes colors, thread number, and flush
#define PDEBUG(fmt, ...) do { \
    int tid = GET_THREAD_NUM(); \
    fprintf(stderr, "%s[PARALLEL T%d] " fmt "%s\n", get_thread_color(tid), tid, ##__VA_ARGS__, COLOR_RESET); \
    fflush(stderr); \
} while(0)

int random_nonce(uint8_t* nonce, size_t size) {
    FILE* urandom = fopen("/dev/urandom", "rb");
    if (!urandom) {
        perror("Failed to open /dev/urandom");
        return FAILURE;
    }
    size_t read = fread(nonce, sizeof(uint8_t), size, urandom);
    fclose(urandom);
    return read == size ? SUCCESS : FAILURE;
}

static uint8_t s_box(uint8_t byte) {
    return SBOX[byte];
}

uint32_t bytes_to_word(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3) {
    return ((uint32_t)b0 << 24) | ((uint32_t)b1 << 16) | ((uint32_t)b2 << 8) | ((uint32_t)b3);
}

void word_to_bytes(uint32_t word, uint8_t* bytes) {
    bytes[0] = (word >> 24) & 0xFF;
    bytes[1] = (word >> 16) & 0xFF;
    bytes[2] = (word >> 8) & 0xFF;
    bytes[3] = word & 0xFF;
}

void sub_word(uint32_t* word) {
    uint8_t bytes[4];
    word_to_bytes(*word, bytes);
    bytes[0] = s_box(bytes[0]);
    bytes[1] = s_box(bytes[1]);
    bytes[2] = s_box(bytes[2]);
    bytes[3] = s_box(bytes[3]);
    *word = bytes_to_word(bytes[0], bytes[1], bytes[2], bytes[3]);
}

void rotate_word(uint32_t* word) {
    uint32_t temp = *word;
    *word = (temp << 8) | (temp >> 24);
}

void key_schedule(const uint8_t* key, uint8_t* round_keys) {
    int i = 0;
    uint32_t worded_round_keys[60];
    for (i = 0; i < AES_NK; i++) {
        worded_round_keys[i] = bytes_to_word(key[i * 4], key[i * 4 + 1], key[i * 4 + 2], key[i * 4 + 3]);
    }
    for (i = AES_NK; i < (AES_NR + 1) * 4; i++) {
        uint32_t temp = worded_round_keys[i - 1];
        if (i % AES_NK == 0) {
            rotate_word(&temp);
            sub_word(&temp);
            temp ^= ((uint32_t)(RCON[(i / AES_NK) - 1])) << 24;
        }
        if (i % AES_NK == 4) {
            sub_word(&temp);
        }
        worded_round_keys[i] = worded_round_keys[i - AES_NK] ^ temp;
    }

    for (i = 0; i < 60; i++) {
        word_to_bytes(worded_round_keys[i], &round_keys[i * 4]);
    }
}

void shift_rows(uint8_t state[AES_BLOCK_SIZE]) {
    uint8_t temp = state[1];
    state[1] = state[5];
    state[5] = state[9];
    state[9] = state[13];
    state[13] = temp;

    temp = state[2];
    state[2] = state[10];
    state[10] = state[14];
    state[14] = state[6];
    state[6] = temp;

    temp = state[3];
    state[3] = state[15];
    state[15] = state[11];
    state[11] = state[7];
    state[7] = temp;
}

uint8_t gmul2(uint8_t byte) {
    if (byte & 0x80)
        return (byte << 1) ^ 0x1b;
    else
        return byte << 1;
}

uint8_t gmul3(uint8_t byte) {
    return gmul2(byte) ^ byte;
}

void mix_columns(uint8_t* state) {
    // Process each column independently
    for (int col = 0; col < 4; col++) {
        // Get the 4 bytes in this column
        uint8_t s0 = state[0 + 4*col];  // row 0
        uint8_t s1 = state[1 + 4*col];  // row 1
        uint8_t s2 = state[2 + 4*col];  // row 2
        uint8_t s3 = state[3 + 4*col];  // row 3

        // Apply matrix multiplication in GF(2^8)
        state[0 + 4*col] = gmul2(s0) ^ gmul3(s1) ^ s2 ^ s3;
        state[1 + 4*col] = s0 ^ gmul2(s1) ^ gmul3(s2) ^ s3;
        state[2 + 4*col] = s0 ^ s1 ^ gmul2(s2) ^ gmul3(s3);
        state[3 + 4*col] = gmul3(s0) ^ s1 ^ s2 ^ gmul2(s3);
    }
}

void aes_block_encrypt(uint8_t state[AES_BLOCK_SIZE], const uint8_t* round_keys) {
    int i;
    int j;
    for (i = 0; i < AES_BLOCK_SIZE; i++) {
        state[i] ^= round_keys[i];
    }

    for (i = 0; i < AES_NR; i++) {
        // Sub bytes
        for (j = 0; j < AES_BLOCK_SIZE; j++) {
            state[j] = s_box(state[j]);
        }

        // shift rows
        shift_rows(state);

        // mix columns
        if (i < AES_NR - 1) {
            mix_columns(state);
        }

        // add round key
        for (j = 0; j < AES_BLOCK_SIZE; j++) {
            state[j] ^= round_keys[AES_BLOCK_SIZE * (i + 1) + j];
        }
    }
}

int aes_ctr_process(FILE* in_fp, FILE* out_fp, const uint8_t key[AES_KEY_SIZE], const uint8_t nonce[AES_NONCE_SIZE]) {
    if (!key || !nonce) {
        fprintf(stderr, "Invalid key or nonce\n");
        return FAILURE;
    }

    int ec = SUCCESS;

    uint8_t* per_round_keys = malloc(AES_ROUND_KEY_SIZE);
    if (!per_round_keys) {
        fprintf(stderr, "Memory allocation failed\n");
        return FAILURE;
    }

    key_schedule(key, per_round_keys);

    uint32_t counter = 0;

    uint8_t baseline_state[AES_BLOCK_SIZE];
    memset(baseline_state, 0, AES_BLOCK_SIZE);
    for (int i = 0; i < AES_NONCE_SIZE; i++) {
        baseline_state[i] = nonce[i];
    }

    size_t read;
    uint8_t bytes[16];
    while ((read = fread(bytes, sizeof(uint8_t), 16, in_fp)) > 0) {
        uint8_t state[AES_BLOCK_SIZE];
        memcpy(state, baseline_state, AES_BLOCK_SIZE);

        state[AES_BLOCK_SIZE - 4] = (counter >> 24) & 0xFF;
        state[AES_BLOCK_SIZE - 3] = (counter >> 16) & 0xFF;
        state[AES_BLOCK_SIZE - 2] = (counter >> 8) & 0xFF;
        state[AES_BLOCK_SIZE - 1] = counter & 0xFF;

        aes_block_encrypt(state, per_round_keys);

        for (size_t i = 0; i < read; i++) {
            bytes[i] ^= state[i];
        }

        uint16_t written = fwrite(bytes, sizeof(uint8_t), read, out_fp);
        if (written != read) {
            fprintf(stderr, "Error writing to output file\n");
            ec = FAILURE;
            goto cleanup;
        }

        counter += 1;
    }

cleanup:
    memset(per_round_keys, 0, AES_ROUND_KEY_SIZE);
    free(per_round_keys);
    return ec;
}

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

int aes_ctr_parallel(const uint8_t* key, const uint8_t* nonce, FILE* in_fp, FILE* out_fp) {
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
