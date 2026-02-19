#!/bin/bash
# Compile the binary

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

BINARY=aes_ctr

echo "=== Compiling AES-CTR ==="
FLAGS=" -Wall -Wextra -Wformat=2 -Wsign-conversion \
    -O2 -D_FORTIFY_SOURCE=2 -fstack-protector-strong \
    -lssl -lcrypto"

if [ "$1" == "parallel" ]; then
    FLAGS="$FLAGS -fopenmp"
fi

gcc aes.c main.c arg_parsing.c -o $BINARY $FLAGS
if [ $? -ne 0 ]; then
    echo -e "${RED}Failed to compile!${NC}"
    exit 1
fi
echo -e "${GREEN}Compiled successfully${NC}"
if [ "$1" == "parallel" ]; then
    echo "Note: To use multiple threads, set OMP_NUM_THREADS environment variable:"
    echo "  export OMP_NUM_THREADS=4   # or your CPU core count"
    echo "Then run: ./aes_ctr"
fi
echo ""
