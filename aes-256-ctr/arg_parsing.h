
#pragma once
#include "consts.h"
#include <stdint.h>

typedef struct {
    char* in_file;
    char* out_file;
    uint8_t* key;
    uint8_t mode;
} aes_args;

int parse_args(aes_args* parsed_args, int argc, char *argv[]);
