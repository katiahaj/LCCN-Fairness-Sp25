import pandas as pd
from pathlib import Path


def aggregate_experiment(experiment_dir: Path) -> pd.DataFrame:
    """Average all run CSVs in an experiment directory."""
    csv_files = list(experiment_dir.glob("run_*.csv"))
    if not csv_files:
        return pd.DataFrame()

    dfs = [pd.read_csv(csv_file) for csv_file in csv_files]

    # Average across all runs, grouping by time
    avg = (
        pd.concat(dfs)
        .groupby("Time", sort=False, as_index=False)
        .mean(numeric_only=True)
    )

    return avg


def main():
    """Process all experiment directories and save averaged results."""
    results_dir = Path("scratch/workspace/all_results_csv")

    if not results_dir.exists():
        print(f"Results directory not found: {results_dir}")
        return

    # Process each experiment directory
    for experiment_dir in results_dir.iterdir():
        if not experiment_dir.is_dir():
            continue

        print(f"Processing experiment: {experiment_dir.name}")

        avg_df = aggregate_experiment(experiment_dir)

        if avg_df.empty:
            print(f"  No CSV files found in {experiment_dir.name}")
            continue

        # Save averaged results to root folder
        output_file = results_dir.parent / f"avg_{experiment_dir.name}.csv"
        avg_df.to_csv(output_file, index=False)

        print(f"  Saved averaged results to: {output_file}")
        print(f"  Averaged {len(list(experiment_dir.glob('run_*.csv')))} runs")

    print("Aggregation complete!")


if __name__ == "__main__":
    main()
