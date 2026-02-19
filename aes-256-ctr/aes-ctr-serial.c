#include "consts.h"
#include "aes-utils.h"

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
