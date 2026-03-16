#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct __attribute__((packed)) {
    uint32_t freq;
    uint8_t symbol;
} freq_entry;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: %s <compressed_file>\n", argv[0]);
        return 1;
    }

    FILE* fp = fopen(argv[1], "rb");
    if (fp == NULL) {
        printf("Failed to open file\n");
        return 1;
    }

    uint32_t len;
    if (fread(&len, sizeof(uint32_t), 1, fp) != 1) {
        printf("Failed to read frequency table size\n");
        fclose(fp);
        return 1;
    }

    for (uint32_t i = 0; i < len; i++) {
        freq_entry ent;
        if (fread(&ent, sizeof(freq_entry), 1, fp) != 1) {
            printf("Failed to read frequency entry %u\n", i);
            break;
        }
        printf("0x%02X ('%c'): %u\n", ent.symbol, (ent.symbol >= 32 && ent.symbol < 127) ? ent.symbol : '.', ent.freq);
    }

    fclose(fp);
    return 0;
}
