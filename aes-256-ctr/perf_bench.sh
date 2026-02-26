#!/bin/bash

# Performance benchmark for AES-256-CTR (serial vs parallel)
# Outputs CSV in results/ with nanosecond timings

set +e

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT_DIR" || exit 1

TS="$(date +%Y%m%d_%H%M%S)"
RESULTS_DIR="${ROOT_DIR}/results"
DATA_DIR="${ROOT_DIR}/perf_data"
CSV_FILE="${RESULTS_DIR}/perf_results_${TS}.csv"

mkdir -p "$RESULTS_DIR" "$DATA_DIR"

# Sizes are in KB to keep dd fast and predictable
SIZES_KB_DEFAULT="64 256 1024 4096 16384"
SIZES_KB="${SIZES_KB:-$SIZES_KB_DEFAULT}"
THREADS="${THREADS:-2 4 8 16}"
RUNS="${RUNS:-1}"
KEY="${KEY:-bench-key}"

now_ns() {
    local ns=$(date +%s%N 2>/dev/null)
    if [ -n "$ns" ] && [ "$ns" != "%N" ]; then
        printf "%s" "$ns"
        return
    fi
    python3 - <<'PY'
import time
print(time.time_ns())
PY
}

make_input() {
    local kb="$1"
    local file="$2"
    if [ -f "$file" ]; then
        return 0
    fi
    dd if=/dev/urandom of="$file" bs=1024 count="$kb" 2>/dev/null
}

log() { printf "%s\n" "$*"; }

log "=== Building all versions ==="
./build.sh all >/dev/null 2>&1
if [ $? -ne 0 ]; then
    log "FAIL: build all"
    exit 1
fi

# Verify all binaries exist
VARIANTS="serial parallel parallel-for parallel-task"
for variant in $VARIANTS; do
    if [ ! -f "bin/aes-ctr-${variant}" ]; then
        log "FAIL: missing binary for $variant"
        exit 1
    fi
done

log "All binaries built successfully"

# Collect results in associative arrays
declare -A results

# Build column headers
size_labels=""
for kb in $SIZES_KB; do
    size_labels="$size_labels size_${kb}kb"
done

# Run benchmarks and collect results
for kb in $SIZES_KB; do
    input_file="$DATA_DIR/input_${kb}kb.bin"
    make_input "$kb" "$input_file"

    # Serial runs
    for run in $(seq 1 "$RUNS"); do
        out_file="$DATA_DIR/out_serial_${kb}kb_run${run}.enc"
        start=$(now_ns)
        ./bin/aes-ctr-serial "$input_file" "$out_file" -e -k "$KEY" >/dev/null 2>&1
        end=$(now_ns)
        elapsed=$((end - start))

        key="serial_1_run${run}"
        results["${key}_${kb}"]="$elapsed"
    done

    # Parallel runs (for parallel, parallel-for, parallel-task variants)
    for variant in parallel parallel-for parallel-task; do
        for t in $THREADS; do
            for run in $(seq 1 "$RUNS"); do
                out_file="$DATA_DIR/out_${variant}_${kb}kb_t${t}_run${run}.enc"
                start=$(now_ns)
                OMP_NUM_THREADS="$t" ./bin/aes-ctr-${variant} "$input_file" "$out_file" -e -k "$KEY" >/dev/null 2>&1
                end=$(now_ns)
                elapsed=$((end - start))

                key="${variant}_${t}_run${run}"
                results["${key}_${kb}"]="$elapsed"
            done
        done
    done

    log "Completed size ${kb}KB"
done

# Write CSV header
{
    printf "timestamp,variant,threads"
    for kb in $SIZES_KB; do
        printf ",size_%skb" "$kb"
    done
    printf "\n"
} > "$CSV_FILE"

# Write results for serial (1 variant, 1 thread, multiple runs)
for run in $(seq 1 "$RUNS"); do
    {
        printf "%s,serial,1" "$TS"
        for kb in $SIZES_KB; do
            ns="${results[serial_1_run${run}_${kb}]}"
            ms=$((ns / 1000000))
            printf ",%s" "$ms"
        done
        printf "\n"
    } >> "$CSV_FILE"
done

# Write results for all parallel variants (multiple variants, multiple threads, multiple runs)
for variant in parallel parallel-for parallel-task; do
    for t in $THREADS; do
        for run in $(seq 1 "$RUNS"); do
            {
                printf "%s,%s,%s" "$TS" "$variant" "$t"
                for kb in $SIZES_KB; do
                    ns="${results[${variant}_${t}_run${run}_${kb}]}"
                    ms=$((ns / 1000000))
                    printf ",%s" "$ms"
                done
                printf "\n"
            } >> "$CSV_FILE"
        done
    done
done

log "Results written to: $CSV_FILE"
./cleanup.sh
rm -rf perf_data
