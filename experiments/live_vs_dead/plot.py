import sys
import os
import re
import pandas as pd
from collections import defaultdict

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
from plot_utils import DATASET_ORDER, get_pretty_name, filter_datasets

def parse_log(logfile):
    """Parse live vs dead log file."""
    results = defaultdict(lambda: defaultdict(dict))
    
    with open(logfile, 'r') as f:
        lines = f.readlines()
    
    dataset = None
    extent = None
    
    for line in lines:
        line = line.strip()
        
        if line.startswith('Dataset:'):
            dataset = line.split(':')[1].strip()
        elif line.startswith('Extent:'):
            extent = line.split(':')[1].strip()
        elif 'Total live insert time' in line:
            match = re.search(r':\s*([\d.eE+-]+)', line)
            if match and dataset and extent:
                results[dataset][extent]['live_insert_time'] = float(match.group(1))
        elif 'Total live remove time' in line:
            match = re.search(r':\s*([\d.eE+-]+)', line)
            if match and dataset and extent:
                results[dataset][extent]['live_remove_time'] = float(match.group(1))
        elif 'Total updating time (dead)' in line:
            match = re.search(r':\s*([\d.eE+-]+)', line)
            if match and dataset and extent:
                results[dataset][extent]['dead_update_time'] = float(match.group(1))
        elif 'Live query percentage' in line:
            match = re.search(r':\s*([\d.eE+-]+)', line)
            if match and dataset and extent:
                results[dataset][extent]['live_query_pct'] = float(match.group(1))
        elif 'Dead query percentage' in line:
            match = re.search(r':\s*([\d.eE+-]+)', line)
            if match and dataset and extent:
                results[dataset][extent]['dead_query_pct'] = float(match.group(1))
    
    return results

def export_csv(results):
    """Table 3: performance breakdown of LIT's Live and Dead index."""
    datasets = sorted(results.keys(),
                      key=lambda d: DATASET_ORDER.index(get_pretty_name(d))
                      if get_pretty_name(d) in DATASET_ORDER else 999)
    datasets = filter_datasets(datasets)

    if not datasets:
        print("No datasets found in log")
        return

    # Use single extent for comparison. Prefer dom0p0001, but fall back to whatever
    # exists in the logfile.
    preferred_extent = 'dom0p0001'
    extents_present = set()
    for dataset in datasets:
        extents_present.update(results.get(dataset, {}).keys())
    if preferred_extent in extents_present:
        extent = preferred_extent
    elif extents_present:
        extent = sorted(extents_present)[0]
    else:
        extent = preferred_extent

    update_csv_data = []
    query_csv_data = []
    for dataset in datasets:
        data = results[dataset].get(extent, {})

        live_insert_total = data.get('live_insert_time', 0)
        live_remove_total = data.get('live_remove_time', 0)
        dead_update_total = data.get('dead_update_time', 0)
        update_total = live_insert_total + live_remove_total + dead_update_total

        update_csv_data.append({
            'Dataset': get_pretty_name(dataset),
            'Live Ins. %': (live_insert_total / update_total * 100) if update_total else 0,
            'Live Rem. %': (live_remove_total / update_total * 100) if update_total else 0,
            'Dead %': (dead_update_total / update_total * 100) if update_total else 0,
        })

        query_csv_data.append({
            'Dataset': get_pretty_name(dataset),
            'Live %': data.get('live_query_pct', 0),
            'Dead %': data.get('dead_query_pct', 0),
        })

    pd.DataFrame(update_csv_data).to_csv('live_vs_dead_update_time.csv', index=False)
    print("Saved: live_vs_dead_update_time.csv")
    pd.DataFrame(query_csv_data).to_csv('live_vs_dead_query_time.csv', index=False)
    print("Saved: live_vs_dead_query_time.csv")

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python plot.py <logfile>")
        sys.exit(1)

    results = parse_log(sys.argv[1])
    export_csv(results)
