import sys
from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd


def find_latest_csv():
    results_dir = Path(__file__).parent / "results"
    csv_files = list(results_dir.glob("perf_results_*.csv"))
    if not csv_files:
        print("No CSV files found in results/", file=sys.stderr)
        sys.exit(1)
    return max(csv_files, key=lambda p: p.stat().st_mtime)


if __name__ == "__main__":
    csv_path = sys.argv[1] if len(sys.argv) > 1 else find_latest_csv()

    df = pd.read_csv(csv_path)

    size_columns = [col for col in df.columns if col.startswith("size_")]
    sizes_kb = [int(col.replace("size_", "").replace("kb", "")) for col in size_columns]

    variants = df["variant"].unique()

    fig, axes = plt.subplots(2, 3, figsize=(18, 10))
    fig.suptitle(
        "AES-CTR Performance: Throughput vs Thread Count",
        fontsize=16,
        fontweight="bold",
    )

    axes = axes.flatten()

    for idx, (size_col, size_kb) in enumerate(zip(size_columns, sizes_kb)):
        ax = axes[idx]

        serial_time = df[df["variant"] == "serial"][size_col].iloc[0]
        serial_throughput = (size_kb * 1000) / serial_time

        for variant in variants:
            if variant == "serial":
                continue

            variant_data = df[df["variant"] == variant].copy()
            variant_data = variant_data.sort_values("threads")

            threads = variant_data["threads"].values
            times = variant_data[size_col].values
            throughput = (size_kb * 1000) / times

            label_map = {
                "parallel": "Parallel (pread/pwrite)",
                "parallel-for": "Parallel-for",
                "parallel-task": "Parallel-task",
            }
            label = label_map.get(variant, variant)

            ax.plot(
                threads, throughput, marker="o", linewidth=2, markersize=8, label=label
            )

        ax.axhline(
            y=serial_throughput,
            color="red",
            linestyle="--",
            linewidth=2,
            label=f"Serial ({serial_throughput:.0f} KB/s)",
            alpha=0.7,
        )

        ax.set_xlabel("Thread Count", fontsize=11, fontweight="bold")
        ax.set_ylabel("Throughput (KB/s)", fontsize=11, fontweight="bold")
        ax.set_title(f"File Size: {size_kb} KB", fontsize=12, fontweight="bold")
        ax.grid(True, alpha=0.3, linestyle="--")
        ax.legend(loc="best", fontsize=9)
        ax.set_xticks([2, 4, 8, 16])

        speedup_text = []
        for variant in ["parallel", "parallel-for", "parallel-task"]:
            variant_data = df[(df["variant"] == variant) & (df["threads"] == 8)]
            if not variant_data.empty:
                time = variant_data[size_col].iloc[0]
                speedup = serial_time / time
                speedup_text.append(f"{variant}: {speedup:.2f}x")

        ax.text(
            0.02,
            0.98,
            "\n".join(speedup_text),
            transform=ax.transAxes,
            fontsize=8,
            verticalalignment="top",
            bbox=dict(boxstyle="round", facecolor="wheat", alpha=0.5),
        )

    if len(size_columns) < len(axes):
        for idx in range(len(size_columns), len(axes)):
            fig.delaxes(axes[idx])

    plt.tight_layout()

    output_path = (
        Path(csv_path).parent / f"performance_visualization_{Path(csv_path).stem}.png"
    )
    plt.savefig(output_path, dpi=300, bbox_inches="tight")
    print(f"Visualization saved to: {output_path}")

    fig2, axes2 = plt.subplots(1, 3, figsize=(18, 5))
    fig2.suptitle(
        "AES-CTR Speedup vs Serial (8 Threads)", fontsize=16, fontweight="bold"
    )

    variants_to_compare = ["parallel", "parallel-for", "parallel-task"]
    colors = ["#1f77b4", "#ff7f0e", "#2ca02c"]

    for idx, variant in enumerate(variants_to_compare):
        ax = axes2[idx]
        variant_data = df[(df["variant"] == variant) & (df["threads"] == 8)]

        if not variant_data.empty:
            speedups = []
            for size_col in size_columns:
                serial_time = df[df["variant"] == "serial"][size_col].iloc[0]
                variant_time = variant_data[size_col].iloc[0]
                speedup = serial_time / variant_time
                speedups.append(speedup)

            bars = ax.bar(
                range(len(sizes_kb)),
                speedups,
                color=colors[idx],
                alpha=0.7,
                edgecolor="black",
            )
            ax.axhline(
                y=1, color="red", linestyle="--", linewidth=2, label="Serial baseline"
            )

            for i, (bar, speedup) in enumerate(zip(bars, speedups)):
                height = bar.get_height()
                ax.text(
                    bar.get_x() + bar.get_width() / 2.0,
                    height,
                    f"{speedup:.2f}x",
                    ha="center",
                    va="bottom",
                    fontsize=10,
                    fontweight="bold",
                )

            ax.set_xlabel("File Size (KB)", fontsize=11, fontweight="bold")
            ax.set_ylabel("Speedup vs Serial", fontsize=11, fontweight="bold")
            ax.set_title(
                f"{variant.capitalize()} (8 threads)", fontsize=12, fontweight="bold"
            )
            ax.set_xticks(range(len(sizes_kb)))
            ax.set_xticklabels([f"{s}" for s in sizes_kb], rotation=45, ha="right")
            ax.grid(True, alpha=0.3, axis="y", linestyle="--")
            ax.legend()

    plt.tight_layout()

    output_path2 = (
        Path(csv_path).parent / f"speedup_comparison_{Path(csv_path).stem}.png"
    )
    plt.savefig(output_path2, dpi=300, bbox_inches="tight")
    print(f"Speedup comparison saved to: {output_path2}")
