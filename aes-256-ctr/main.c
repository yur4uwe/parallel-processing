#include "consts.h"
#include "arg_parsing.h"
#include "aes-utils.h"
#include "aes.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
    int ec = SUCCESS;
    aes_args* parsed_args = malloc(sizeof(aes_args));
    ec = parse_args(parsed_args, argc, argv);
    if (ec == FAILURE) {
        return 1;
    } else if (ec == HELP) {
        return 0;
    }

    FILE* in_fp = fopen(parsed_args->in_file, "rb");
    if (in_fp == NULL) {
        fprintf(stderr, "Failed to open input file: %s\n", parsed_args->in_file);
        return 1;
    }

    fseek(in_fp, 0, SEEK_END);
    int64_t file_size = ftell(in_fp);
    fseek(in_fp, 0, SEEK_SET);

    if (file_size > (UINT32_MAX * (int64_t)AES_BLOCK_SIZE)) {
        fprintf(stderr, "File size exceeds maximum supported size of ~64GB\n");
        ec = FAILURE;
        goto cleanup_in;
    } else if (file_size <= 0) {
        fprintf(stderr, "Input file is empty\n");
        ec = FAILURE;
        goto cleanup_in;
    }

    FILE* out_fp = fopen(parsed_args->out_file, "wb");
    if (out_fp == NULL) {
        fprintf(stderr, "Failed to open output file: %s\n", parsed_args->out_file);
        ec = FAILURE;
        goto cleanup_in;
    }

    uint8_t nonce[AES_NONCE_SIZE];
    if (parsed_args->mode == MODE_ENCRYPT) {
        if (random_nonce(nonce, AES_NONCE_SIZE) != SUCCESS) {
            fprintf(stderr, "Failed to generate nonce\n");
            goto cleanup_out;
        }
        size_t written = fwrite(nonce, sizeof(uint8_t), AES_NONCE_SIZE, out_fp);
        if (written != AES_NONCE_SIZE) {
            fprintf(stderr, "Failed to write nonce to output file\n");
            ec = FAILURE;
            goto cleanup_out;
        }
    } else if (parsed_args->mode == MODE_DECRYPT) {
        if (fread(nonce, 1, AES_NONCE_SIZE, in_fp) != AES_NONCE_SIZE) {
            fprintf(stderr, "Failed to read nonce from input file\n");
            ec = FAILURE;
            goto cleanup_out;
        }
    } else {
        fprintf(stderr, "Invalid mode\n");
        ec = FAILURE;
        goto cleanup_out;
    }

    int aes_ec = aes_ctr_process(in_fp, out_fp, parsed_args->key, nonce);
    if (aes_ec != SUCCESS) {
        fprintf(stderr, "Failed to process file\n");
        ec = FAILURE;
    }

cleanup_out:
    fclose(out_fp);
cleanup_in:
    fclose(in_fp);
    memset(parsed_args->key, 0, AES_KEY_SIZE);
    free(parsed_args->key);
    free(parsed_args);
    return ec;
}
