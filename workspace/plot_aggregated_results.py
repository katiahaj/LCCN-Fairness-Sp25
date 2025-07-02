import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
from pathlib import Path
from typing import Dict, List


def setup_plot_style():
    """Configure matplotlib and seaborn styling."""
    plt.style.use("seaborn-v0_8")
    sns.set_palette("husl")
    plt.rcParams["figure.dpi"] = 100
    plt.rcParams["font.size"] = 10


def parse_column_pairs(columns: List[str]) -> Dict[str, List[str]]:
    """Group columns into pairs (Flow1/Flow2) and singles."""
    pairs = {}
    singles = []

    # Extract base metrics (remove Flow1_/Flow2_ prefixes)
    flow_cols = [col for col in columns if col.startswith(("Flow1_", "Flow2_"))]

    for col in flow_cols:
        if col.startswith("Flow1_"):
            base_metric = col[6:]  # Remove 'Flow1_'
            flow2_col = f"Flow2_{base_metric}"
            if flow2_col in columns:
                if base_metric not in pairs:
                    pairs[base_metric] = []
                pairs[base_metric] = [col, flow2_col]

    # Non-flow columns
    singles = [
        col for col in columns if not col.startswith(("Flow1_", "Flow2_", "Time"))
    ]

    return pairs, singles


def plot_paired_metrics(
    df: pd.DataFrame, metric: str, cols: List[str], save_dir: Path, experiment: str
):
    """Plot paired flow metrics on the same chart."""
    plt.figure(figsize=(10, 6))

    for col in cols:
        flow_num = col.split("_")[0]
        label = f"{flow_num} {metric}"
        plt.plot(df["Time"], df[col], label=label, linewidth=2)

    plt.xlabel("Time (seconds)")
    plt.ylabel(metric.replace("_", " ").title())
    plt.title(f"{experiment} - {metric.replace('_', ' ').title()} Comparison")
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.tight_layout()

    filename = save_dir / f"{experiment}_{metric}_comparison.png"
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
        plt.ylabel(metric.replace("_", " ").title())
        plt.title(f"{experiment} - {metric.replace('_', ' ').title()}")
        plt.grid(True, alpha=0.3)
        plt.tight_layout()

        filename = save_dir / f"{experiment}_{metric}.png"
        plt.savefig(filename, dpi=150, bbox_inches="tight")
        plt.close()

        print(f"  Saved: {filename}")


def create_stacked_overview(
    df: pd.DataFrame,
    pairs: Dict[str, List[str]],
    singles: List[str],
    save_dir: Path,
    experiment: str,
):
    """Create a vertically stacked overview of all metrics."""

    # Calculate number of subplots needed
    n_plots = len(pairs) + len(singles)
    if n_plots == 0:
        return

    fig, axes = plt.subplots(n_plots, 1, figsize=(12, 3 * n_plots), sharex=True)

    # Handle case of single subplot
    if n_plots == 1:
        axes = [axes]

    plot_idx = 0

    # Plot paired metrics
    for metric, cols in pairs.items():
        ax = axes[plot_idx]
        for col in cols:
            flow_num = col.split("_")[0]
            label = f"{flow_num} {metric}"
            ax.plot(df["Time"], df[col], label=label, linewidth=2)

        ax.set_ylabel(metric.replace("_", " ").title())
        ax.legend(loc="upper right")
        ax.grid(True, alpha=0.3)
        plot_idx += 1

    # Plot single metrics
    for metric in singles:
        ax = axes[plot_idx]
        ax.plot(df["Time"], df[metric], linewidth=2, color="steelblue")
        ax.set_ylabel(metric.replace("_", " ").title())
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
    pairs, singles = parse_column_pairs(df.columns.tolist())

    # Generate paired plots
    for metric, cols in pairs.items():
        plot_paired_metrics(df, metric, cols, exp_dir, experiment)

    # Generate single metric plots
    if singles:
        plot_single_metrics(df, singles, exp_dir, experiment)

    # Generate stacked overview
    create_stacked_overview(df, pairs, singles, exp_dir, experiment)

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
