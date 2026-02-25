#!/bin/bash

# Correctness test runner for AES-256-CTR (serial + parallel)
# Keeps artifacts for inspection in test_artifacts/<timestamp>/

set +e

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT_DIR" || exit 1

TS="$(date +%Y%m%d_%H%M%S)"
OUT_DIR="${ROOT_DIR}/test_artifacts/${TS}"
mkdir -p "$OUT_DIR"

PASS=0
FAIL=0
TOTAL=0

log() { printf "%s\n" "$*"; }

run_cmd() {
    # usage: run_cmd <desc> <expected_exit> <cmd...>
    local desc="$1"
    local expected="$2"
    shift 2

    TOTAL=$((TOTAL + 1))
    "$@" >/dev/null 2>&1
    local rc=$?
    if [ "$rc" -eq "$expected" ]; then
        PASS=$((PASS + 1))
        log "PASS: $desc"
    else
        FAIL=$((FAIL + 1))
        log "FAIL: $desc (expected exit $expected, got $rc)"
    fi
}

run_cmp() {
    # usage: run_cmp <desc> <file_a> <file_b>
    local desc="$1"
    local a="$2"
    local b="$3"

    TOTAL=$((TOTAL + 1))
    if cmp -s "$a" "$b"; then
        PASS=$((PASS + 1))
        log "PASS: $desc"
    else
        FAIL=$((FAIL + 1))
        log "FAIL: $desc (files differ)"
        log "  Compare: cmp "$a" "$b""
    fi
}

run_cmp_mismatch() {
    # usage: run_cmp_mismatch <desc> <file_a> <file_b>
    local desc="$1"
    local a="$2"
    local b="$3"

    TOTAL=$((TOTAL + 1))
    if cmp -s "$a" "$b"; then
        FAIL=$((FAIL + 1))
        log "FAIL: $desc (files unexpectedly match)"
    else
        PASS=$((PASS + 1))
        log "PASS: $desc"
    fi
}

log "=== Building all versions ==="
./build.sh all >/dev/null 2>&1
if [ $? -ne 0 ]; then
    log "FAIL: build all"
    exit 1
fi

# Copy binaries with descriptive names for testing
VARIANTS="serial parallel parallel-for parallel-task"
for variant in $VARIANTS; do
    if [ ! -f "bin/aes-ctr-${variant}" ]; then
        log "FAIL: missing binary for $variant"
        exit 1
    fi
done

KEY_OK="correct-key"
KEY_BAD="wrong-key"

INPUT_SMALL="$OUT_DIR/input_small.txt"
INPUT_EMPTY="$OUT_DIR/input_empty.bin"
INPUT_ODD="$OUT_DIR/input_odd.bin"
INPUT_MED="$OUT_DIR/input_med.bin"

printf "hello aes ctr\n" > "$INPUT_SMALL"
: > "$INPUT_EMPTY"
printf "0123456789abcdef012" > "$INPUT_ODD"  # 19 bytes, not a multiple of 16
# 256KB random input for medium test
DD_COUNT=256
DD_BS=1024
(dd if=/dev/urandom of="$INPUT_MED" bs=$DD_BS count=$DD_COUNT 2>/dev/null)

log "=== Negative/argument tests ==="
run_cmd "help flag (serial)" 0 ./bin/aes-ctr-serial -h
run_cmd "missing key" 1 ./bin/aes-ctr-serial "$INPUT_SMALL" "$OUT_DIR/out.enc" -e
run_cmd "empty key" 1 ./bin/aes-ctr-serial "$INPUT_SMALL" "$OUT_DIR/out.enc" -e -k ""
run_cmd "missing output file" 1 ./bin/aes-ctr-serial "$INPUT_SMALL" -e -k "$KEY_OK"
run_cmd "missing input file" 1 ./bin/aes-ctr-serial "$OUT_DIR/no_such_file" "$OUT_DIR/out.enc" -e -k "$KEY_OK"

log "=== Round-trip tests (serial) ==="
run_cmd "serial encrypt small" 0 ./bin/aes-ctr-serial "$INPUT_SMALL" "$OUT_DIR/small_s.enc" -e -k "$KEY_OK"
run_cmd "serial decrypt small" 0 ./bin/aes-ctr-serial "$OUT_DIR/small_s.enc" "$OUT_DIR/small_s.dec" -d -k "$KEY_OK"
run_cmp "serial small round-trip" "$INPUT_SMALL" "$OUT_DIR/small_s.dec"

run_cmd "serial encrypt odd" 0 ./bin/aes-ctr-serial "$INPUT_ODD" "$OUT_DIR/odd_s.enc" -e -k "$KEY_OK"
run_cmd "serial decrypt odd" 0 ./bin/aes-ctr-serial "$OUT_DIR/odd_s.enc" "$OUT_DIR/odd_s.dec" -d -k "$KEY_OK"
run_cmp "serial odd round-trip" "$INPUT_ODD" "$OUT_DIR/odd_s.dec"

