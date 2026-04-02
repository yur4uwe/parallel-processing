import numpy as np
import matplotlib.pyplot as plt
import matplotlib.animation as animation
import struct
import sys
import os


def read_header(f):
    # struct format: uint32_t (count), uint32_t (num_snapshots), 4 * float (bounds)
    header_data = f.read(4 + 4 + 16)
    if not header_data:
        return None
    count, num_snapshots, min_x, max_x, min_y, max_y = struct.unpack(
        "IIffff", header_data
    )
    return {
        "count": count,
        "num_snapshots": num_snapshots,
        "bounds": (min_x, max_x, min_y, max_y),
    }


def dequantize(val, min_val, max_val):
    return (val / 65535.0) * (max_val - min_val) + min_val


def read_snapshot(f, count, bounds):
    min_x, max_x, min_y, max_y = bounds
    # Each particle has 2 uint16_t (qx, qy)
    data = f.read(count * 4)
    if not data:
        return None

    raw_coords = struct.unpack("H" * (count * 2), data)
    x = [dequantize(raw_coords[i * 2], min_x, max_x) for i in range(count)]
    y = [dequantize(raw_coords[i * 2 + 1], min_y, max_y) for i in range(count)]
    return x, y


def verify_file_structure(filename):
    file_size = os.path.getsize(filename)
    if file_size < 24:
        print(f"[FAIL] File too small to contain header ({file_size} bytes)")
        return False

    with open(filename, "rb") as f:
        header = read_header(f)
        if not header:
            print("[FAIL] Could not parse header")
            return False

    count = header["count"]
    num_snapshots = header["num_snapshots"]
    min_x, max_x, min_y, max_y = header["bounds"]

    print(f"--- Verification Report ---")
    print(f"File Size: {file_size} bytes")
    print(f"Particles: {count}")
    print(f"Snapshots: {num_snapshots}")

    # Check 1: Sanity of bounds
    if min_x >= max_x or min_y >= max_y:
        print(f"[FAIL] Invalid bounds: X[{min_x}, {max_x}] Y[{min_y}, {max_y}]")
        return False

    # Check 2: Sanity of counts
    if count == 0 or num_snapshots == 0:
        print(f"[FAIL] Particle or snapshot count is zero")
        return False

    # Check 3: File size calculation
    # Header (24) + Snapshots (num_snapshots * count * 4 bytes)
    expected_size = 24 + (num_snapshots * count * 4)

    if file_size == expected_size:
        print(f"[OK] File size matches metadata perfectly.")
    elif file_size > expected_size:
        extra = file_size - expected_size
        print(
            f"[WARN] File has {extra} bytes of trailing data (could be padding or compression metadata)"
        )
    else:
        missing = expected_size - file_size
        print(
            f"[FAIL] File is truncated. Missing {missing} bytes for the expected snapshot count."
        )
        return False

    print(f"---------------------------\n")
    return True


def visualize(filename: str, interval: int = 30):
    if not os.path.exists(filename):
        print(f"Error: File {filename} not found.")
        return

    if not verify_file_structure(filename):
        print("Aborting visualization due to file structure errors.")
        return

    with open(filename, "rb") as f:
        header = read_header(f)
        if not header:
            print("Error: Could not read file header.")
            return

        print(
            f"Visualizing {header['count']} particles over {header['num_snapshots']} snapshots."
        )
        print(f"Bounds: {header['bounds']}")

        snapshots = []
        for i in range(header["num_snapshots"]):
            snap = read_snapshot(f, header["count"], header["bounds"])
            if snap:
                snapshots.append(snap)
            else:
                print(
                    f"Warning: Only read {len(snapshots)} of {header['num_snapshots']} snapshots."
                )
                break

    fig, ax = plt.subplots(figsize=(8, 8))
    min_x, max_x, min_y, max_y = header["bounds"]
    ax.set_xlim(min_x, max_x)
    ax.set_ylim(min_y, max_y)
    ax.set_aspect("equal")
    ax.set_facecolor("black")

    # Hide axes for a cleaner look
    ax.get_xaxis().set_visible(False)
    ax.get_yaxis().set_visible(False)

    scatter = ax.scatter([], [], s=1, c="white", alpha=0.6)
    
    # Text for frame counter
    frame_text = ax.text(0.02, 0.95, '', transform=ax.transAxes, color='white', fontsize=12)

    def init():
        scatter.set_offsets(np.empty((0, 2)))
        frame_text.set_text('')
        return scatter, frame_text

    def update(frame):
        if frame == 0:
            print("Animation started.")
        
        x, y = snapshots[frame]
        data = np.column_stack((x, y))
        scatter.set_offsets(data)
        
        # Update frame counter
        current_frame = frame + 1
        total_frames = len(snapshots)
        frame_text.set_text(f"Snapshot: {current_frame}/{total_frames}")
        
        if current_frame == total_frames:
            print("Animation reached the end.")
            
        return scatter, frame_text

    ani = animation.FuncAnimation(
        fig, update, frames=len(snapshots), init_func=init, blit=True, interval=interval, repeat=False
    )

    plt.title(f"N-Body Simulation: {header['count']} particles", color="white", pad=-20)
    fig.patch.set_facecolor("black")
    plt.show()


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python visualize.py <simulation_output.bin> [interval_ms]")
        print("Example: python visualize.py simulation.bin 50")
    else:
        interval = 30
        if len(sys.argv) > 2:
            try:
                interval = int(sys.argv[2])
            except ValueError:
                print(f"Invalid interval: {sys.argv[2]}, using default 30ms")
        
        visualize(sys.argv[1], interval)
