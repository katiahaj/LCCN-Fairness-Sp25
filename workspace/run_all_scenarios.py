import subprocess
import sys
from pathlib import Path
import concurrent.futures
import multiprocessing
import random

NUM_SIMULATIONS = 10


# Define a worker function to run a single simulation
def run_simulation(params):
    scenario, rtt_asym, sim_run, root_dir = params
    rtt_name = "asymmetric" if rtt_asym else "symmetric"
    rtt_flag = "true" if rtt_asym else "false"

    experiment_dir = root_dir / f"{scenario}_{rtt_name}"
    experiment_dir.mkdir(parents=True, exist_ok=True)

    # Generate random seed for this run
    seed = random.randint(1, 1_000_000)

    output_file = experiment_dir / f"run_{sim_run:03d}.csv"

    command = [
        "./ns3",
        "run",
        "scratch/workspace/script.cc",
        "--",  # The separator for ns-3 options vs. script options
        f"--scenario={scenario}",
        f"--asymmetricRtt={rtt_flag}",
        f"--seed={seed}",
        f"--outputFile={output_file}",
    ]

    run_id = (
        f"Scenario={scenario}, RTT={rtt_name}, Simulation={sim_run}/{NUM_SIMULATIONS}"
    )
    print(f"Starting: {run_id}")

    try:
        subprocess.run(
            command, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE
        )
        print(f"SUCCESS: {run_id} - Saved to {output_file}")
        return True
    except subprocess.CalledProcessError as e:
        print(
            f"FAILURE: {run_id} failed with exit code {e.returncode}.", file=sys.stderr
        )
        return False


def main():
    """
    Master Runner Script for ns-3 TCP Fairness Experiments.
    Automates running all defined scenarios.
    """

    # Configuration
    scenarios = [
        "AllNewReno",
        "AllCubic",
        "AllBbr",
        "AllDctcp",
        "RenoVsCubic",
        "RenoVsBbr",
        "BbrVsCubic",
        "AllMixed",
    ]
    rtt_configs = [False, True]  # False for symmetric, True for asymmetric

    print(f"NUM_SIMULATIONS = {NUM_SIMULATIONS}")

    # Root results directory
    ROOT = Path("scratch/workspace/all_results_csv")

    # Prerequisites
    print(f"Creating root results directory: {ROOT}")
    ROOT.mkdir(parents=True, exist_ok=True)

    # Main Execution Loop
    total_runs = len(scenarios) * len(rtt_configs) * NUM_SIMULATIONS

    print("-" * 60)
    print(f"Preparing to run {total_runs} simulations in parallel")
    print("-" * 60)

    # Create a list of all simulation parameters
    simulation_params = []
    for scenario in scenarios:
        for rtt_asym in rtt_configs:
            for sim_run in range(1, NUM_SIMULATIONS + 1):
                simulation_params.append((scenario, rtt_asym, sim_run, ROOT))

    # Determine the number of workers (use 75% of available cores, minimum 1)
    max_workers = max(1, int(multiprocessing.cpu_count() * 0.75))
    print(f"Using {max_workers} parallel workers")

    # Run simulations in parallel
    with concurrent.futures.ProcessPoolExecutor(max_workers=max_workers) as executor:
        results = list(executor.map(run_simulation, simulation_params))

    # Report summary
    successful = results.count(True)
    failed = results.count(False)
    print(f"\nCompleted {successful} successful simulations, {failed} failed")

    print("--- All simulations completed! ---")
    print(f"All results are located in the '{ROOT}' directory.")


if __name__ == "__main__":
    main()
