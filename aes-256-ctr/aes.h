#include <stdint.h>
#include <stdio.h>
#include "consts.h"

int aes_ctr_process(FILE* in_fp, FILE* out_fp, const uint8_t key[AES_KEY_SIZE], const uint8_t nonce[AES_NONCE_SIZE]);
