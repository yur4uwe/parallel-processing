#include <stdio.h>
#include <stdint.h>

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

    uint32_t freqs[256];
    fread(freqs, sizeof(uint32_t), 256, fp);
    fclose(fp);

    for (int i = 0; i < 256; i++) {
        if (freqs[i] > 0) {
            printf("0x%02X ('%c'): %u\n", i, (i >= 32 && i < 127) ? i : '.', freqs[i]);
        }
    }

    return 0;
}
