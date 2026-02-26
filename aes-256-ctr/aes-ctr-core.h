#pragma once
#include "consts.h"
#include <stdint.h>
#include <stdio.h>

/* ============================================================================
 * AES-CTR Core Interface
 *
 * This header provides a unified interface for all AES-CTR implementations
 * by extracting common preparation, cleanup, and encryption logic.
 * ============================================================================ */

typedef struct {
    uint8_t* round_keys;           // Pre-computed round keys
    uint8_t baseline_state[AES_BLOCK_SIZE];  // Nonce-initialized state
} aes_ctr_context;

/* Initialize context with key schedule and baseline state */
aes_ctr_context* aes_ctr_context_create(const uint8_t key[AES_KEY_SIZE],
                                        const uint8_t nonce[AES_NONCE_SIZE]);

/* Securely free context and wipe sensitive data */
void aes_ctr_context_destroy(aes_ctr_context* ctx);

/*
 * Core encryption function: XORs a buffer in-place with AES-CTR keystream.
 *
 * Parameters:
 *   buffer: input/output buffer to encrypt/decrypt in-place
 *   buffer_size: number of bytes in buffer
 *   block_offset: which AES block to start from (for counter initialization)
 *   ctx: pre-initialized AES-CTR context
 */
void aes_ctr_encrypt_buffer(uint8_t* buffer, size_t buffer_size,
                            uint32_t block_offset, const aes_ctr_context* ctx);

/*
 * Process function typedef for different implementations.
 * Each implementation processes input from in_fp to out_fp using the given context.
 * Returns SUCCESS or FAILURE.
 */
typedef int (*aes_ctr_process_fn)(FILE* in_fp, FILE* out_fp, const aes_ctr_context* ctx);

/* Different processing implementations */
int aes_ctr_process_serial(FILE* in_fp, FILE* out_fp, const aes_ctr_context* ctx);
int aes_ctr_process_parallel_pread(FILE* in_fp, FILE* out_fp, const aes_ctr_context* ctx);
int aes_ctr_process_parallel_for(FILE* in_fp, FILE* out_fp, const aes_ctr_context* ctx);
int aes_ctr_process_parallel_task(FILE* in_fp, FILE* out_fp, const aes_ctr_context* ctx);

/*
 * Main dispatcher function that handles setup, variant selection, and cleanup.
 * This is the public interface that main.c calls.
 */
int aes_ctr_process(FILE* in_fp, FILE* out_fp, const uint8_t key[AES_KEY_SIZE],
                    const uint8_t nonce[AES_NONCE_SIZE], aes_ctr_process_fn implementation);
