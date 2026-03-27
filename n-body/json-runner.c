#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "./json/json.h"

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: %s <json-file-to-parse>", argv[0]);
        return EXIT_FAILURE;
    }

    FILE* json_file = fopen(argv[1], "rb");

    uint64_t file_size = -1;
    fseek(json_file, 0, SEEK_END);
    file_size = ftell(json_file);
    fseek(json_file, 0, SEEK_SET);

    uint8_t* json_str = malloc(sizeof(uint8_t) * file_size);

    json_value* val = parse_json(json_str);
    if (val == NULL) {
        return EXIT_FAILURE;
    } else {
        return EXIT_SUCCESS;
    }
}
