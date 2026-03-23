#!/bin/bash

set -e

# Color definitions
BLUE='\033[0;34m'
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

VARIANTS=(serial parallel)
UTILS=(gen_from_freq extract-freq-table)
BUILD_DIR="bin"

# Create bin directory if it doesn't exist
mkdir -p "$BUILD_DIR"

is_valid_variant() {
    if [ "$1" == "all" ] || [ "$1" == "" ]; then
        return 0
    fi

    for variant in "${VARIANTS[@]}"; do
        if [ "$variant" == "$1" ]; then
            return 0
        fi
    done

    for util in "${UTILS[@]}"; do
        if [ "$util" == "$1" ]; then
            return 0
        fi
    done

    return 1
}

# Main script logic
target="${1:-all}"
shift 1 || true # Consume target

# Optional: Set CHUNK_SIZE if provided as second argument
CUSTOM_CHUNK_SIZE=""
while [[ $# -gt 0 ]]; do
    case "$1" in
        --chunk-size)
            CUSTOM_CHUNK_SIZE="$2"
            shift 2
            ;;
        *)
            shift
            ;;
    esac
done

CFLAGS="-Wall -Wextra -O3"
if [ -n "$CUSTOM_CHUNK_SIZE" ]; then
    CFLAGS="$CFLAGS -DCHUNK_SIZE=$CUSTOM_CHUNK_SIZE"
    echo -e "${YELLOW}Using custom CHUNK_SIZE: $CUSTOM_CHUNK_SIZE${NC}"
fi

build_util() {
    local util="$1"
    local output

    case "$util" in
        gen_from_freq)
            output="$BUILD_DIR/gen_from_freq"
            ;;
        extract-freq-table)
            output="$BUILD_DIR/eft"
            ;;
        *)
            echo -e "${RED}Error: Unknown util '$util'${NC}"
            return 1
            ;;
    esac

    echo -e "${BLUE}Building $util utility...${NC}"

    if gcc $CFLAGS "$util.c" -o "$output" -lm 2>&1; then
        echo -e "${GREEN}✓ $util compiled successfully${NC}"
        echo -e "  ${YELLOW}Output: $output${NC}"
        return 0
    else
        echo -e "${RED}✗ Failed to compile $util${NC}"
        return 1
    fi
}

build() {
    local variant="$1"
    local main_src output

    case "$variant" in
        serial)
            output="$BUILD_DIR/huffman-$variant"
            main_src="smain.c"
            ;;
        parallel)
            main_src="pmain.c"
            output="$BUILD_DIR/huffman-parallel"
            ;;
        *)
            echo -e "${RED}Error: Unknown variant '$variant'${NC}"
            return 1
            ;;
    esac

    local sources="arg-parsing.c $main_src min-heap.c freq-table.c huffman-common.c $variant/freq-table.c $variant/huffman.c"

    echo -e "${BLUE}Building $variant huffman...${NC}"

    if mpicc $CFLAGS $sources -o "$output" -lm 2>&1; then
        echo -e "${GREEN}✓ $variant huffman compiled successfully${NC}"
        echo -e "  ${YELLOW}Output: $output${NC}"
        return 0
    else
        echo -e "${RED}✗ Failed to compile $variant huffman${NC}"
        return 1
    fi
}

build_all() {
    local failed=0

    build serial || ((failed++))
    build parallel || ((failed++))
    
    for util in "${UTILS[@]}"; do
        build_util "$util" || ((failed++))
    done

    if [ $failed -eq 0 ]; then
        echo -e "${GREEN}All builds completed successfully!${NC}"
        return 0
    else
        echo -e "${RED}$failed build(s) failed${NC}"
        return 1
    fi
}

clean() {
    echo -e "${BLUE}Cleaning build artifacts...${NC}"
    rm -f "$BUILD_DIR"/huffman-* "$BUILD_DIR"/gen_from_freq "$BUILD_DIR"/eft
    echo -e "${GREEN}✓ Cleaned${NC}"
}

show_usage() {
    cat << EOF
${BLUE}Huffman Algorithm Build Script${NC}

Usage: $0 [COMMAND] [OPTIONS]

Commands:
  all              Build all variants and utilities [default]
  serial           Build serial variant only
  parallel         Build parallel variant only
  utils            Build utility tools (gen_from_freq, eft)
  clean            Remove build artifacts
  help             Show this message

Options:
  --chunk-size VAL Set custom CHUNK_SIZE (in bytes)

Examples:
  $0                 # Build all variants
  $0 serial          # Build serial variant only
  $0 parallel --chunk-size 131072
  $0 clean           # Clean build artifacts

EOF
}

case "$target" in
    help|-h|--help)
        show_usage
        exit 0
        ;;
    clean)
        clean
        exit 0
        ;;
    serial)
        build serial
        ;;
    parallel)
        build parallel
        ;;
    utils)
        for util in "${UTILS[@]}"; do
            build_util "$util"
        done
        ;;
    all)
        build_all
        ;;
    *)
        echo -e "${RED}Error: Unknown command '$target'${NC}"
        show_usage
        exit 1
        ;;
esac
