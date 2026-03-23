#pragma once

#ifndef CHUNK_SIZE
#define CHUNK_SIZE (64 * 1024)
#endif

enum {
    SUCCESS = 0,
    FAILURE = 1,
    HELP = 2,

    MODE_COMPRESS = 10,
    MODE_DECOMPRESS = 11,
};

#ifdef MPI_VERSION
#include <string.h>
#define DEBUG_COMM(comm, fmt, ...)                                             \
    do {                                                                       \
        int rank, world_rank;                                                  \
        char comm_name[MPI_MAX_OBJECT_NAME];                                   \
        int name_len;                                                          \
        MPI_Comm_rank(comm, &rank);                                            \
        MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);                            \
        MPI_Comm_get_name(comm, comm_name, &name_len);                         \
        if (name_len == 0) strcpy(comm_name, "Comm");                          \
        fprintf(stderr,                                                        \
                "\033[0;31m[W:%d][%s:%d][%s:%d]\033[0m " fmt "\n",             \
                world_rank, comm_name, rank, __FILE__, __LINE__,               \
                ##__VA_ARGS__);                                                \
        fflush(stderr);                                                        \
    } while (0)
#define DEBUG(fmt, ...) DEBUG_COMM(MPI_COMM_WORLD, fmt, ##__VA_ARGS__)
#else
#define DEBUG(fmt, ...)                                                       \
    do {                                                                      \
        fprintf(stderr, "\033[0;31m[Serial][%s:%d]\033[0m " fmt "\n",         \
                __FILE__, __LINE__, ##__VA_ARGS__);                           \
        fflush(stderr);                                                       \
    } while (0)
#define DEBUG_COMM(comm, fmt, ...) DEBUG(fmt, ##__VA_ARGS__)
#endif
