#define _DEFAULT_SOURCE
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BUFFER_SIZE (1024 * 1024) // 1MB

int main(int argc, char** argv) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <src_file> <target_size_bytes> <output_file>\n", argv[0]);
        return 1;
    }

    const char* src_path = argv[1];
    const char* output_path = argv[3];

    errno = 0;
    char* endptr;
    unsigned long long target_size = strtoull(argv[2], &endptr, 10);
    if (*endptr != '\0') {
        fprintf(stderr, "Error: Invalid target size '%s'\n", argv[2]);
        return 1;
    }

    FILE* src_fp = fopen(src_path, "rb");
    if (src_fp == NULL) {
        perror("Error opening source file");
        return 1;
    }

    uint64_t freqs[256] = {0};
    uint64_t total_count = 0;
    uint8_t* buffer = malloc(BUFFER_SIZE);
    if (!buffer) {
        fprintf(stderr, "Error: failed to allocate memory for reading\n");
        fclose(src_fp);
        return 1;
    }

    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, src_fp)) > 0) {
        for (size_t i = 0; i < bytes_read; i++) {
            freqs[buffer[i]]++;
            total_count++;
        }
    }
    fclose(src_fp);

    if (total_count == 0) {
        fprintf(stderr, "Error: source file is empty\n");
        free(buffer);
        return 1;
    }

    // Build cumulative distribution
    struct {
        uint8_t symbol;
        uint64_t limit;
    } dist[256];
    int num_symbols = 0;
    uint64_t current_limit = 0;
    for (int i = 0; i < 256; i++) {
        if (freqs[i] > 0) {
            current_limit += freqs[i];
            dist[num_symbols].symbol = (uint8_t)i;
            dist[num_symbols].limit = current_limit;
            num_symbols++;
        }
    }

    FILE* output_fp = fopen(output_path, "wb");
    if (output_fp == NULL) {
        perror("Error opening output file");
        free(buffer);
        return 1;
    }

    // Use a better seed for randomness
    srand48(time(NULL));

    unsigned long long remaining = target_size;
    while (remaining > 0) {
        size_t to_gen = (remaining > BUFFER_SIZE) ? BUFFER_SIZE : (size_t)remaining;
        for (size_t i = 0; i < to_gen; i++) {
            // drand48 returns in range [0, 1)
            uint64_t r = (uint64_t)(drand48() * (double)total_count);
            
            // Binary search for the symbol
            int low = 0, high = num_symbols - 1;
            uint8_t selected = 0;
            while (low <= high) {
                int mid = low + (high - low) / 2;
                if (r < dist[mid].limit) {
                    selected = dist[mid].symbol;
                    high = mid - 1;
                } else {
                    low = mid + 1;
                }
            }
            buffer[i] = selected;
        }
        if (fwrite(buffer, 1, to_gen, output_fp) != to_gen) {
            perror("Error writing to output file");
            fclose(output_fp);
            free(buffer);
            return 1;
        }
        remaining -= to_gen;
    }

    fclose(output_fp);
    free(buffer);
    return 0;
}
