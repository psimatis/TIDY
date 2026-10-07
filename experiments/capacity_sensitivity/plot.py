import sys
import os
import re
import pandas as pd
from collections import defaultdict

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
from plot_utils import DATASET_ORDER, get_pretty_name, filter_datasets

def parse_log(logfile):
    results = defaultdict(lambda: defaultdict(lambda: defaultdict(dict)))

    with open(logfile, 'r') as f:
        lines = f.readlines()

    dataset = None
    extent = None
    capacity = None

    for line in lines:
        line = line.strip()

        if line.startswith('Dataset:'):
            dataset = line.split(':', 1)[1].strip()
        elif line.startswith('Extent:'):
            extent = line.split(':', 1)[1].strip()
        elif line.startswith('Capacity:'):
            capacity = line.split(':', 1)[1].strip()
        elif 'Total updating time (dead)' in line and '[secs]' in line:
            match = re.search(r':\s*([\d.eE+-]+)', line)
            if match and dataset and extent and capacity:
                results[dataset][extent][capacity]['update_time'] = float(match.group(1))
        elif 'Total querying time          [secs]' in line:
            match = re.search(r':\s*([\d.eE+-]+)', line)
            if match and dataset and extent and capacity:
                results[dataset][extent][capacity]['query_time'] = float(match.group(1))

    return results

def export_csv(results):
    """Table 5: B-Rail's sensitivity to the node capacity C."""
    datasets = sorted(results.keys(), key=lambda d: DATASET_ORDER.index(get_pretty_name(d)) if get_pretty_name(d) in DATASET_ORDER else 999)
    datasets = filter_datasets(datasets)

    if not datasets:
        print("No datasets found in log")
        return

    # Use whichever extent is present in the data (prefer dom0p0001, fall back to first found)
    extents_found = set()
    for dataset in datasets:
        extents_found.update(results[dataset].keys())
    extent = 'dom0p0001' if 'dom0p0001' in extents_found else sorted(extents_found)[0]

    capacities_present = set()
    for dataset in datasets:
        for cap in results[dataset].get(extent, {}).keys():
            capacities_present.add(cap)
    capacity_order = sorted(capacities_present, key=lambda x: int(x))

    update_csv_data = []
    query_csv_data = []
    for dataset in datasets:
        data = results[dataset].get(extent, {})

        update_row = {'Dataset': get_pretty_name(dataset)}
        query_row = {'Dataset': get_pretty_name(dataset)}
        for cap in capacity_order:
            if cap in data:
                update_row[cap] = data[cap].get('update_time', 0)
                query_row[cap] = data[cap].get('query_time', 0)
        update_csv_data.append(update_row)
        query_csv_data.append(query_row)

    pd.DataFrame(update_csv_data).to_csv('capacity_sensitivity_update_time.csv', index=False)
    print("Saved: capacity_sensitivity_update_time.csv")
    pd.DataFrame(query_csv_data).to_csv('capacity_sensitivity_query_time.csv', index=False)
    print("Saved: capacity_sensitivity_query_time.csv")

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python plot.py <logfile>")
        sys.exit(1)

    results = parse_log(sys.argv[1])
    export_csv(results)
