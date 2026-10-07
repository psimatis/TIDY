import sys
import os
import re
import pandas as pd
from collections import defaultdict

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
from plot_utils import DATASET_ORDER, get_pretty_name, filter_datasets

def parse_log(logfile):
    """Parse rho sensitivity log file."""
    results = defaultdict(lambda: defaultdict(lambda: defaultdict(dict)))

    with open(logfile, 'r') as f:
        lines = f.readlines()

    dataset = None
    extent = None
    rho = None
    pending_dataset = None

    for raw_line in lines:
        line = raw_line.rstrip('\n')

        if line.startswith('Dataset:'):
            pending_dataset = line.split(':', 1)[1].strip()
            dataset = None
            extent = None
            continue

        if pending_dataset is not None:
            if 'Extent:' in line:
                left, right = line.split('Extent:', 1)
                pending_dataset = (pending_dataset + left).strip()
                pending_dataset = re.sub(r'\s+', '', pending_dataset)
                dataset = pending_dataset
                extent = right.strip().split()[0]
                pending_dataset = None
                continue
            else:
                pending_dataset = (pending_dataset + ' ' + line).strip()
                continue

        if line.startswith('Extent:'):
            extent = line.split(':', 1)[1].strip().split()[0]
            continue

        if 'Rho (outlier fraction)' in line:
            match = re.search(r':\s*([\d.eE+-]+)', line)
            if match:
                rho = match.group(1)
            continue

        if 'Total updating time (dead)' in line and '[secs]' in line:
            match = re.search(r':\s*([\d.eE+-]+)', line)
            if match and dataset and extent and rho:
                results[dataset][extent][rho]['update_time'] = float(match.group(1))
            continue

        if 'Total querying time (dead)' in line and '[secs]' in line:
            match = re.search(r':\s*([\d.eE+-]+)', line)
            if match and dataset and extent and rho:
                results[dataset][extent][rho]['query_time'] = float(match.group(1))
            continue

        if 'Final Delta' in line:
            match = re.search(r':\s*([\d.eE+-]+)', line)
            if match and dataset and extent and rho:
                results[dataset][extent][rho]['delta'] = float(match.group(1))
            continue

        if 'Short intervals' in line:
            match = re.search(r':\s*(\d+)', line)
            if match and dataset and extent and rho:
                results[dataset][extent][rho]['short_count'] = int(match.group(1))
            continue

        if 'Long intervals' in line:
            match = re.search(r':\s*(\d+)', line)
            if match and dataset and extent and rho:
                results[dataset][extent][rho]['long_count'] = int(match.group(1))
            continue

    return results

def export_csv(results):
    """Table 8: TIDY's sensitivity to the outlier ratio rho."""
    datasets = sorted(results.keys(),
                      key=lambda d: DATASET_ORDER.index(get_pretty_name(d))
                      if get_pretty_name(d) in DATASET_ORDER else 999)
    datasets = filter_datasets(datasets)

    if not datasets:
        print("No datasets found in log")
        return

    # Pick best available extent (prefer dom0p0001, then dom0p001, dom0p01; skip snapshot)
    extent_preference = ['dom0p0001', 'dom0p001', 'dom0p01']
    all_extents = set()
    for dataset in datasets:
        all_extents.update(results[dataset].keys())
    extent = next((e for e in extent_preference if e in all_extents), (all_extents - {'snapshot'}).pop() if all_extents - {'snapshot'} else 'dom0p0001')

    rho_values_present = set()
    for dataset in datasets:
        for rho in results[dataset].get(extent, {}).keys():
            rho_values_present.add(rho)
    rho_order = sorted(rho_values_present, key=lambda x: float(x))

    update_csv_data = []
    query_csv_data = []
    for dataset in datasets:
        data = results[dataset].get(extent, {})

        update_row = {'Dataset': get_pretty_name(dataset)}
        query_row = {'Dataset': get_pretty_name(dataset)}
        for rho in rho_order:
            if rho in data:
                update_row[rho] = data[rho].get('update_time', 0)
                query_row[rho] = data[rho].get('query_time', 0)
        update_csv_data.append(update_row)
        query_csv_data.append(query_row)

    pd.DataFrame(update_csv_data).to_csv('rho_sensitivity_update_time.csv', index=False)
    print("Saved: rho_sensitivity_update_time.csv")
    pd.DataFrame(query_csv_data).to_csv('rho_sensitivity_query_time.csv', index=False)
    print("Saved: rho_sensitivity_query_time.csv")

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python plot.py <logfile>")
        sys.exit(1)

    results = parse_log(sys.argv[1])
    export_csv(results)
