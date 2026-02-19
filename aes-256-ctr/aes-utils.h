#include "consts.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>

// Macro to get thread number - works with and without OpenMP
#ifdef _OPENMP
#define GET_THREAD_NUM() omp_get_thread_num()
#else
#define GET_THREAD_NUM() 0
#endif

#define DEBUG(...) fprintf(stderr, __VA_ARGS__); \
    fflush(stderr);

// Color codes for different threads
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[0;31m"
#define COLOR_GREEN   "\033[0;32m"
#define COLOR_BLUE    "\033[0;34m"
#define COLOR_MAGENTA "\033[0;35m"
#define COLOR_CYAN    "\033[0;36m"
#define COLOR_YELLOW  "\033[0;33m"

// Parallel Debug macro - automatically includes colors, thread number, and flush
#define PDEBUG(fmt, ...) do { \
    int tid = GET_THREAD_NUM(); \
    fprintf(stderr, "%s[PARALLEL T%d] " fmt "%s\n", get_thread_color(tid), tid, ##__VA_ARGS__, COLOR_RESET); \
    fflush(stderr); \
} while(0)

const char* get_thread_color(int tid);

int random_nonce(uint8_t* nonce, size_t size);

void key_schedule(const uint8_t* key, uint8_t* round_keys);

void aes_block_encrypt(uint8_t state[AES_BLOCK_SIZE], const uint8_t* round_keys);
