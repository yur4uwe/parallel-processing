#include <stdint.h>
#include <stdio.h>
#include <string.h>

void count_freq(uint32_t freqs[256], FILE* fp) {
    memset(freqs, 0, 256 * sizeof(uint32_t));
    int byte;
    while ((byte = fgetc(fp)) != EOF) {
        freqs[(uint8_t)byte]++;
    }
}

void huffman(FILE* in_fp, FILE* out_fp) {
    uint32_t freqs[256];
    count_freq(freqs, in_fp);
}
