import sys
import os
import re
import csv
from collections import defaultdict

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
from plot_utils import DATASET_ORDER, get_pretty_name, filter_datasets


def parse_scalability_log(logfile):
    memory_results = defaultdict(lambda: defaultdict(list))
    avg_insertion_cost_results = defaultdict(lambda: defaultdict(list))

    with open(logfile, 'r') as f:
        lines = f.readlines()

    dataset = None
    extent = None
    percentage = None
    insert_time = None
    avg_insertion_cost = None
    memory_size = None

    for line in lines:
        line = line.strip()

        if line.startswith('Dataset:'):
            dataset = line.split(':')[1].strip()
        elif line.startswith('Extent:'):
            extent = line.split(':')[1].strip()
        elif line.startswith('Percentage:'):
            match = re.search(r':\s*(\d+)%', line)
            if match:
                percentage = int(match.group(1))
        elif line.startswith('Insertion time'):
            match = re.search(r':\s*([\d.eE+-]+)', line)
            if match:
                insert_time = float(match.group(1))
        elif line.startswith('Average insertion cost'):
            match = re.search(r':\s*([\d.eE+-]+)', line)
            if match:
                avg_insertion_cost = float(match.group(1))
        elif line.startswith('Index size'):
            match = re.search(r':\s*([\d.eE+-]+)', line)
            if match:
                memory_size = float(match.group(1))
                if dataset and extent and percentage is not None and insert_time is not None:
                    memory_results[dataset][extent].append((percentage, memory_size))
                    if avg_insertion_cost is not None:
                        avg_insertion_cost_results[dataset][extent].append((percentage, avg_insertion_cost))
                    avg_insertion_cost = None

    return memory_results, avg_insertion_cost_results


def export_to_csv(memory_results, avg_insertion_cost_results):
    datasets = sorted(avg_insertion_cost_results.keys(),
                      key=lambda d: DATASET_ORDER.index(get_pretty_name(d))
                      if get_pretty_name(d) in DATASET_ORDER else 999)
    datasets = filter_datasets(datasets)

    with open('brail_scalability_table.csv', 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(['Dataset', 'Extent',
                         '20%_AvgCost', '40%_AvgCost', '60%_AvgCost', '80%_AvgCost', '100%_AvgCost',
                         '20%_Memory', '40%_Memory', '60%_Memory', '80%_Memory', '100%_Memory'])
        for dataset in datasets:
            for extent in sorted(avg_insertion_cost_results[dataset].keys()):
                avg_cost_dict = {p: c for p, c in avg_insertion_cost_results[dataset][extent]}
                memory_dict = {p: m for p, m in memory_results[dataset][extent]}
                row = [get_pretty_name(dataset), extent]
                for pct in [20, 40, 60, 80, 100]:
                    row.append(avg_cost_dict.get(pct, ''))
                for pct in [20, 40, 60, 80, 100]:
                    row.append(memory_dict.get(pct, ''))
                writer.writerow(row)
    print("Saved: brail_scalability_table.csv")


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python plot.py <logfile>")
        sys.exit(1)

    memory_results, avg_insertion_cost_results = parse_scalability_log(sys.argv[1])
    export_to_csv(memory_results, avg_insertion_cost_results)
