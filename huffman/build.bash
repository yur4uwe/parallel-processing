#!/bin/bash

set -e

# Color definitions
BLUE='\033[0;34m'
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

VARIANTS=(serial parallel)
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

    return 1
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

    local sources="arg-parsing.c $main_src min-heap.c freq-table.c $variant/freq-table.c $variant/huffman.c"

    echo -e "${BLUE}Building $variant huffman...${NC}"

    if mpicc -Wall -Wextra -O2 $sources -o "$output" -lm 2>&1; then
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
    rm -f "$BUILD_DIR"/huffman-*
    echo -e "${GREEN}✓ Cleaned${NC}"
}

show_usage() {
    cat << EOF
${BLUE}Huffman Algorithm Build Script${NC}

Usage: $0 [COMMAND] [OPTIONS]

Commands:
  all              Build all variants (serial, parallel) [default]
  serial           Build serial variant only
  parallel         Build parallel variant only
  clean            Remove build artifacts
  help             Show this message

Examples:
  $0                 # Build all variants
  $0 serial          # Build serial variant only
  $0 clean           # Clean build artifacts

EOF
}

# Main script logic
target="${1:-all}"

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
    all)
        build_all
        ;;
    *)
        echo -e "${RED}Error: Unknown command '$target'${NC}"
        show_usage
        exit 1
        ;;
esac
