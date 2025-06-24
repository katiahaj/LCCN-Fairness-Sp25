import pandas as pd
import matplotlib.pyplot as plt
import os
import glob
import sys

# Configuration
# The directory where the runner script saved all the CSV files.
RESULTS_DIR = "/workspace/all_results_csv"  # Change this if you used the bash script's default "all_results"

# The directory where the new plot images will be saved.
PLOTS_DIR = "/workspace/all_plots"

# Main Script

def create_all_plots():
    """
    Finds all CSV files in the results directory, and for each file,
    creates and saves a throughput plot.
    """
    print(f"--- Starting Graph Generation ---")
    
    # Check if the results directory exists
    if not os.path.isdir(RESULTS_DIR):
        print(f"Error: Results directory '{RESULTS_DIR}' not found.", file=sys.stderr)
        print("Please run the simulation suite first.", file=sys.stderr)
        sys.exit(1)

    # Create the output directory for plots if it doesn't exist
    os.makedirs(PLOTS_DIR, exist_ok=True)
    print(f"Plots will be saved in '{PLOTS_DIR}' directory.")

    # Find all CSV files in the results directory
    csv_files = glob.glob(os.path.join(RESULTS_DIR, '*.csv'))

    if not csv_files:
        print(f"No CSV files found in '{RESULTS_DIR}'. Nothing to plot.", file=sys.stderr)
        return

    # Loop through each discovered CSV file
    for csv_file in csv_files:
        print(f"\nProcessing: {csv_file}")
        try:
            # Read the CSV file, skipping any commented lines
            df = pd.read_csv(csv_file, comment='#')

            # Dynamically find all columns that represent flow throughput
            throughput_cols = [col for col in df.columns if col.endswith('_Bps')]
            
            if not throughput_cols:
                print(f"  > Warning: No throughput columns found in {csv_file}. Skipping.")
                continue

            # Create a new plot for this CSV file
            plt.figure(figsize=(12, 7))

            # Plot each throughput flow found in the file
            for col_name in throughput_cols:
                # The label will be the column name itself (for example "Flow1_NewReno_Bps")
                plt.plot(df['Time'], df[col_name], label=col_name)

            # Formatting
            base_name = os.path.basename(csv_file)
            title_name = base_name.replace('results_', '').replace('.csv', '').replace('_', ' ')
            
            plt.title(f"Throughput vs. Time\n({title_name})")
            plt.xlabel("Time (seconds)")
            plt.ylabel("Throughput (Bytes/s)")
            plt.legend()
            plt.grid(True)
            plt.tight_layout()

            # Saving as png
            plot_filename = base_name.replace(".csv", ".png")
            output_path = os.path.join(PLOTS_DIR, plot_filename)
            
            plt.savefig(output_path)
            print(f"  > Plot saved to: {output_path}")

            plt.close()

        except Exception as e:
            print(f"  > Error processing {csv_file}: {e}", file=sys.stderr)

    print("\n--- Graph generation complete! ---")

if __name__ == "__main__":
    create_all_plots()
