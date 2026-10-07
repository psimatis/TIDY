import sys
import os
import re
import numpy as np
from collections import defaultdict

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from plot_utils import (
    get_extent_pretty_name,
    apply_default_style, create_subplot_figure, plot_simple_bar_chart, plot_grouped_bar_chart,
    add_figure_legend, finalize_and_save, sync_yaxis_limits,
    METHOD_NAMES, METHOD_COLORS, METHOD_HATCHES,
    get_pretty_name, export_to_csv,
)

METHOD_CONFIG = {
    'btree_tlx.exe': {'name': METHOD_NAMES['TLX'], 'color': METHOD_COLORS['TLX'], 'hatch': METHOD_HATCHES['TLX']},
    'btree_quit.exe': {'name': METHOD_NAMES['QUIT-dup'], 'color': METHOD_COLORS['QUIT-dup'], 'hatch': METHOD_HATCHES['QUIT-dup']},
    'brail.exe': {'name': METHOD_NAMES['BrailTree'], 'color': METHOD_COLORS['BrailTree'], 'hatch': METHOD_HATCHES['BrailTree']},
}
METHOD_ORDER = ['btree_tlx.exe', 'btree_quit.exe', 'brail.exe']

def parse_log(logfile):
    experiments = []
    current_exp = None
    current_method = None

    with open(logfile, 'r') as f:
        for line in f:
            line = line.strip()

            if line.startswith("Dataset:"):
                if current_exp and current_exp['methods']:
                    experiments.append(current_exp)
                current_exp = {
                    'dataset': line.split(":", 1)[1].strip(),
                    'extent': None,
                    'methods': {}
                }
            elif line.startswith("Extent:") and current_exp:
                current_exp['extent'] = line.split(":", 1)[1].strip()
            elif line.startswith("--- ") and line.endswith(" ---"):
                current_method = line[4:-4]
                if current_exp and current_method not in current_exp['methods']:
                    current_exp['methods'][current_method] = {'update_time': 0, 'query_time': 0, 'num_queries': 0, 'total_result': 0, 'memory_mb': 0}
            elif "updating time (dead)" in line and current_exp and current_method:
                match = re.search(r':\s*([\d.e+-]+)', line)
                if match:
                    current_exp['methods'][current_method]['update_time'] = float(match.group(1))
            elif "querying time (dead)" in line and current_exp and current_method:
                match = re.search(r':\s*([\d.e+-]+)', line)
                if match:
                    current_exp['methods'][current_method]['query_time'] = float(match.group(1))
            elif "Num of queries" in line and current_exp and current_method:
                match = re.search(r':\s*(\d+)', line)
                if match:
                    current_exp['methods'][current_method]['num_queries'] = int(match.group(1))
            elif "Total result" in line and current_exp and current_method:
                match = re.search(r':\s*(\d+)', line)
                if match:
                    current_exp['methods'][current_method]['total_result'] = int(match.group(1))
            elif "Index size" in line and "[MB]" in line and current_exp and current_method:
                match = re.search(r':\s*([\d.e+-]+)', line)
                if match:
                    current_exp['methods'][current_method]['memory_mb'] = float(match.group(1))

    if current_exp and current_exp['methods']:
        experiments.append(current_exp)

    return experiments


def plot_results(experiments, outdir):

    by_dataset = defaultdict(list)
    for exp in experiments:
        pretty = get_pretty_name(exp['dataset'])
        by_dataset[pretty].append(exp)

    from plot_utils import DATASET_ORDER
    datasets = sorted(by_dataset.keys(), key=lambda x: DATASET_ORDER.index(x) if x in DATASET_ORDER else 999)

    apply_default_style()

    n_datasets = len(by_dataset)

    # Update time
    fig, axes = create_subplot_figure(n_datasets, suptitle='Update Time', add_legend=True)

    for idx, dataset_name in enumerate(datasets):
        exps = by_dataset[dataset_name]
        ax = axes[idx]
        vals = []
        for method in METHOD_ORDER:
            method_vals = [e['methods'].get(method, {}).get('update_time', 0) for e in exps]
            avg_val = np.mean(method_vals) if method_vals else 0
            vals.append(avg_val)

        colors = [METHOD_CONFIG[m]['color'] for m in METHOD_ORDER]
        hatches = [METHOD_CONFIG[m]['hatch'] for m in METHOD_ORDER]
        labels = [METHOD_CONFIG[m]['name'] for m in METHOD_ORDER]
        plot_simple_bar_chart(ax, METHOD_ORDER, vals, colors=colors, hatches=hatches, labels=labels,
                              title=dataset_name, log_scale=True, ylabel='Update Time (s)' if idx == 0 else None,
                              xlabel=' ')
        ax.set_ylim(bottom=0.01)

    sync_yaxis_limits(axes)
    handles, labels_legend = axes[0].get_legend_handles_labels()
    add_figure_legend(fig, handles, labels_legend)

    finalize_and_save(fig, os.path.join(outdir, 'btree_update_time.pdf'))

    # Query time
    fig, axes = create_subplot_figure(n_datasets, suptitle='Query Time', add_legend=True)

    EXTENT_ORDER = ['snapshot', 'dom0p0001', 'dom0p001', 'dom0p01']

    for idx, dataset_name in enumerate(datasets):
        exps = by_dataset[dataset_name]
        ax = axes[idx]
        exps = sorted(exps, key=lambda x: EXTENT_ORDER.index(x['extent']) if x['extent'] in EXTENT_ORDER else 999)
        extents = [get_extent_pretty_name(e['extent']) for e in exps]

        data = {method: [e['methods'].get(method, {}).get('query_time', 0) for e in exps] for method in METHOD_ORDER}

        plot_grouped_bar_chart(ax, extents, METHOD_ORDER, data, method_config=METHOD_CONFIG,
                               title=dataset_name, xlabel='Domain Extent', log_scale=True,
                               ylabel='Query Time (s)' if idx == 0 else None)

    sync_yaxis_limits(axes)
    handles, labels_legend = axes[0].get_legend_handles_labels()
    add_figure_legend(fig, handles, labels_legend)

    finalize_and_save(fig, os.path.join(outdir, 'btree_query_time.pdf'))

    # Memory usage data for Table 7 (no figure in the paper)
    memory_data = []
    for dataset_name in datasets:
        exps = by_dataset[dataset_name]
        for method in METHOD_ORDER:
            method_vals = [e['methods'].get(method, {}).get('memory_mb', 0) for e in exps]
            avg_val = np.mean(method_vals) if method_vals else 0
            memory_data.append({
                'dataset': dataset_name,
                'method': METHOD_CONFIG[method]['name'],
                'memory_mb': avg_val
            })

    export_to_csv(os.path.join(outdir, 'btree_memory.csv'), memory_data, ['dataset', 'method', 'memory_mb'])


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: plot.py <logfile>")
        sys.exit(1)

    outdir = os.path.dirname(os.path.abspath(sys.argv[1]))
    experiments = parse_log(sys.argv[1])
    if experiments:
        plot_results(experiments, outdir)
    else:
        print("No experiments found in log file")
