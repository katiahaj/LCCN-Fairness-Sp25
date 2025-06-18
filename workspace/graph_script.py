# plot_throughput.py
import pandas as pd
import matplotlib.pyplot as plt

# Replace with your actual CSV filename
csv_file = "results-Bbr-vs-Bbr.csv"

# Read the CSV, skipping comment lines
df = pd.read_csv(csv_file, comment='#', sep=',|\s+', engine='python')

plt.figure(figsize=(10, 6))
plt.plot(df['TimeInSeconds'], df['Flow1ThroughputBytesPerSecond'], label='Flow 1 Throughput', color='blue')
plt.plot(df['TimeInSeconds'], df['Flow2ThroughputBytesPerSecond'], label='Flow 2 Throughput', color='red')
plt.xlabel('TimeInSeconds')
plt.ylabel('Throughput (Bytes/s)')
plt.title(f'Throughput vs Time\n{csv_file}')
plt.legend()
plt.grid(True)
plt.tight_layout()
plt.show()