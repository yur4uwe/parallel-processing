#include "aes-ctr-core.h"
#include "aes-utils.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Context Management
 * ============================================================================ */

aes_ctr_context* aes_ctr_context_create(const uint8_t key[AES_KEY_SIZE], 
                                        const uint8_t nonce[AES_NONCE_SIZE]) {
    if (!key || !nonce) {
        fprintf(stderr, "Invalid key or nonce\n");
        return NULL;
    }

    aes_ctr_context* ctx = (aes_ctr_context*)malloc(sizeof(aes_ctr_context));
    if (!ctx) {
        fprintf(stderr, "Failed to allocate context\n");
        return NULL;
    }

    ctx->round_keys = (uint8_t*)malloc(AES_ROUND_KEY_SIZE);
    if (!ctx->round_keys) {
        fprintf(stderr, "Memory allocation failed\n");
        free(ctx);
        return NULL;
    }

    // Generate round keys from master key
    key_schedule(key, ctx->round_keys);

    // Initialize baseline state with nonce
    memset(ctx->baseline_state, 0, AES_BLOCK_SIZE);
    for (int i = 0; i < AES_NONCE_SIZE; i++) {
        ctx->baseline_state[i] = nonce[i];
    }

    return ctx;
}

void aes_ctr_context_destroy(aes_ctr_context* ctx) {
    if (!ctx) return;
    
    if (ctx->round_keys) {
        memset(ctx->round_keys, 0, AES_ROUND_KEY_SIZE);
        free(ctx->round_keys);
    }
    
    memset(ctx->baseline_state, 0, AES_BLOCK_SIZE);
    free(ctx);
}

/* ============================================================================
 * Core Encryption Logic
 * ============================================================================ */

void aes_ctr_encrypt_buffer(uint8_t* buffer, size_t buffer_size, 
                            uint32_t block_offset, const aes_ctr_context* ctx) {
    if (!buffer || !ctx) return;

    uint32_t total_blocks = ceil_div(buffer_size, AES_BLOCK_SIZE);

    for (uint32_t i = 0; i < total_blocks; i++) {
        uint8_t state[AES_BLOCK_SIZE];
        uint32_t block_counter = block_offset + i;

        // Initialize state with nonce and counter
        memcpy(state, ctx->baseline_state, AES_BLOCK_SIZE);
        state[AES_BLOCK_SIZE - 4] = (block_counter >> 24) & 0xFF;
        state[AES_BLOCK_SIZE - 3] = (block_counter >> 16) & 0xFF;
        state[AES_BLOCK_SIZE - 2] = (block_counter >> 8) & 0xFF;
        state[AES_BLOCK_SIZE - 1] = block_counter & 0xFF;

        // Encrypt to generate keystream
        aes_block_encrypt(state, ctx->round_keys);

        // XOR with plaintext/ciphertext
        size_t offset = i * AES_BLOCK_SIZE;
        size_t bytes_to_xor = min_u32(buffer_size - offset, AES_BLOCK_SIZE);
        
        for (size_t j = 0; j < bytes_to_xor; j++) {
            buffer[offset + j] ^= state[j];
        }
    }
}

/* ============================================================================
 * Main Dispatcher
 * ============================================================================ */

int aes_ctr_process(FILE* in_fp, FILE* out_fp, const uint8_t key[AES_KEY_SIZE], 
                    const uint8_t nonce[AES_NONCE_SIZE], aes_ctr_process_fn implementation) {
    // Create context with setup
    aes_ctr_context* ctx = aes_ctr_context_create(key, nonce);
    if (!ctx) {
        return FAILURE;
    }

    // Call the appropriate implementation
    int result = implementation(in_fp, out_fp, ctx);

    // Cleanup
    aes_ctr_context_destroy(ctx);

    return result;
}
