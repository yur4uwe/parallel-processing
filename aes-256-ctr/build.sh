#!/bin/bash
# Compile the binary

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

mkdir -p bin

MODES=("serial", "parallel-for", "parallel-task")

DEFAULT_MODE="serial"

SELECTED_MODE=$DEFAULT_MODE

is_valid_mode() {
    for mode in "${MODES[@]}"; do
        if [ "$mode" == "$1" ]; then
            return 0
        fi
    done
    return 1
}

if is_valid_mode "$1"; then
    echo -e "${GREEN}Selected mode: $1${NC}"
    SELECTED_MODE=$1
elif [ "$1" == "" ]; then
    echo -e "${YELLOW}No mode specified, using default: $DEFAULT_MODE${NC}"
else
    echo -e "${RED}Invalid mode: $1${NC}"
    exit 1
fi

BINARY=bin/aes-ctr-$SELECTED_MODE

echo "=== Compiling AES-CTR ==="
FLAGS=" -Wall -Wextra -Wformat=2 -Wsign-conversion \
    -O2 -D_FORTIFY_SOURCE=2 -fstack-protector-strong \
    -lssl -lcrypto"

if [[ "$SELECTED_MODE" == "parallel-for" || "$SELECTED_MODE" == "parallel-task" ]]; then
    FLAGS="$FLAGS -fopenmp"
fi

gcc aes-ctr-$SELECTED_MODE.c aes-utils.c main.c arg_parsing.c -o $BINARY $FLAGS
if [ $? -ne 0 ]; then
    echo -e "${RED}Failed to compile!${NC}"
    exit 1
fi
echo -e "${GREEN}Compiled successfully${NC}"
if [[ "$SELECTED_MODE" == "parallel-for" || "$SELECTED_MODE" == "parallel-task" ]]; then
    echo "Note: To use multiple threads, set OMP_NUM_THREADS environment variable:"
    echo "  export OMP_NUM_THREADS=4   # or your CPU core count"
    echo "Then run: ./aes_ctr"
fi
echo ""
