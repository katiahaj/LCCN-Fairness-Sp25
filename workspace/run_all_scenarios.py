import os
import subprocess
import sys

def main():
    """
    Master Runner Script for ns-3 TCP Fairness Experiments.
    Automates running all defined scenarios.
    """

    # Configuration
    scenarios = [
        "AllNewReno", "AllCubic", "AllBbr", "AllDctcp",
        "RenoVsCubic", "RenoVsBbr", "BbrVsCubic", "AllMixed"
    ]
    rtt_configs = [False, True]  # False for symmetric, True for asymmetric

    output_dir = "/workspace/all_results_csv"

    # Prerequisites
    print(f"Creating output directory: {output_dir}")
    os.makedirs(output_dir, exist_ok=True)

    print("Copying script.cc to scratch directory...")
    # Using subprocess to run the cp command
    subprocess.run(["cp", "/workspace/script.cc", "./scratch/"], check=True)

    # Main Execution Loop
    run_count = 1
    total_runs = len(scenarios) * len(rtt_configs)

    for scenario in scenarios:
        for rtt_asym in rtt_configs:
            print("-" * 60)
            print(f"RUN {run_count}/{total_runs}: Starting Scenario: {scenario}, Asymmetric RTT: {rtt_asym}")
            print("-" * 60)

            rtt_name = "asymmetric" if rtt_asym else "symmetric"
            rtt_flag = "true" if rtt_asym else "false"
            
            output_file = f"{output_dir}/results_{scenario}_{rtt_name}.csv"

            command = [
                "./ns3",
                "run",
                "scratch/script.cc",
                "--",  # The separator for ns-3 options vs. script options
                f"--scenario={scenario}",
                f"--asymmetricRtt={rtt_flag}",
                f"--outputFile={output_file}"
            ]
            
            print(f"Executing command: {' '.join(command)}")

            try:
                # Execute the command
                subprocess.run(command, check=True)
                print(f"SUCCESS: Scenario {scenario} ({rtt_name}) completed.")
                print(f"Output saved to {output_file}\n")
            except subprocess.CalledProcessError as e:
                print(f"FAILURE: Scenario {scenario} ({rtt_name}) failed with exit code {e.returncode}.", file=sys.stderr)
            
            run_count += 1
            
    print("--- All scenarios completed! ---")
    print(f"All results are located in the '{output_dir}' directory.")

if __name__ == "__main__":
    main()
    