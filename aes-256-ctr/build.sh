#!/bin/bash
# Build AES-256-CTR variants: serial, parallel-pread, parallel-for, parallel-task
# Usage:
#   ./build.sh              # Build all variants (default)
#   ./build.sh serial       # Build only serial variant
#   ./build.sh parallel     # Build only parallel-pread variant
#   ./build.sh parallel-for # Build only parallel-for variant
#   ./build.sh parallel-task # Build only parallel-task variant
#   ./build.sh all          # Build all variants explicitly
#   ./build.sh clean        # Remove build artifacts

set -e  # Exit on error

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# All available modes
MODES=("serial" "parallel" "parallel-for" "parallel-task")

# Common source files (shared across all variants)
COMMON_SOURCES="aes-utils.c main.c arg_parsing.c aes-ctr-core.c"

# Compiler flags
BASE_FLAGS="-Wall -Wextra -Wformat=2 -Wsign-conversion -O2 -D_FORTIFY_SOURCE=2 -fstack-protector-strong"
LINK_FLAGS="-lssl -lcrypto"

# Function to print usage
print_usage() {
    echo "Usage: $0 [MODE]"
    echo ""
    echo "Modes:"
    echo "  (no arg or 'all')    Build all variants"
    echo "  serial               Build serial variant"
    echo "  parallel             Build parallel-pread variant"
    echo "  parallel-for         Build parallel-for variant"
    echo "  parallel-task        Build parallel-task variant"
    echo "  clean                Remove all build artifacts"
    echo ""
}

# Function to check if mode is valid
is_valid_mode() {
    if [ "$1" == "all" ] || [ "$1" == "" ]; then
        return 0
    fi
    for mode in "${MODES[@]}"; do
        if [ "$mode" == "$1" ]; then
            return 0
        fi
    done
    return 1
}

# Function to build a single variant
build_variant() {
    local mode=$1
    local flags="$BASE_FLAGS"
    local impl_define=""

    # Determine which implementation function to use
    case "$mode" in
        serial)           impl_define="-D AES_CTR_IMPL=aes_ctr_process_serial" ;;
        parallel)         impl_define="-D AES_CTR_IMPL=aes_ctr_process_parallel_pread" ;;
        parallel-for)     impl_define="-D AES_CTR_IMPL=aes_ctr_process_parallel_for" ;;
        parallel-task)    impl_define="-D AES_CTR_IMPL=aes_ctr_process_parallel_task" ;;
    esac

    # Add OpenMP for parallel variants
    if [[ "$mode" == "parallel" || "$mode" == "parallel-for" || "$mode" == "parallel-task" ]]; then
        flags="$flags -fopenmp"
    fi

    echo -e "${BLUE}Building aes-ctr-$mode...${NC}"
    gcc $flags $impl_define aes-ctr-$mode.c $COMMON_SOURCES -o bin/aes-ctr-$mode $LINK_FLAGS

    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✓ aes-ctr-$mode compiled successfully${NC}"
    else
        echo -e "${RED}✗ Failed to compile aes-ctr-$mode${NC}"
        return 1
    fi
}

# Function to clean build artifacts
clean_build() {
    echo -e "${YELLOW}Cleaning build artifacts...${NC}"
    rm -rf bin/aes-ctr-*
    echo -e "${GREEN}Clean complete${NC}"
}

# Parse arguments
SELECTED_MODE="${1:-all}"

# Handle special cases
if [ "$SELECTED_MODE" == "clean" ]; then
    clean_build
    exit 0
fi

if [ "$SELECTED_MODE" == "-h" ] || [ "$SELECTED_MODE" == "--help" ]; then
    print_usage
    exit 0
fi

# Validate mode
if ! is_valid_mode "$SELECTED_MODE"; then
    echo -e "${RED}Error: Invalid mode '$SELECTED_MODE'${NC}"
    print_usage
    exit 1
fi

# Create output directory
mkdir -p bin

# Determine which modes to build
if [ "$SELECTED_MODE" == "all" ] || [ "$SELECTED_MODE" == "" ]; then
    MODES_TO_BUILD=("${MODES[@]}")
    echo -e "${GREEN}Building all variants...${NC}"
else
    MODES_TO_BUILD=("$SELECTED_MODE")
    echo -e "${GREEN}Building $SELECTED_MODE variant...${NC}"
fi

echo ""

# Build each variant
BUILD_FAILED=0
for mode in "${MODES_TO_BUILD[@]}"; do
    build_variant "$mode"
    if [ $? -ne 0 ]; then
        BUILD_FAILED=1
    fi
done

echo ""

# Report results
if [ $BUILD_FAILED -eq 0 ]; then
    echo -e "${GREEN}=== Build Successful ===${NC}"
    echo ""
    echo "Available binaries:"
    for mode in "${MODES_TO_BUILD[@]}"; do
        if [ -f "bin/aes-ctr-$mode" ]; then
            echo -e "  ${BLUE}./bin/aes-ctr-$mode${NC}"
        fi
    done
    echo ""
    echo "Usage examples:"
    echo "  ./bin/aes-ctr-serial -e -k 'password' input.txt output.txt"
    echo "  ./bin/aes-ctr-parallel-for -d -k 'password' encrypted.txt decrypted.txt"
    echo ""
    if [[ "${MODES_TO_BUILD[@]}" =~ "parallel" ]] || [[ "${MODES_TO_BUILD[@]}" =~ "parallel-for" ]] || [[ "${MODES_TO_BUILD[@]}" =~ "parallel-task" ]]; then
        echo "For parallel variants, set thread count:"
        echo "  export OMP_NUM_THREADS=4  # or your CPU core count"
        echo ""
    fi
else
    echo -e "${RED}=== Build Failed ===${NC}"
    exit 1
fi
