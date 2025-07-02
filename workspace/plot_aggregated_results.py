import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
from pathlib import Path
from typing import Dict, List


# Human-readable axis labels mapping
UNIT_LABELS = {
    "Bps": "Throughput (Bytes/s)",
    "PktLoss": "Packet Loss (pkts)",
    "Cwnd": "CWND (segments)",
    "JainsFairnessIndex": "Jain's Fairness Index",
}


def setup_plot_style():
    """Configure matplotlib and seaborn styling."""
    plt.style.use("seaborn-v0_8")
    sns.set_palette("husl")
    plt.rcParams["figure.dpi"] = 100
    plt.rcParams["font.size"] = 10


def parse_flow_metrics(columns: List[str]) -> Dict[str, List[str]]:
    """Group flow-related columns by metric suffix.

    Returns dict where key is metric suffix (e.g., 'Bps') and value is list of columns
    belonging to that metric (e.g., ['Bbr_1_Bps', 'Cubic_2_Bps']). Also returns list of
    non-flow single-value metrics.
    """
    metric_groups: Dict[str, List[str]] = {}
    singles: List[str] = []

    for col in columns:
        if col == "Time":
            continue
        parts = col.split("_")
        if len(parts) >= 2 and parts[-1] in {"Bps", "PktLoss", "Cwnd"}:
            suffix = parts[-1]
            metric_groups.setdefault(suffix, []).append(col)
        else:
            singles.append(col)

    return metric_groups, singles


def plot_metric_group(
    df: pd.DataFrame,
    metric_suffix: str,
    cols: List[str],
    save_dir: Path,
    experiment: str,
):
    """Plot all flow columns that share the same metric suffix on one chart."""
    plt.figure(figsize=(10, 6))

    for col in cols:
        label = col.rsplit("_", 1)[0]  # everything except the metric suffix
        plt.plot(df["Time"], df[col], label=label, linewidth=2)

    plt.xlabel("Time (seconds)")
    ylabel = UNIT_LABELS.get(metric_suffix, metric_suffix)
    plt.ylabel(ylabel)
    plt.title(f"{experiment} - {ylabel}")
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.tight_layout()

    filename = save_dir / f"{experiment}_{metric_suffix}.png"
    plt.savefig(filename, dpi=150, bbox_inches="tight")
    plt.close()

    print(f"  Saved: {filename}")


def plot_single_metrics(
    df: pd.DataFrame, metrics: List[str], save_dir: Path, experiment: str
):
    """Plot single metrics individually."""
    for metric in metrics:
        plt.figure(figsize=(10, 6))
        plt.plot(df["Time"], df[metric], linewidth=2, color="steelblue")
        plt.xlabel("Time (seconds)")
        plt.ylabel(UNIT_LABELS.get(metric, metric))
        plt.title(f"{experiment} - {metric.replace('_', ' ').title()}")
        plt.grid(True, alpha=0.3)
        plt.tight_layout()

        filename = save_dir / f"{experiment}_{metric}.png"
        plt.savefig(filename, dpi=150, bbox_inches="tight")
        plt.close()

        print(f"  Saved: {filename}")


def create_stacked_overview(
    df: pd.DataFrame,
    metric_groups: Dict[str, List[str]],
    singles: List[str],
    save_dir: Path,
    experiment: str,
):
    """Create a vertically stacked overview of all metrics."""

    # Calculate number of subplots needed
    n_plots = len(metric_groups) + len(singles)
    if n_plots == 0:
        return

    fig, axes = plt.subplots(n_plots, 1, figsize=(12, 3 * n_plots), sharex=True)

    # Handle case of single subplot
    if n_plots == 1:
        axes = [axes]

    plot_idx = 0

    # Plot flow metric groups
    for metric_suffix, cols in metric_groups.items():
        ax = axes[plot_idx]
        for col in cols:
            label = col.rsplit("_", 1)[0]
            ax.plot(df["Time"], df[col], label=label, linewidth=2)

        ax.set_ylabel(UNIT_LABELS.get(metric_suffix, metric_suffix))
        ax.legend(loc="upper right")
        ax.grid(True, alpha=0.3)
        plot_idx += 1

    # Plot non-flow single metrics (e.g., Jain's Index)
    for metric in singles:
        ax = axes[plot_idx]
        ax.plot(df["Time"], df[metric], linewidth=2, color="steelblue")
        ax.set_ylabel(UNIT_LABELS.get(metric, metric))
        ax.grid(True, alpha=0.3)
        plot_idx += 1

    # Set x-label only on bottom plot
    axes[-1].set_xlabel("Time (seconds)")

    # Overall title
    fig.suptitle(f"{experiment} - Complete Overview", fontsize=16, y=0.98)
    plt.tight_layout()
    plt.subplots_adjust(top=0.95)

    filename = save_dir / f"{experiment}_overview_stacked.png"
    plt.savefig(filename, dpi=150, bbox_inches="tight")
    plt.close()

    print(f"  Saved: {filename}")


def process_experiment(csv_file: Path, output_dir: Path):
    """Process a single experiment CSV file."""
    experiment = csv_file.stem.replace("avg_", "")
    print(f"Processing: {experiment}")

    # Read data
    df = pd.read_csv(csv_file)

    if df.empty:
        print(f"  Skipping empty file: {csv_file}")
        return

    # Create experiment-specific output directory
    exp_dir = output_dir / experiment
    exp_dir.mkdir(exist_ok=True)

    # Parse columns into pairs and singles
    metric_groups, singles = parse_flow_metrics(df.columns.tolist())

    # Plot each metric group (could be 2+ flows)
    for metric_suffix, cols in metric_groups.items():
        plot_metric_group(df, metric_suffix, cols, exp_dir, experiment)

    # Plot non-flow single metrics (e.g., Jain's Index)
    if singles:
        plot_single_metrics(df, singles, exp_dir, experiment)

    # Generate stacked overview using the new grouping
    create_stacked_overview(df, metric_groups, singles, exp_dir, experiment)

    print(f"  Completed: {experiment}\n")


def main():
    """Main function to process all aggregated experiment files."""
    setup_plot_style()

    # Find all averaged CSV files
    workspace_dir = Path("scratch/workspace")
    csv_files = list(workspace_dir.glob("avg_*.csv"))

    if not csv_files:
        print("No aggregated CSV files found. Run aggregate_results.py first.")
        return

    # Create output directory
    output_dir = workspace_dir / "plots"
    output_dir.mkdir(exist_ok=True)

    print(f"Found {len(csv_files)} experiment files to plot")
    print(f"Output directory: {output_dir}\n")

    # Process each experiment
    for csv_file in sorted(csv_files):
        process_experiment(csv_file, output_dir)

    print("All plots generated successfully!")
    print(f"Check the '{output_dir}' directory for results.")


if __name__ == "__main__":
    main()
