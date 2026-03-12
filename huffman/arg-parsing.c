
#include "arg-parsing.h"
#include "consts.h"
#include "arg-parsing.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <openssl/sha.h>

void print_help(char* bin) {
    printf("Usage: %s <input_file> <output_file>\n", bin);
}

int parse_args(huffman_args* parsed_args, int argc, char* argv[]) {
    parsed_args->in_path = NULL;
    parsed_args->out_path = NULL;

    if (argc < 2 || argc > 3) {
        print_help(argv[0]);
        return FAILURE;
    }

    int in_file_set = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == SUCCESS || strcmp(argv[i], "--help") == SUCCESS) {
            print_help(argv[0]);
            return HELP;
        }
        if (strcmp(argv[i], "-c") == SUCCESS || strcmp(argv[i], "--compress") == SUCCESS) {
            if (parsed_args->mode != 0) {
                printf("Program called with both modes, what NIGGA??");
                print_help(argv[0]);
                return HELP;
            }
            parsed_args->mode = MODE_COMPRESS;
        }
        if (strcmp(argv[i], "-d") == SUCCESS || strcmp(argv[i], "--decompress") == SUCCESS) {
            if (parsed_args->mode != 0) {
                printf("Program called with both modes, what NIGGA??");
                print_help(argv[0]);
                return HELP;
            }
            parsed_args->mode = MODE_DECOMPRESS;
        }

        if (in_file_set == 0) {
            parsed_args->in_path = argv[i];
            in_file_set = 1;
        } else if (in_file_set == 1) {
            parsed_args->out_path = argv[i];
            in_file_set = 2;
        }
    }

    if (in_file_set == 0) {
        printf("Error: Missing input file argument\n");
        return FAILURE;
    }

    if (in_file_set != 2) {
        printf("Error: Missing output file argument\n");
        return FAILURE;
    }

    return SUCCESS;
}
