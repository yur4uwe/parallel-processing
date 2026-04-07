# N-Body CUDA Project

## Overview
2D N-body gravitational simulation implemented in **pure C**. The project will focus on a serial implementation followed by a CUDA parallel force calculation, with binary file output and optional compression.

## Scope
- Implementation in C (no C++ features).
- Serial implementation as a correctness and performance baseline.
- CUDA parallelization for performance optimization.
- Custom JSON parser for configuration.

---

## Algorithm

### Core Loop (per time step)
1. **Force Calculation:** For each particle, sum gravitational forces from all other particles.
   - **Serial:** Nested O(N²) loops on CPU.
   - **CUDA:** One thread per particle on GPU.
2. **Update Velocities:** Using computed forces.
3. **Update Positions:** Using velocities.
4. **Repeat for N steps.**

### Force Calculation
- O(N²) interactions per step.
- Every interaction is independent — maps directly to CUDA threads.
- Each thread computes force on one particle from all others.

### Integration Method
- **Leapfrog integration** — provides better energy conservation than Euler, used as the default.

---

## Configuration & JSON Parser

### Custom JSON Parser
- Written from scratch in C for the purpose of learning and flexibility.
- Reads simulation parameters from a `.json` file.
- Handles standard JSON data types (numbers, strings, booleans, objects).

### JSON Schema Example
```json
{
   "physics": {
     "particles": 10000, // number of particles
     "steps": 5000, // number of steps taken by simulation
     "dt": 0.01, // time step
     "G": 6.674e-11, // gravitational constant
     "softening": 0.1 // softening parameter to avoid division by zero
   },
   "initial_state": {
     "type": "random", // "random", "orbit", "collision"
     "seed": 0, // random seed for initial state 0 = random
     "mass_min": 1.0, // minimum mass of particles
     "mass_max": 10.0, // maximum mass of particles
     "mass_distribution": "uniform", // mass distribution type
     "alpha": 1.0, // power law exponent
     "box_size": 500.0, // size of simulation box
     "velocity_scale": 0.05 // scale for initial velocity relative to box size 
     // (for this example particle can at most cross 5% of the box in a step)
   },
   "compute": {
     "mode": "serial", // serial or CUDA
     "threads_per_block": 256 // threads per block for CUDA
   },
   "io": {
     "output_file": "simulation.bin", // output file path
     "stride": 10, // write every Nth step to file
     "compression": true // enable compression
   }
}
```

---

## CUDA Architecture

### Parallelization Strategy
- One CUDA thread per particle.
- Each thread iterates over all other particles to sum forces.
- No inter-thread dependencies within a step — embarrassingly parallel.

### Benchmark Metric
- Simulated time steps per second.
- CPU vs GPU speedup at varying N (e.g. 1000, 5000, 10000, 50000 particles).

---

## Output Format

### Binary File
- Raw binary, not text — float formatting is too slow to be in the simulation loop.
- Write every Nth step via stride parameter to control file size.

### File Layout
```
step0: x0 y0 x1 y1 x2 y2 ... xN yN
step1: x0 y0 x1 y1 x2 y2 ... xN yN
...
```
Each value is a 32-bit float (4 bytes). Total size: `2 * 4 * num_particles * (num_steps / stride)` bytes.

### File Size Estimates (no compression)
| Particles | Steps | Stride | Size |
|-----------|-------|--------|------|
| 10,000 | 1,000 | 1 | ~80MB |
| 10,000 | 10,000 | 10 | ~80MB |
| 50,000 | 10,000 | 10 | ~400MB |

---

## Compression

### Approach: Quantization + Huffman
Reuses existing Huffman implementation from MPI project.

#### Step 1 — Quantization
- Reduce 32-bit floats to 16-bit integers scaled to simulation bounds.
- Halves raw file size before compression.
- Sufficient precision for visualization purposes.

#### Step 2 — Delta Encoding
- Store differences between steps rather than absolute positions.
- Particle movement is smooth so deltas are small.
- Small value range improves Huffman compression ratio.

#### Step 3 — Huffman
- Apply existing Huffman encoder to quantized delta bytes.
- Reuses serial Huffman implementation directly.

---

## Visualization
Separate Python script, not part of graded work — demo tool only.
```python
import numpy as np
import matplotlib.animation as animation
# read binary positions, reshape, animate
# ~30 lines
```

---

## Benchmark Plan
Run with varying N and measure wall time per step:
- Compare CPU sequential vs CUDA.
- Plot speedup = T_cpu(N) / T_gpu(N).
- Expected significant speedup at N > 1000 where O(N²) dominates.

---

## Notes
- Chaos property: tiny floating point errors accumulate over time, diverging from true solution — acceptable for simulation purposes.
- 3-body and N-body have no closed-form analytical solution, only numerical approximation via time stepping.
- Physical accuracy is not a goal — correctness means stable simulation without explosions.
- **CUDA API for Go:** Available via `cgo`, but this project remains in C for better alignment with CUDA patterns and low-level control.

## Coding Conventions

### JSON Assertions
The project uses specialized macros in `json/json.h` for validating JSON structures. To use these macros, the calling function **must** follow this pattern:
1. Define an integer error code variable: `int ec = EXIT_SUCCESS;`
2. Provide a label for failure handling: `assert_failed:`
3. Ensure `assert_failed:` performs necessary resource cleanup and returns `ec`.

Example:
```c
int my_function() {
    int ec = EXIT_SUCCESS;
    // ...
    JSON_ASSERT_TYPE(val, JSON_OBJECT, "Expected object");
    return EXIT_SUCCESS;

assert_failed:
    // cleanup
    return ec;
}
```
