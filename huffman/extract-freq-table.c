#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "consts.h"

typedef struct __attribute__((packed)) {
    uint32_t freq;
    uint8_t symbol;
} freq_entry;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s [OPTIONS...] <compressed_file>\n", argv[0]);
        fprintf(stderr, "  --show-chunk-sizes  shows size of each chunk");
        return 1;
    }

    bool show_chunk_lengths = false;

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--show-chunk-sizes") == EXIT_SUCCESS) {
            show_chunk_lengths = true;
        }
    }

    FILE* fp = fopen(argv[1], "rb");
    if (fp == NULL) {
        perror("Failed to open file");
        return 1;
    }

    uint32_t len;
    if (fread(&len, sizeof(uint32_t), 1, fp) != 1) {
        fprintf(stderr,
                "Error: Could not read table length (File too small or "
                "corrupt).\n");
        fclose(fp);
        return 1;
    }

    // 1. Check Maximum Length Bound (Huffman table cannot have > 256 symbols)
    if (len == 0 || len > 256) {
        fprintf(
            stderr,
            "Error: Malformed table length (%u). Must be between 1 and 256.\n",
            len);
        fclose(fp);
        return 1;
    }

    bool seen[256] = {false};
    printf("Detected Frequency Table (Length: %u):\n", len);

    for (uint32_t i = 0; i < len; i++) {
        freq_entry ent;
        if (fread(&ent, sizeof(freq_entry), 1, fp) != 1) {
            fprintf(stderr,
                    "\nError: Unexpected EOF while reading entry %u. Table is "
                    "truncated.\n",
                    i);
            fclose(fp);
            return 1;
        }

        // 2. Check Symbol Uniqueness (Each symbol must be unique in a Huffman
        // table)
        if (seen[ent.symbol]) {
            fprintf(stderr,
                    "\nError: Duplicate symbol 0x%02X found at entry %u. Table "
                    "is malformed.\n",
                    ent.symbol, i);
            fclose(fp);
            return 1;
        }

        // 3. Frequency Integrity (Optional Warning)
        if (ent.freq == 0) {
            fprintf(stderr, "  [Warning] Symbol 0x%02X has 0 frequency.\n",
                    ent.symbol);
        }

        seen[ent.symbol] = true;
        printf("  [%3u] 0x%02X ('%c'): %u\n", i, ent.symbol,
               (ent.symbol >= 32 && ent.symbol < 127) ? ent.symbol : '.',
               ent.freq);
    }

    // 4. Metadata Integrity: Attempt to read Chunk Count
    uint32_t chunk_count;
    if (fread(&chunk_count, sizeof(uint32_t), 1, fp) != 1) {
        fprintf(stderr,
                "\nWarning: File ends abruptly after frequency table. Missing "
                "Chunk Count.\n");
    } else {
        printf("\nMetadata: Chunk Count = %u\n", chunk_count);
    }

    uint32_t* chunk_sizes = malloc(chunk_count * sizeof(uint32_t));
    if (fread(chunk_sizes, sizeof(uint32_t), chunk_count, fp) != chunk_count) {
        fprintf(stderr, "\nWarning: File ends after chunk size table\n");
    }

    if (show_chunk_lengths) {
        printf("Chunk sizes:\n");
    } else {
        printf("Omitting chunk size output\n");
    }
    for (uint32_t i = 0; i < chunk_count; i++) {
        if (show_chunk_lengths) {
            printf("  [%3u] compressed len = %u\n", i, chunk_sizes[i]);
        }
        if (chunk_sizes[i] >= CHUNK_SIZE) {
            printf(
                "Warning: Chunk [%3u] is %u bytes and is bigger than "
                "uncompressed chunk",
                i, chunk_sizes[i]);
        }
    }

    fclose(fp);
    return 0;
}
