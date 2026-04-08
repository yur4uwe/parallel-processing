import csv
import matplotlib.pyplot as plt

def read_csv(filename):
    data = []
    with open(filename, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            data.append({k: float(v) for k, v in row.items()})
    return data

# Load scaling data
scaling_data = read_csv('simulations/benchmark_scaling.csv')
particles = [d['particles'] for d in scaling_data]
serial_perf = [d['serial_steps_per_sec'] for d in scaling_data]
cuda_perf = [d['cuda_steps_per_sec'] for d in scaling_data]
speedup = [d['speedup'] for d in scaling_data]

# Plot 1: CPU vs GPU Performance
plt.figure(figsize=(10, 6))
plt.plot(particles, serial_perf, 'o-', label='Serial (CPU)')
plt.plot(particles, cuda_perf, 's-', label='CUDA (GPU)')
plt.xscale('log')
plt.yscale('log')
plt.xlabel('Number of Particles (N)')
plt.ylabel('Steps per Second (Log Scale)')
plt.title('N-Body Simulation Performance: CPU vs GPU')
plt.legend()
plt.grid(True, which="both", ls="-", alpha=0.5)
plt.savefig('simulations/performance_comparison.png')
plt.close()

# Plot 2: Speedup
plt.figure(figsize=(10, 6))
plt.plot(particles, speedup, 'D-', color='green')
plt.xscale('log')
plt.xlabel('Number of Particles (N)')
plt.ylabel('Speedup (Serial Time / CUDA Time)')
plt.title('CUDA Speedup vs. Number of Particles')
plt.grid(True, which="both", ls="-", alpha=0.5)
plt.savefig('simulations/cuda_speedup.png')
plt.close()

# Load tuning data
tuning_data = read_csv('simulations/benchmark_tuning.csv')
tpb = [str(int(d['threads_per_block'])) for d in tuning_data]
tuning_perf = [d['performance_steps_per_sec'] for d in tuning_data]

# Plot 3: CUDA Tuning (Threads per Block)
plt.figure(figsize=(10, 6))
plt.bar(tpb, tuning_perf, color='skyblue')
plt.xlabel('Threads per Block')
plt.ylabel('Steps per Second')
plt.title('CUDA Performance Tuning: Threads per Block')
plt.grid(axis='y', linestyle='--', alpha=0.7)
plt.savefig('simulations/cuda_tuning.png')
plt.close()

print("Plots generated successfully in simulations/ directory.")
