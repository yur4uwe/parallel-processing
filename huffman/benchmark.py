import subprocess
import os
import csv
import sys
import tempfile
import shutil
import time
from datetime import datetime

# Configurations
INPUT_SIZES_KB = [1, 10, 50, 250, 500, 750, 1 * 1024, 10 * 1024, 50 * 1024]
CHUNK_SIZES_KB = [
    1,
    2,
    4,
    8,
    16,
    64,
    256,
    1024,
]
PROCESS_COUNTS = [2, 3, 4]  # MAX 4 processes, no oversubscribe
ITERATIONS = 5

SRC_TEXT_FILE = "test/input-encode/test_big.txt"
BENCHMARK_DIR = "benchmarks"
os.makedirs(BENCHMARK_DIR, exist_ok=True)

timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
RESULTS_FILE = os.path.join(BENCHMARK_DIR, f"benchmark_{timestamp}.csv")
LATEST_FILE = os.path.join(BENCHMARK_DIR, "benchmark_results.csv")


def run_command(cmd: str):
    start_wall = time.perf_counter()
    result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
    end_wall = time.perf_counter()

    wall_time = end_wall - start_wall

    if result.returncode != 0:
        print(f"\n[ERROR] Command failed with exit code {result.returncode}")
        print(f"Command: {cmd}")
        print(f"Stderr: {result.stderr}")
        return None, wall_time
    return result.stdout, wall_time


def create_test_file(size_kb: int, path: str):
    print(f"Generating {size_kb}MB test file at {path} using {SRC_TEXT_FILE}...")
    size_bytes = size_kb * 1024
    cmd = f"bin/gen_from_freq {SRC_TEXT_FILE} {size_bytes} {path}"
    out, _ = run_command(cmd)
    if out is None:
        print("Failed to generate test file. Aborting.")
        sys.exit(1)


def build_utils():
    print("Building utility tools...")
    out, _ = run_command("./build.bash utils")
    if out is None:
        print("Failed to build utilities. Aborting.")
        sys.exit(1)


def build_all(chunk_size_kb: int):
    print(f"Building huffman (all) with CHUNK_SIZE={chunk_size_kb}K...")
    out, _ = run_command(f"./build.bash all --chunk-size {chunk_size_kb * 1024}")
    if out is None:
        print("Build failed. Aborting.")
        sys.exit(1)


def extract_time(output: str | None):
    if not output:
        return None
    for line in output.splitlines():
        if line.startswith("TIME:"):
            try:
                return float(line.split(":")[1].strip())
            except (TypeError, ValueError, OverflowError):
                return None
    return None


def benchmark():
    results: list[dict[str, int | float | str]] = []

    build_utils()

    # Create a unique temporary directory for this benchmark run
    tmp_dir = tempfile.mkdtemp(prefix="huffman_bench_")
    print(f"Using temporary directory for test files: {tmp_dir}")

    try:
        for size_kb in INPUT_SIZES_KB:
            test_file = os.path.join(tmp_dir, f"test_{size_kb}mb.bin")
            create_test_file(size_kb, test_file)

            for chunk_size in CHUNK_SIZES_KB:
                build_all(chunk_size)

                # Configurations to run: (type, np)
                configs = [("serial", "-")] + [
                    ("parallel", np) for np in PROCESS_COUNTS
                ]

                for run_type, np in configs:
                    for mode in ["compress", "decompress"]:
                        label = f"{run_type.capitalize()} {mode.capitalize()}: Size={size_kb}KB, Chunk={chunk_size}KB"
                        if run_type == "parallel":
                            label += f", NP={np}"
                        print(label)

                        comp_file = os.path.join(tmp_dir, f"test_{size_kb}mb.huff")
                        decomp_file = os.path.join(
                            tmp_dir, f"test_{size_kb}mb.decomp.bin"
                        )

                        for i in range(ITERATIONS):
                            if mode == "compress":
                                cmd = (
                                    f"bin/huffman-serial -c {test_file} {comp_file}"
                                    if run_type == "serial"
                                    else f"mpirun -n {np} bin/huffman-parallel -c {test_file} {comp_file}"
                                )
                            else:
                                if not os.path.exists(comp_file):
                                    print(
                                        "  Warning: Skipping decompression, compressed file missing."
                                    )
                                    continue
                                cmd = (
                                    f"bin/huffman-serial -d {comp_file} {decomp_file}"
                                    if run_type == "serial"
                                    else f"mpirun -n {np} bin/huffman-parallel -d {comp_file} {decomp_file}"
                                )

                            stdout, wall_time = run_command(cmd)
                            core_time = extract_time(stdout)

                            if core_time is not None:
                                results.append(
                                    {
                                        "type": run_type,
                                        "input_size_kb": size_kb,
                                        "chunk_size_bytes": chunk_size // 1024,
                                        "mpi_processes": np,
                                        "iteration": i + 1,
                                        "mode": mode,
                                        "core_time_s": core_time,
                                        "total_time_s": wall_time,
                                        "overhead_s": wall_time - core_time,
                                        "core_throughput_mbs": (size_kb // 1024)
                                        / core_time,
                                        "total_throughput_mbs": (size_kb // 1024)
                                        / wall_time,
                                    }
                                )
                            else:
                                print(f"  Warning: {label} run {i + 1} failed.")

                            if mode == "decompress" and os.path.exists(decomp_file):
                                os.remove(decomp_file)

                        if (
                            mode == "compress"
                            and os.path.exists(comp_file)
                            and run_type == "parallel"
                            and np == PROCESS_COUNTS[-1]
                        ):
                            # Only remove after all parallel runs for this chunk size are done if we were sequential,
                            # but here we remove it after each mode's iterations if it's the last np.
                            # Actually let's just keep it for decompression and remove after.
                            pass

                    # Clean up compressed file after both modes for this config
                    comp_file = os.path.join(tmp_dir, f"test_{size_kb}mb.huff")
                    if os.path.exists(comp_file):
                        os.remove(comp_file)

            if os.path.exists(test_file):
                os.remove(test_file)
    finally:
        shutil.rmtree(tmp_dir)
        print(f"Cleaned up temporary directory: {tmp_dir}")

    if results:
        keys = results[0].keys()
        for filename in [RESULTS_FILE, LATEST_FILE]:
            with open(filename, "w", newline="") as f:
                dict_writer = csv.DictWriter(f, fieldnames=keys)
                dict_writer.writeheader()
                dict_writer.writerows(results)

    print("\nBenchmark finished.")
    print(f"  Results saved to: {LATEST_FILE}")


if __name__ == "__main__":
    benchmark()
