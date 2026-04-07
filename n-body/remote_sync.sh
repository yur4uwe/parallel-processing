#!/usr/bin/env bash

# --- CONFIGURATION ---
WIN_USER="Yup4uwe"
WIN_IP="192.168.88.254"
# ---------------------

echo "🛠️  Building binaries locally on Mint..."
./build.bash serial
./build.bash cuda

if [ $? -ne 0 ]; then
    echo "❌ Local build failed! Please check errors."
    exit 1
fi

echo "🚀 Sending binaries and benchmark script to Windows (C:/temp/)..."
# Send both binaries, the config file, and the benchmark script
scp bin/nbody-serial bin/nbody-cuda config_example.json benchmark.py ${WIN_USER}@${WIN_IP}:C:/temp/

echo "🏃 Running benchmarks on Windows/WSL..."
# We call WSL to run the python benchmark script
ssh ${WIN_USER}@${WIN_IP} "wsl -d Ubuntu bash -c \"chmod +x /mnt/c/temp/nbody-serial /mnt/c/temp/nbody-cuda && cd /mnt/c/temp/ && python3 benchmark.py\""

echo "📥 Fetching benchmark CSVs..."
# Pull the CSV files back from the Windows bridge folder
scp ${WIN_USER}@${WIN_IP}:C:/temp/benchmark_scaling.csv ./benchmark_scaling.csv
scp ${WIN_USER}@${WIN_IP}:C:/temp/benchmark_tuning.csv ./benchmark_tuning.csv

echo "✅ Done! CSV files saved locally."
