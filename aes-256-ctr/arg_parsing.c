
#include "consts.h"
#include "arg_parsing.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <openssl/sha.h>

void print_help(char* bin) {
    printf("Usage: %s <input_file> <output_file> [...OPTIONS]\n", bin);
    printf("OPTIONS:\n");
    printf("  -h, --help        Display this help message\n");
    printf("  -e, --encrypt     Encrypt the input file\n");
    printf("  -d, --decrypt     Decrypt the input file\n");
    printf("  -k, --key string  Specify the encryption key\n");
}

int parse_args(aes_args* parsed_args, int argc, char* argv[]) {
    parsed_args->mode = 0;
    parsed_args->key = malloc(AES_KEY_SIZE);
    if (!parsed_args->key) {
        fprintf(stderr, "Error: Failed to allocate key memory\n");
        return FAILURE;
    }
    parsed_args->in_file = NULL;
    parsed_args->out_file = NULL;

    if (argc < 2) {
        print_help(argv[0]);
        return FAILURE;
    }

    int in_file_set = 0;
    char* password = NULL;  // Store pointer to password string

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == SUCCESS || strcmp(argv[i], "--help") == SUCCESS) {
            print_help(argv[0]);
            return HELP;
        }

        if (strcmp(argv[i], "-e") == SUCCESS || strcmp(argv[i], "--encrypt") == SUCCESS) {
            parsed_args->mode = MODE_ENCRYPT;
            continue;
        }
        if (strcmp(argv[i], "-d") == SUCCESS || strcmp(argv[i], "--decrypt") == SUCCESS) {
            parsed_args->mode = MODE_DECRYPT;
            continue;
        }
        if (strcmp(argv[i], "-k") == SUCCESS || strcmp(argv[i], "--key") == SUCCESS) {
            if (i + 1 >= argc) {
                printf("Error: Missing key argument\n");
                return FAILURE;
            }
            if (strlen(argv[i+1]) == 0) {
                printf("Error: Key argument is empty\n");
                return FAILURE;
            }
            password = argv[i + 1];  // Just store pointer to argv
            i++;
            continue;
        }

        // Positional arguments: input file and output file
        if (in_file_set == 0) {
            parsed_args->in_file = argv[i];  // Point to argv string
            in_file_set = 1;
        } else if (in_file_set == 1) {
            parsed_args->out_file = argv[i];  // Point to argv string
            in_file_set = 2;
        }
    }

    if (!password) {
        printf("Error: Missing '--key' argument\n");
        return FAILURE;
    }

    if (in_file_set == 0) {
        printf("Error: Missing input file argument\n");
        return FAILURE;
    }

    // Generate default output filename if not provided
    if (in_file_set != 2) {
        printf("Error: Missing output file argument\n");
        return FAILURE;
    }

    // Hash the password to get the key
    SHA256((const unsigned char*)password, strlen(password), parsed_args->key);

    return SUCCESS;
}
