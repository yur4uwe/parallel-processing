import json
import subprocess
import os
import csv

CONFIG_FILE = "config_benchmark.json"
SCALING_CSV = "benchmark_scaling.csv"
TUNING_CSV = "benchmark_tuning.csv"

# Wider range to find the crossover point and saturation point
PARTICLE_COUNTS = [100, 500, 1000, 5000, 10000, 20000, 40000]
THREADS_PER_BLOCK_TESTS = [32, 64, 128, 256, 512, 1024]
STEPS = 100


def run_simulation(binary, particles, threads=256):
    with open("config_example.json", "r") as f:
        conf = json.load(f)

    conf["physics"]["particles"] = particles
    conf["physics"]["steps"] = STEPS
    conf["compute"]["threads_per_block"] = threads
    conf["io"]["output_file"] = "/dev/null"

    with open(CONFIG_FILE, "w") as f:
        json.dump(conf, f)

    try:
        result = subprocess.run([binary, CONFIG_FILE], capture_output=True, text=True)
        for line in result.stdout.splitlines():
            if "Performance:" in line:
                return float(line.split(":")[1].split()[0])
    except:
        return 0.0
    return 0.0


if __name__ == "__main__":
    print("🚀 Starting Comprehensive N-Body Benchmark\n")

    # Test 1: Scaling Analysis
    print("--- Test 1: Scaling Analysis (Serial vs CUDA) ---")
    scaling_results = []
    with open(SCALING_CSV, mode="w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(
            ["particles", "serial_steps_per_sec", "cuda_steps_per_sec", "speedup"]
        )

        for p in PARTICLE_COUNTS:
            s_perf = run_simulation("./nbody-serial", p)
            c_perf = run_simulation("./nbody-cuda", p)
            speedup = c_perf / s_perf if s_perf > 0 else 0
            scaling_results.append((p, s_perf, c_perf, speedup))
            writer.writerow([p, s_perf, c_perf, speedup])
            print(
                f"N={p:5} | Serial: {s_perf:8.2f} | CUDA: {c_perf:8.2f} | Speedup: {speedup:6.2f}x"
            )

    # Test 2: Thread/Block Tuning
    print("\n--- Test 2: CUDA Kernel Tuning (Fixed N=10000) ---")
    with open(TUNING_CSV, mode="w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["threads_per_block", "performance_steps_per_sec"])

        fixed_n = 10000
        for t in THREADS_PER_BLOCK_TESTS:
            perf = run_simulation("./nbody-cuda", fixed_n, t)
            writer.writerow([t, perf])
            print(f"Threads/Block: {t:4} | Performance: {perf:8.2f} steps/s")

    print(f"\n✅ Data saved to {SCALING_CSV} and {TUNING_CSV}")
