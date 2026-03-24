import matplotlib.pyplot as plt
import seaborn as sns
import os
import sys
import pandas as pd
from typing import cast

# Set style
sns.set_theme(style="whitegrid")
plt.rcParams["figure.figsize"] = (12, 8)


def load_data(csv_path: str) -> pd.DataFrame:
    if not os.path.exists(csv_path):
        print(f"Error: {csv_path} not found.")
        sys.exit(1)
    df = pd.read_csv(csv_path)
    # Ensure numeric types
    df["input_size_kb"] = pd.to_numeric(df["input_size_kb"], errors="coerce")
    df["chunk_size_bytes"] = pd.to_numeric(df["chunk_size_bytes"], errors="coerce")
    df["core_throughput_mbs"] = pd.to_numeric(
        df["core_throughput_mbs"], errors="coerce"
    )
    df["total_throughput_mbs"] = pd.to_numeric(
        df["total_throughput_mbs"], errors="coerce"
    )
    df["overhead_s"] = pd.to_numeric(df.get("overhead_s", 0), errors="coerce")
    return cast(pd.DataFrame, df)


def plot_throughput_scaling(df: pd.DataFrame, output_dir: str) -> None:
    """Q1 & Q4: Throughput (MB/s) for both modes on same chart, Serial as baseline."""
    # Filter for a specific chunk size (e.g., 64KB) to show scaling
    chunk_to_plot = 64 * 1024
    subset = df[df["chunk_size_bytes"] == chunk_to_plot].copy()
    if not isinstance(subset, pd.DataFrame):
        subset = pd.DataFrame(subset)

    # Calculate averages for line plot
    grouped = subset.groupby(["type", "mpi_processes", "mode", "input_size_kb"])
    avg_df = grouped.agg(
        {"core_throughput_mbs": "mean", "total_throughput_mbs": "mean"}
    ).reset_index()
    if not isinstance(avg_df, pd.DataFrame):
        avg_df = pd.DataFrame(avg_df)

    for size in avg_df["input_size_kb"].unique():
        size_df = avg_df[avg_df["input_size_kb"] == size]
        if not isinstance(size_df, pd.DataFrame):
            size_df = pd.DataFrame(size_df)

        _ = plt.figure()

        # Separate serial baseline
        serial_comp_subset = size_df[
            (size_df["type"] == "serial") & (size_df["mode"] == "compress")
        ]["core_throughput_mbs"]

        serial_decomp_subset = size_df[
            (size_df["type"] == "serial") & (size_df["mode"] == "decompress")
        ]["core_throughput_mbs"]

        # Parallel data
        parallel_df = size_df[size_df["type"] == "parallel"]

        if not parallel_df.empty:
            # Melt the parallel data to distinguish Core vs Total in a Seaborn-idiomatic way
            melted_df = parallel_df.melt(
                id_vars=["mpi_processes", "mode"],
                value_vars=["core_throughput_mbs", "total_throughput_mbs"],
                var_name="Throughput Type",
                value_name="MB/s",
            )
            # Rename for legend clarity
            type_map = {
                "core_throughput_mbs": "Core",
                "total_throughput_mbs": "Total (Inc. Init)",
            }
            melted_df["Throughput Type"] = melted_df["Throughput Type"].map(type_map)

            _ = sns.lineplot(
                data=melted_df,
                x="mpi_processes",
                y="MB/s",
                hue="mode",
                style="Throughput Type",
                markers=True,
                dashes=True,
            )

        # Add serial baselines if they exist
        if isinstance(serial_comp_subset, pd.Series) and not serial_comp_subset.empty:
            val = float(serial_comp_subset.iloc[0])
            _ = plt.axhline(
                y=val, color="blue", linestyle=":", label="Serial Compress Baseline"
            )

        if (
            isinstance(serial_decomp_subset, pd.Series)
            and not serial_decomp_subset.empty
        ):
            val = float(serial_decomp_subset.iloc[0])
            _ = plt.axhline(
                y=val, color="orange", linestyle=":", label="Serial Decompress Baseline"
            )

        _ = plt.title(
            f"Throughput Scaling (File Size: {size}MB, Chunk: {chunk_to_plot // 1024}KB)"
        )
        _ = plt.xlabel("Number of Processes")
        _ = plt.ylabel("Throughput (MB/s)")
        _ = plt.legend()
        _ = plt.savefig(os.path.join(output_dir, f"throughput_scaling_{size}mb.png"))
        plt.close()


def plot_impact_charts(df: pd.DataFrame, output_dir: str) -> None:
    """Q2: Impact of Chunk Size and File Size on Performance."""
    # 1. Chunk Size Impact
    _ = plt.figure()
    subset = df[df["type"] == "parallel"].copy()
    if not isinstance(subset, pd.DataFrame):
        subset = pd.DataFrame(subset)
    subset["chunk_size_kb"] = subset["chunk_size_bytes"] / 1024

    _ = sns.boxplot(data=subset, x="chunk_size_kb", y="core_throughput_mbs", hue="mode")
    _ = plt.title("Impact of Chunk Size on Parallel Throughput")
    _ = plt.ylabel("Core Throughput (MB/s)")
    _ = plt.savefig(os.path.join(output_dir, "impact_chunk_size.png"))
    plt.close()

    # 2. File Size Impact
    _ = plt.figure()
    _ = sns.boxplot(data=df, x="input_size_kb", y="core_throughput_mbs", hue="type")
    _ = plt.title("Impact of Input File Size on Throughput (Serial vs Parallel)")
    _ = plt.ylabel("Core Throughput (MB/s)")
    _ = plt.savefig(os.path.join(output_dir, "impact_file_size.png"))
    plt.close()


if __name__ == "__main__":
    csv_path = "benchmarks/benchmark_results.csv"
    output_dir = "benchmarks/plots"
    os.makedirs(output_dir, exist_ok=True)

    df_data = load_data(csv_path)

    print("Generating scaling plots...")
    plot_throughput_scaling(df_data, output_dir)

    print("Generating impact plots...")
    plot_impact_charts(df_data, output_dir)

    print(f"Plots saved to {output_dir}")
