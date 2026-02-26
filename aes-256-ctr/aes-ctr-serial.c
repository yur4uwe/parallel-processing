#include "aes-ctr-core.h"
#include "consts.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int aes_ctr_process_serial(FILE* in_fp, FILE* out_fp, const aes_ctr_context* ctx) {
    if (!ctx) {
        fprintf(stderr, "Invalid context\n");
        return FAILURE;
    }

    int ec = SUCCESS;
    uint32_t counter = 0;
    size_t read;
    uint8_t bytes[16];

    while ((read = fread(bytes, sizeof(uint8_t), 16, in_fp)) > 0) {
        // Use core encryption on this block
        aes_ctr_encrypt_buffer(bytes, read, counter, ctx);

        uint16_t written = fwrite(bytes, sizeof(uint8_t), read, out_fp);
        if (written != read) {
            fprintf(stderr, "Error writing to output file\n");
            ec = FAILURE;
            break;
        }

        counter += 1;
    }

    return ec;
}
