#include "consts.h"
#include "aes-utils.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Function to get color code for current thread
const char* get_thread_color(int tid) {
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

uint32_t ceil_div(size_t numerator, size_t denominator) {
    return (uint32_t)((numerator + denominator - 1) / denominator);
}

uint32_t min_u32(uint32_t a, uint32_t b) {
    return a < b ? a : b;
}
