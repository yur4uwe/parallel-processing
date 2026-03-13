#pragma once

enum {
	SUCCESS = 0,
	FAILURE = 1,
	HELP = 2,

	MODE_COMPRESS = 10,
	MODE_DECOMPRESS = 11,
};

#define DEBUG(fmt, ...) do { \
    fprintf(stderr, "\033[0;31m" fmt "\033[0m\n", ##__VA_ARGS__); \
    fflush(stderr); \
} while(0)