run_cmd "serial encrypt empty" 0 ./bin/aes-ctr-serial "$INPUT_EMPTY" "$OUT_DIR/empty_s.enc" -e -k "$KEY_OK"
run_cmd "serial decrypt empty" 0 ./bin/aes-ctr-serial "$OUT_DIR/empty_s.enc" "$OUT_DIR/empty_s.dec" -d -k "$KEY_OK"
run_cmp "serial empty round-trip" "$INPUT_EMPTY" "$OUT_DIR/empty_s.dec"

log "=== Round-trip tests (all parallel variants) ==="
for variant in parallel parallel-for parallel-task; do
    log "--- Testing $variant ---"
    run_cmd "${variant} encrypt small" 0 ./bin/aes-ctr-${variant} "$INPUT_SMALL" "$OUT_DIR/small_${variant}.enc" -e -k "$KEY_OK"
    run_cmd "${variant} decrypt small" 0 ./bin/aes-ctr-${variant} "$OUT_DIR/small_${variant}.enc" "$OUT_DIR/small_${variant}.dec" -d -k "$KEY_OK"
    run_cmp "${variant} small round-trip" "$INPUT_SMALL" "$OUT_DIR/small_${variant}.dec"

    run_cmd "${variant} encrypt medium" 0 ./bin/aes-ctr-${variant} "$INPUT_MED" "$OUT_DIR/med_${variant}.enc" -e -k "$KEY_OK"
    run_cmd "${variant} decrypt medium" 0 ./bin/aes-ctr-${variant} "$OUT_DIR/med_${variant}.enc" "$OUT_DIR/med_${variant}.dec" -d -k "$KEY_OK"
    run_cmp "${variant} medium round-trip" "$INPUT_MED" "$OUT_DIR/med_${variant}.dec"

    run_cmd "${variant} encrypt odd" 0 ./bin/aes-ctr-${variant} "$INPUT_ODD" "$OUT_DIR/odd_${variant}.enc" -e -k "$KEY_OK"
    run_cmd "${variant} decrypt odd" 0 ./bin/aes-ctr-${variant} "$OUT_DIR/odd_${variant}.enc" "$OUT_DIR/odd_${variant}.dec" -d -k "$KEY_OK"
    run_cmp "${variant} odd round-trip" "$INPUT_ODD" "$OUT_DIR/odd_${variant}.dec"
done

log "=== Cross-compat tests ==="
# Serial encrypt, each parallel variant decrypt
for variant in parallel parallel-for parallel-task; do
    run_cmd "serial encrypt -> ${variant} decrypt" 0 ./bin/aes-ctr-serial "$INPUT_MED" "$OUT_DIR/med_s_to_${variant}.enc" -e -k "$KEY_OK"
    run_cmd "${variant} decrypt serial output" 0 ./bin/aes-ctr-${variant} "$OUT_DIR/med_s_to_${variant}.enc" "$OUT_DIR/med_s_to_${variant}.dec" -d -k "$KEY_OK"
    run_cmp "cross round-trip (s->${variant})" "$INPUT_MED" "$OUT_DIR/med_s_to_${variant}.dec"
done

# Each parallel variant encrypt, serial decrypt
for variant in parallel parallel-for parallel-task; do
    run_cmd "${variant} encrypt -> serial decrypt" 0 ./bin/aes-ctr-${variant} "$INPUT_MED" "$OUT_DIR/med_${variant}_to_s.enc" -e -k "$KEY_OK"
    run_cmd "serial decrypt ${variant} output" 0 ./bin/aes-ctr-serial "$OUT_DIR/med_${variant}_to_s.enc" "$OUT_DIR/med_${variant}_to_s.dec" -d -k "$KEY_OK"
    run_cmp "cross round-trip (${variant}->s)" "$INPUT_MED" "$OUT_DIR/med_${variant}_to_s.dec"
done

log "=== Wrong key check ==="
run_cmd "decrypt with wrong key" 0 ./bin/aes-ctr-parallel "$OUT_DIR/med_s_to_parallel.enc" "$OUT_DIR/med_wrong.dec" -d -k "$KEY_BAD"
run_cmp_mismatch "wrong key should not match" "$INPUT_MED" "$OUT_DIR/med_wrong.dec"

log ""
log "================================"
log "TEST RESULTS"
log "Total:  $TOTAL"
log "Passed: $PASS"
log "Failed: $FAIL"
log "Artifacts: $OUT_DIR"
log "================================"

if [ "$FAIL" -eq 0 ]; then
    rm -rf "$OUT_DIR"
    ./cleanup.sh
    exit 0
else
    exit 1
fi
