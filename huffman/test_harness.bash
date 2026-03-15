#!/bin/bash

set -e

# Color definitions
BLUE='\033[0;34m'
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

# Configuration
BIN_DIR="bin"
TEST_DIR="test"
TEMP_DIR=$(mktemp -d)
VARIANTS=(serial)
# Note: parallel variant requires mpirun and is tested separately

cleanup() {
    rm -rf "$TEMP_DIR"
}
trap cleanup EXIT

print_header() {
    echo -e "\n${CYAN}=== $1 ===${NC}\n"
}

print_test_result() {
    local name="$1"
    local status="$2"
    local details="$3"

    if [ "$status" = "PASS" ]; then
        echo -e "${GREEN}✓${NC} $name"
    else
        echo -e "${RED}✗${NC} $name"
    fi
    [ -n "$details" ] && echo -e "  ${YELLOW}$details${NC}"
}

# Test 1: Compression correctness (encode)
test_encode_correctness() {
    print_header "Compression Correctness Tests (Encode)"
    local passed=0
    local failed=0

    for variant in "${VARIANTS[@]}"; do
        local binary="$BIN_DIR/huffman-$variant"
        [ ! -f "$binary" ] && { echo -e "${RED}Binary not found: $binary${NC}"; failed=$((failed+1)); continue; }

        for input in "$TEST_DIR"/input-encode/*.txt; do
            local filename=$(basename "$input")
            local expected="$TEST_DIR/expected-encode/${filename%.txt}.huff"
            local output="$TEMP_DIR/${filename%.txt}.huff"

            if $binary -c "$input" "$output" > /dev/null 2>&1; then
                if cmp -s "$output" "$expected"; then
                    print_test_result "$variant: $filename" "PASS"
                    passed=$((passed+1))
                else
                    print_test_result "$variant: $filename" "FAIL" "Output differs from expected"
                    failed=$((failed+1))
                fi
            else
                print_test_result "$variant: $filename" "FAIL" "Compression failed"
                failed=$((failed+1))
            fi
        done
    done

    echo -e "\n${CYAN}Encode Results: ${GREEN}$passed passed${NC}, ${RED}$failed failed${NC}"
    return $((failed > 0 ? 1 : 0))
}

# Test 2: Decompression correctness (decode)
test_decode_correctness() {
    print_header "Decompression Correctness Tests (Decode)"
    local passed=0
    local failed=0

    for variant in "${VARIANTS[@]}"; do
        local binary="$BIN_DIR/huffman-$variant"
        [ ! -f "$binary" ] && { echo -e "${RED}Binary not found: $binary${NC}"; failed=$((failed+1)); continue; }

        for input in "$TEST_DIR"/input-decode/*.huff; do
            local filename=$(basename "$input")
            local expected="$TEST_DIR/expected-decode/${filename%.huff}.huff.txt"
            local output="$TEMP_DIR/${filename%.huff}.txt"

            if $binary -d "$input" "$output" > /dev/null 2>&1; then
                if cmp -s "$output" "$expected"; then
                    print_test_result "$variant: $filename" "PASS"
                    passed=$((passed+1))
                else
                    print_test_result "$variant: $filename" "FAIL" "Output differs from expected"
                    failed=$((failed+1))
                fi
            else
                print_test_result "$variant: $filename" "FAIL" "Decompression failed"
                failed=$((failed+1))
            fi
        done
    done

    echo -e "\n${CYAN}Decode Results: ${GREEN}$passed passed${NC}, ${RED}$failed failed${NC}"
    return $((failed > 0 ? 1 : 0))
}

# Test 3: Performance comparison (encode)
test_encode_performance() {
    print_header "Performance Tests (Encode)"

    for input in "$TEST_DIR"/input-encode/*.txt; do
        local filename=$(basename "$input")
        local input_size=$(stat -f%z "$input" 2>/dev/null || stat -c%s "$input")
        
        echo -e "${BLUE}$filename ($(numfmt --to=iec-i --suffix=B $input_size 2>/dev/null || echo "$input_size bytes"))${NC}"

        for variant in "${VARIANTS[@]}"; do
            local binary="$BIN_DIR/huffman-$variant"
            [ ! -f "$binary" ] && continue

            local output="$TEMP_DIR/${filename%.txt}.huff"
            local total_time=0
            local iterations=5

            for i in $(seq 1 $iterations); do
                local start=$(date +%s%N)
                $binary -c "$input" "$output" > /dev/null 2>&1
                local end=$(date +%s%N)
                local elapsed=$(( (end - start) / 1000000 ))
                total_time=$((total_time + elapsed))
            done

            local avg_time=$((total_time / iterations))
            echo -e "  $variant: ${YELLOW}${avg_time}ms${NC} (avg over $iterations runs)"
        done
        echo ""
    done
}

# Test 4: Compression ratio
test_compression_ratio() {
    print_header "Compression Ratio Analysis"

    for variant in "${VARIANTS[@]}"; do
        local binary="$BIN_DIR/huffman-$variant"
        [ ! -f "$binary" ] && continue

        echo -e "${BLUE}$variant:${NC}"

        for input in "$TEST_DIR"/input-encode/*.txt; do
            local filename=$(basename "$input")
            local input_size=$(stat -f%z "$input" 2>/dev/null || stat -c%s "$input")
            local output="$TEMP_DIR/${filename%.txt}.huff"

            if $binary -c "$input" "$output" > /dev/null 2>&1; then
                local output_size=$(stat -f%z "$output" 2>/dev/null || stat -c%s "$output")
                local ratio=$(awk "BEGIN {printf \"%.2f\", ($output_size / $input_size) * 100}")
                local reduction=$(awk "BEGIN {printf \"%.2f\", 100 - ($output_size / $input_size) * 100}")
                
                echo -e "  $filename:"
                echo -e "    Original: $input_size bytes"
                echo -e "    Compressed: $output_size bytes"
                echo -e "    Ratio: ${YELLOW}${ratio}%${NC} (${GREEN}${reduction}% reduction${NC})"
            fi
        done
        echo ""
    done
}

# Main logic
show_usage() {
    cat << EOF
${BLUE}Huffman Compression Test Harness${NC}

Usage: $0 [COMMAND]

Commands:
  all               Run all tests (correctness, performance, ratio)
  correctness       Run correctness tests (encode + decode)
  encode-correct    Test compression correctness only
  decode-correct    Test decompression correctness only
  performance       Test compression performance
  ratio             Show compression ratio analysis
  mpi-tests         Run tests with MPI (parallel version)
  help              Show this message

Examples:
  $0                 # Run all tests
  $0 correctness    # Run correctness tests
  $0 performance    # Test performance

EOF
}

target="${1:-all}"

case "$target" in
    help|-h|--help)
        show_usage
        exit 0
        ;;
    encode-correct)
        test_encode_correctness
        ;;
    decode-correct)
        test_decode_correctness
        ;;
    correctness)
        test_encode_correctness
        test_decode_correctness
        ;;
    performance)
        test_encode_performance
        ;;
    ratio)
        test_compression_ratio
        ;;
    all)
        test_encode_correctness
        test_decode_correctness
        test_encode_performance
        test_compression_ratio
        ;;
    mpi-tests)
        if ! command -v mpirun &> /dev/null; then
            echo -e "${RED}Error: mpirun not found. Please install MPI.${NC}"
            exit 1
        fi
        local binary="$BIN_DIR/huffman-parallel"
        if [ ! -f "$binary" ]; then
            echo -e "${RED}Error: $binary not found${NC}"
            exit 1
        fi
        print_header "MPI Parallel Tests"
        echo -e "${YELLOW}Running parallel variant with mpirun...${NC}\n"
        for input in "$TEST_DIR"/input-encode/*.txt; do
            local filename=$(basename "$input")
            local output="$TEMP_DIR/${filename%.txt}.huff"
            echo "Testing: $filename"
            if mpirun -n 2 "$binary" -c "$input" "$output" 2>&1; then
                echo -e "${GREEN}✓ Compression succeeded${NC}\n"
            else
                echo -e "${RED}✗ Compression failed${NC}\n"
            fi
        done
        ;;
    *)
        echo -e "${RED}Error: Unknown command '$target'${NC}"
        show_usage
        exit 1
        ;;
esac
