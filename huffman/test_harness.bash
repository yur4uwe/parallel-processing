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
GEN_DIR="$TEMP_DIR/generated"
VARIANTS=(serial parallel)
FAILED_DIR="failed_test"

NP_LIST=(2 3 4) # Default MPI processes to test
                # Should be less than 5 as then its oversubscribing
SAVE_ARTIFACTS=true # User requested default

# Parse flags and arguments
ARGS=()
while [[ $# -gt 0 ]]; do
    case "$1" in
        -n|--np)
            shift
            # Split the argument into array
            IFS=' ' read -r -a NP_LIST <<< "$1"
            shift
            ;;
        --no-save-failed)
            SAVE_ARTIFACTS=false
            shift
            ;;
        *)
            ARGS+=("$1")
            shift
            ;;
    esac
done

target="${ARGS[0]:-all}"

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
    if [ -n "$details" ]; then
        echo -e "  ${YELLOW}$details${NC}"
    fi
}

mkdir -p "$GEN_DIR"
if [ "$SAVE_ARTIFACTS" = true ]; then
    mkdir -p "$FAILED_DIR"
fi

# 1. Dynamic Test File Generation
generate_test_files() {
    print_header "Generating Test Files"
    
    # Empty file
    touch "$GEN_DIR/empty.txt"
    echo -e "${GREEN}✓${NC} Generated empty.txt"
    
    # Single byte
    echo -n "A" > "$GEN_DIR/single_byte.txt"
    echo -e "${GREEN}✓${NC} Generated single_byte.txt"
    
    # Repeated character (100KB)
    head -c 100000 /dev/zero | tr '\0' 'A' > "$GEN_DIR/repeated_100k.txt"
    echo -e "${GREEN}✓${NC} Generated repeated_100k.txt"
    
    # Exactly 1 chunk (65,536 bytes) - using random data for complexity
    head -c 65536 /dev/urandom > "$GEN_DIR/exactly_1_chunk.txt"
    echo -e "${GREEN}✓${NC} Generated exactly_1_chunk.txt"
    
    # 1 chunk + 1 byte (65,537 bytes)
    head -c 65537 /dev/urandom > "$GEN_DIR/1_chunk_plus_1.txt"
    echo -e "${GREEN}✓${NC} Generated 1_chunk_plus_1.txt"
    
    # Large random binary file (1MB)
    head -c 1048576 /dev/urandom > "$GEN_DIR/large_random_1MB.txt"
    echo -e "${GREEN}✓${NC} Generated large_random_1MB.txt"
    
    # Large text file (Book equivalent) ~ 2MB
    # We repeat a couple of sentences to get good compression
    local sentence="This is a test sentence designed to be repeated multiple times to simulate a text file with a decent compression ratio. It contains various characters and words. "
    local reps=15000 # ~ 2.4 MB
    awk -v s="$sentence" -v r="$reps" 'BEGIN {for(i=0;i<r;i++) printf "%s", s;}' > "$GEN_DIR/large_book_sim.txt"
    echo -e "${GREEN}✓${NC} Generated large_book_sim.txt"
    
    # Copy existing files to GEN_DIR for unified processing
    if [ -d "$TEST_DIR/input-encode" ]; then
        cp "$TEST_DIR"/input-encode/*.txt "$GEN_DIR/" 2>/dev/null || true
    fi
}

get_exec_cmd() {
    local variant="$1"
    local binary="$2"
    local np="$3"
    if [ "$variant" = "parallel" ]; then
        echo "mpirun -n $np $binary"
    else
        echo "$binary"
    fi
}

save_failure_artifacts() {
    local test_name="$1"
    local input="$2"
    local output_huff="$3"
    local output_dec="$4"
    
    if [ "$SAVE_ARTIFACTS" = true ]; then
        local base_name=$(basename "$input")
        local timestamp=$(date +%s)
        local dest_dir="$FAILED_DIR/${test_name}_${base_name}_${timestamp}"
        mkdir -p "$dest_dir"
        cp "$input" "$dest_dir/input_${base_name}"
        [ -f "$output_huff" ] && cp "$output_huff" "$dest_dir/compressed.huff"
        [ -f "$output_dec" ] && cp "$output_dec" "$dest_dir/decoded_${base_name}"
        echo -e "    ${RED}Failure artifacts saved to: ${dest_dir}${NC}"
    fi
}

run_roundtrip_test() {
    local input="$1"
    local variant="$2"
    local np="$3"
    
    local filename=$(basename "$input")
    local binary="$BIN_DIR/huffman-$variant"
    
    if [ ! -f "$binary" ]; then
        echo -e "${RED}Binary not found: $binary${NC}"
        return 1
    fi
    
    local exec_cmd=$(get_exec_cmd "$variant" "$binary" "$np")
    local output_huff="$TEMP_DIR/${variant}_${np}_${filename%.*}.huff"
    local output_dec="$TEMP_DIR/${variant}_${np}_${filename%.*}_dec.txt"
    
    local test_display_name="$variant"
    [ "$variant" = "parallel" ] && test_display_name="parallel (NP=$np)"
    
    # Encode
    if ! $exec_cmd -c "$input" "$output_huff" > /dev/null 2>&1; then
        print_test_result "$test_display_name: $filename" "FAIL" "Compression failed"
        save_failure_artifacts "${variant}_np${np}" "$input" "" ""
        return 1
    fi
    
    # Decode
    if ! $exec_cmd -d "$output_huff" "$output_dec" > /dev/null 2>&1; then
        print_test_result "$test_display_name: $filename" "FAIL" "Decompression failed"
        save_failure_artifacts "${variant}_np${np}" "$input" "$output_huff" ""
        return 1
    fi
    
    # Verify
    if cmp -s "$input" "$output_dec"; then
        print_test_result "$test_display_name: $filename" "PASS"
        return 0
    else
        print_test_result "$test_display_name: $filename" "FAIL" "Decoded output differs from original input"
        save_failure_artifacts "${variant}_np${np}" "$input" "$output_huff" "$output_dec"
        return 1
    fi
}

test_roundtrip() {
    print_header "Pure Round-Trip Correctness Tests"
    local passed=0
    local failed=0
    
    for input in "$GEN_DIR"/*.txt; do
        # Serial
        if run_roundtrip_test "$input" "serial" "1"; then
            passed=$((passed+1))
        else
            failed=$((failed+1))
        fi
        
        # Parallel (MPI Scaling)
        for np in "${NP_LIST[@]}"; do
            if [ "$np" -lt 2 ]; then
                echo -e "${YELLOW}Skipping parallel test for NP=$np (requires at least 2 processes)${NC}"
                continue
            fi
            if run_roundtrip_test "$input" "parallel" "$np"; then
                passed=$((passed+1))
            else
                failed=$((failed+1))
            fi
        done
    done
    
    echo -e "\n${CYAN}Round-Trip Results: ${GREEN}$passed passed${NC}, ${RED}$failed failed${NC}"
    [ $failed -eq 0 ]
}

test_compression_ratio() {
    print_header "Compression Ratio Analysis"
    
    local input="$GEN_DIR/large_book_sim.txt"
    [ ! -f "$input" ] && { echo -e "${RED}Error: large_book_sim.txt not found${NC}"; return 1; }
    local filename=$(basename "$input")
    local input_size=$(stat -c%s "$input" 2>/dev/null || stat -f%z "$input")
    
    echo -e "${BLUE}Target File: $filename (${input_size} bytes)${NC}"

    for variant in "${VARIANTS[@]}"; do
        local binary="$BIN_DIR/huffman-$variant"
        if [ ! -f "$binary" ]; then
            echo -e "${RED}Binary not found: $binary${NC}"
            continue
        fi

        local np=1
        if [ "$variant" = "parallel" ]; then
            # Find first NP >= 2
            for val in "${NP_LIST[@]}"; do
                if [ "$val" -ge 2 ]; then
                    np="$val"
                    break
                fi
            done
            if [ "$np" -lt 2 ]; then
                echo -e "${YELLOW}Skipping parallel ratio test (no NP >= 2 provided)${NC}"
                continue
            fi
        fi

        local exec_cmd=$(get_exec_cmd "$variant" "$binary" "$np")
        local output="$TEMP_DIR/${variant}_ratio_${filename%.txt}.huff"

        if $exec_cmd -c "$input" "$output" > /dev/null 2>&1; then
            local output_size=$(stat -c%s "$output" 2>/dev/null || stat -f%z "$output")
            local ratio=$(awk "BEGIN {printf \"%.2f\", ($output_size / $input_size) * 100}")
            local reduction=$(awk "BEGIN {printf \"%.2f\", 100 - ($output_size / $input_size) * 100}")
            
            echo -e "${BLUE}$variant (NP=$np):${NC}"
            echo -e "  Compressed: $output_size bytes"
            echo -e "  Ratio: ${YELLOW}${ratio}%${NC} (${GREEN}${reduction}% reduction${NC})"
        else
            echo -e "${RED}Compression failed for $variant${NC}"
        fi
    done
}

test_performance_csv() {
    print_header "Performance Tests (CSV Generation)"
    
    local csv_file="performance.csv"
    echo "File,Variant,NP,Encode Time (ms),Decode Time (ms)" > "$csv_file"
    
    for input in "$GEN_DIR"/*.txt; do
        local filename=$(basename "$input")
        echo "Testing $filename..."
        
        # Serial
        local s_bin="$BIN_DIR/huffman-serial"
        if [ -f "$s_bin" ]; then
            local s_out="$TEMP_DIR/perf_s_${filename}.huff"
            local s_dec="$TEMP_DIR/perf_s_${filename}_dec.txt"
            
            # Use date to get milliseconds timestamp, fallback to seconds if needed
            local s_start_c=$(date +%s%3N 2>/dev/null || date +%s)
            if $s_bin -c "$input" "$s_out" > /dev/null 2>&1; then
                local s_end_c=$(date +%s%3N 2>/dev/null || date +%s)
                local s_enc_time=$((s_end_c - s_start_c))
                
                local s_start_d=$(date +%s%3N 2>/dev/null || date +%s)
                if $s_bin -d "$s_out" "$s_dec" > /dev/null 2>&1; then
                    local s_end_d=$(date +%s%3N 2>/dev/null || date +%s)
                    local s_dec_time=$((s_end_d - s_start_d))
                    echo "$filename,serial,1,$s_enc_time,$s_dec_time" >> "$csv_file"
                else
                    echo "$filename,serial,1,$s_enc_time,FAIL" >> "$csv_file"
                fi
            else
                echo "$filename,serial,1,FAIL,FAIL" >> "$csv_file"
            fi
        fi
        
        # Parallel
        local p_bin="$BIN_DIR/huffman-parallel"
        if [ -f "$p_bin" ]; then
            for np in "${NP_LIST[@]}"; do
                if [ "$np" -lt 2 ]; then continue; fi
                local p_out="$TEMP_DIR/perf_p_${np}_${filename}.huff"
                local p_dec="$TEMP_DIR/perf_p_${np}_${filename}_dec.txt"
                local p_cmd="mpirun -n $np $p_bin"
                
                local p_start_c=$(date +%s%3N 2>/dev/null || date +%s)
                if $p_cmd -c "$input" "$p_out" > /dev/null 2>&1; then
                    local p_end_c=$(date +%s%3N 2>/dev/null || date +%s)
                    local p_enc_time=$((p_end_c - p_start_c))
                    
                    local p_start_d=$(date +%s%3N 2>/dev/null || date +%s)
                    if $p_cmd -d "$p_out" "$p_dec" > /dev/null 2>&1; then
                        local p_end_d=$(date +%s%3N 2>/dev/null || date +%s)
                        local p_dec_time=$((p_end_d - p_start_d))
                        echo "$filename,parallel,$np,$p_enc_time,$p_dec_time" >> "$csv_file"
                    else
                        echo "$filename,parallel,$np,$p_enc_time,FAIL" >> "$csv_file"
                    fi
                else
                    echo "$filename,parallel,$np,FAIL,FAIL" >> "$csv_file"
                fi
            done
        fi
    done
    echo -e "${GREEN}✓${NC} Performance results saved to ${CYAN}$csv_file${NC}"
}

show_usage() {
    cat << EOF
${BLUE}Huffman Compression Test Harness (Robust Pure Round-Trip)${NC}

Usage: $0 [COMMAND] [OPTIONS]

Commands:
  all               Run all tests (round-trip, ratio, performance)
  roundtrip         Run dynamic round-trip correctness tests
  ratio             Show compression ratio analysis on large text
  perf-csv          Run performance benchmark and generate performance.csv
  help              Show this message

Options:
  --no-save-failed  Do not save failed test artifacts to $FAILED_DIR/
  -n, --np <list>   Set MPI process counts to test (e.g., -n "2 4 8") default: "2 3 4 8"
                    Note: Parallel version requires at least 2 processes.

Examples:
  $0 roundtrip
  $0 all -n "2 4 8"
  $0 perf-csv
EOF
}

case "$target" in
    help|-h|--help)
        show_usage
        exit 0
        ;;
    roundtrip)
        generate_test_files
        test_roundtrip || exit 1
        ;;
    ratio)
        generate_test_files
        test_compression_ratio
        ;;
    perf-csv)
        generate_test_files
        test_performance_csv
        ;;
    all)
        generate_test_files
        test_roundtrip || true # Continue even if round-trip fails
        test_compression_ratio
        test_performance_csv
        ;;
    *)
        echo -e "${RED}Error: Unknown command '$target'${NC}"
        show_usage
        exit 1
        ;;
esac

