import sys
from pathlib import Path
from typing import cast

import matplotlib.pyplot as plt
import pandas as pd
from matplotlib.patches import Patch


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

    # Group by variant and threads, then calculate mean and std for multiple runs
    grouped = cast(
        pd.DataFrame,
        df.groupby(["variant", "threads"])[size_columns]
        .agg(["mean", "std"])
        .reset_index(),
    )

    variants = df["variant"].unique()

    fig, axes = plt.subplots(2, 3, figsize=(18, 10))
    fig.suptitle(
        "AES-CTR Performance: Throughput vs Thread Count (Averaged across runs)",
        fontsize=16,
        fontweight="bold",
    )

    axes = axes.flatten()

    for idx, (size_col, size_kb) in enumerate(zip(size_columns, sizes_kb)):
        ax = axes[idx]

        # Get serial baseline (average)
        serial_data = grouped[grouped["variant"] == "serial"]
        serial_time = serial_data.loc[serial_data.index[0], (size_col, "mean")]
        serial_throughput = (size_kb * 1000) / serial_time

        # Collect data for box plots
        parallel_variants = [v for v in variants if v != "serial"]
        thread_counts = sorted(
            df[df["variant"].isin(parallel_variants)]["threads"].unique()
        )

        box_data = []
        positions = []
        colors_list = []
        labels_list = []

        color_map = {
            "parallel": "#1f77b4",
            "parallel-for": "#ff7f0e",
            "parallel-task": "#2ca02c",
        }

        label_map = {
            "parallel": "Parallel (sections)",
            "parallel-for": "Parallel-for",
            "parallel-task": "Parallel-task",
        }

        pos_offset = 0
        for thread_count in thread_counts:
            for v_idx, variant in enumerate(parallel_variants):
                variant_df = df[
                    (df["variant"] == variant) & (df["threads"] == thread_count)
                ]
                if not variant_df.empty:
                    times = variant_df[size_col].values
                    throughputs = (size_kb * 1000) / times
                    box_data.append(throughputs)
                    positions.append(pos_offset)
                    colors_list.append(color_map.get(variant, "#999999"))
                    if thread_count == thread_counts[0]:  # Only label once per variant
                        labels_list.append(label_map.get(variant, variant))
                    pos_offset += 1
            pos_offset += 0.5  # Gap between thread groups

        # Create box plots
        bp = ax.boxplot(
            box_data,
            positions=positions,
            widths=0.6,
            patch_artist=True,
            showmeans=True,
            meanprops=dict(
                marker="D", markerfacecolor="red", markeredgecolor="red", markersize=6
            ),
            medianprops=dict(linewidth=2, color="black"),
            boxprops=dict(linewidth=1.5),
            whiskerprops=dict(linewidth=1.5),
            capprops=dict(linewidth=1.5),
        )

        # Color the boxes
        for patch, color in zip(bp["boxes"], colors_list):
            patch.set_facecolor(color)
            patch.set_alpha(0.7)

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
        ax.grid(True, alpha=0.3, linestyle="--", axis="y")

        # Create legend with unique labels

        legend_elements = []
        seen_variants = set()
        for variant in parallel_variants:
            if variant not in seen_variants:
                legend_elements.append(
                    Patch(
                        facecolor=color_map.get(variant, "#999999"),
                        alpha=0.7,
                        label=label_map.get(variant, variant),
                    )
                )
                seen_variants.add(variant)
        legend_elements.append(
            plt.Line2D(
                [0], [0], color="red", linestyle="--", linewidth=2, label="Serial"
            )
        )
        ax.legend(handles=legend_elements, loc="best", fontsize=9)

        # Set x-axis labels for thread groups
        group_positions = []
        group_labels = []
        pos = 0
        for thread_count in thread_counts:
            group_size = len(parallel_variants)
            group_center = pos + (group_size - 1) / 2
            group_positions.append(group_center)
            group_labels.append(str(thread_count))
            pos += group_size + 0.5
        ax.set_xticks(group_positions)
        ax.set_xticklabels(group_labels)

        speedup_text = []
        for variant in variants:
            if variant == "serial":
                continue
            variant_data = grouped[
                (grouped["variant"] == variant) & (grouped["threads"] == 8)
            ]
            if not variant_data.empty:
                time = variant_data.loc[variant_data.index[0], (size_col, "mean")]
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
