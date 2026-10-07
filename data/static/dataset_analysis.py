"""Figure 2 and the dataset statistics of Table 2.

Run from data/static/, next to the raw dataset .dat files.
"""
import matplotlib
matplotlib.use("Agg")

import pandas as pd
import matplotlib.pyplot as plt
import csv
import glob
import os
import numpy as np

FONT_SIZES = {
    'title': 28,
    'suptitle': 32,
    'axis_label': 26,
    'tick': 24,
    'legend': 24,
    'annotation': 22,
    'bar_label': 22,
}

plt.rcParams.update({
    'font.size': 26,
    'axes.titlesize': FONT_SIZES['title'],
    'axes.labelsize': FONT_SIZES['axis_label'],
    'xtick.labelsize': FONT_SIZES['tick'],
    'ytick.labelsize': FONT_SIZES['tick'],
    'legend.fontsize': FONT_SIZES['legend'],
    'figure.autolayout': False,
    'pdf.fonttype': 42,
    'ps.fonttype': 42,
})

DATASET_ORDER = ['BIKES', 'FLIGHTS', 'TAXIS', 'WILDFIRES', 'WEBKIT']

# Find all dataset files in the current directory
data_files = sorted(glob.glob('*.dat'))
print(f"Found {len(data_files)} datasets: {data_files}")

datasets = {}
table2_rows = []

for file in data_files:
    clean_name = os.path.splitext(file)[0]
    print(f"--- {clean_name} ---")
    try:
        df = pd.read_csv(file, sep=r'\s+', header=None, usecols=[0, 1], names=['start', 'end'])
        df['duration'] = df['end'] - df['start']
        datasets[clean_name] = df
        
        # Statistics
        num_intervals = len(df)
        min_start = df['start'].min()
        max_end = df['end'].max()
        domain_extent = max_end - min_start
        median_dur = df['duration'].median()
        p99_dur = df['duration'].quantile(0.99)
        
        print(f"Number of Intervals: {num_intervals:,}")
        print(f"Domain Extent: {domain_extent:,} (Min Start: {min_start:,}, Max End: {max_end:,})")
        print(f"Median Duration (50%): {median_dur:,.0f}")
        print(f"99th Percentile Duration: {p99_dur:,.0f}")
        print()

        table2_rows.append([clean_name, num_intervals, domain_extent, median_dur, p99_dur])

    except Exception as e:
        print(f"Error reading {file}: {e}")

# Table 2: Dataset Statistics
table2_rows.sort(key=lambda row: DATASET_ORDER.index(row[0]) if row[0] in DATASET_ORDER else 999)
with open('dataset_statistics.csv', 'w', newline='') as csvfile:
    writer = csv.writer(csvfile)
    writer.writerow(['Dataset', 'Num Intervals', 'Domain Extent', 'Median Duration', '99th Percentile Duration'])
    writer.writerows(table2_rows)
print("Data saved as dataset_statistics.csv")

sorted_names = [name for name in DATASET_ORDER if name in datasets]
num_files = len(sorted_names)

# Figure 2a: Scatter Plots (x=end, y=duration)

if num_files > 0:
    fig, axes = plt.subplots(1, num_files, figsize=(6 * num_files, 6))
    if num_files == 1:
        axes = [axes]
    
    for i, name in enumerate(sorted_names):
        df = datasets[name]
        ax = axes[i]
        
        # If more than 3 million points, use 20% sample
        if len(df) > 3000000:
            df_plot = df.sample(frac=0.2, random_state=42)
        else:
            df_plot = df
        
        ax.scatter(df_plot['end'], df_plot['duration'], alpha=0.5, s=10, color='green', rasterized=True)
        ax.set_title(name)
        ax.set_xlabel('End Time')
        ax.set_ylabel('Duration')
        ax.grid(True)
        ax.ticklabel_format(style='scientific', axis='both', scilimits=(0,0))
        
    plt.tight_layout()
    plt.savefig('dataset_scatter_end_duration.pdf', format='pdf', bbox_inches='tight', dpi=150)
else:
    print("No datasets to plot.")

# Figure 2b: Histograms (x=duration, y=count), log-log scale
if num_files > 0:
    fig, axes = plt.subplots(1, num_files, figsize=(6 * num_files, 6))
    if num_files == 1:
        axes = [axes]
    
    for i, name in enumerate(sorted_names):
        df = datasets[name]
        ax = axes[i]
        dur = df['duration']
        dur = dur[dur > 0]
        bins = np.logspace(np.log10(dur.min()), np.log10(dur.max()), 50)
        ax.hist(dur, bins=bins, alpha=0.7, color='green', edgecolor='black')
        ax.set_title(name)
        ax.set_xlabel('Duration')
        ax.set_ylabel('Count')
        ax.set_xscale('log')
        ax.set_yscale('log')
        ax.grid(True)
        
    plt.tight_layout()
    plt.savefig('dataset_histograms.pdf', format='pdf', bbox_inches='tight')
    print("Saved dataset_histograms.pdf")
else:
    print("No datasets to plot.")
