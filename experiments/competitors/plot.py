import sys
import os
import re
import numpy as np
import pandas as pd
from collections import defaultdict

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
from plot_utils import (
    DATASET_ORDER, FONT_SIZES, METHOD_COLORS, METHOD_NAMES, METHOD_HATCHES,
    apply_default_style, create_subplot_figure, finalize_and_save, get_pretty_name,
    plot_grouped_bar_chart,
    add_method_legend_to_figure, plot_capped_linear_bars,
    filter_datasets, find_matching_dataset
)

METHOD_ORDER = ['R-tree', 'HINT', 'B-rail', 'TIDY']
BAR_WIDTH_SIMPLE = 0.55   # default: 0.5
BAR_WIDTH_GROUPED = 0.16  # default: 0.14
METHOD_CONFIG = {
    'R-tree': {'name': METHOD_NAMES['R-tree'], 'color': METHOD_COLORS['R-tree'], 'hatch': METHOD_HATCHES['R-tree']},
    'HINT': {'name': METHOD_NAMES['HINT'], 'color': METHOD_COLORS['HINT'], 'hatch': METHOD_HATCHES['HINT']},
    'B-rail': {'name': METHOD_NAMES['B-rail'], 'color': METHOD_COLORS['B-rail'], 'hatch': METHOD_HATCHES['B-rail']},
    'TIDY': {'name': METHOD_NAMES['TIDY'], 'color': METHOD_COLORS['TIDY'], 'hatch': METHOD_HATCHES['TIDY']},
}

def parse_competitors_log(logfile):
    """Parse competitors log for R-tree, LIT (HINT), and BRAIL (B-rail).
    Only keeps the first complete result for each dataset/extent/method combo.
    """
    results = defaultdict(lambda: defaultdict(lambda: defaultdict(dict)))
    
    with open(logfile, 'r') as f:
        lines = f.readlines()
    
    dataset = None
    extent = None
    current_method = None
    update_time = None
    
    for line in lines:
        line = line.strip()
        
        if line.startswith('Dataset:'):
            dataset = line.split(':')[1].strip()
        elif line.startswith('Extent:'):
            extent = line.split(':')[1].strip()
        elif line == 'LIT' or line == 'LIT-STAB':
            current_method = 'HINT'
        elif line == 'R-tree' or line == 'R-tree-STAB':
            current_method = 'R-tree'
        elif line == 'BRAIL' or line == 'BRAIL-STAB':
            current_method = 'B-rail'
        elif current_method and 'Total updating time (dead)' in line:
            match = re.search(r':\s*([\d.eE+-]+)', line)
            if match:
                update_time = float(match.group(1))
        elif current_method and 'Total querying time (dead)' in line:
            match = re.search(r':\s*([\d.eE+-]+)', line)
            if match and dataset and extent:
                if 'query_time' not in results[dataset][extent][current_method]:
                    results[dataset][extent][current_method]['query_time'] = float(match.group(1))
                    results[dataset][extent][current_method]['update_time'] = update_time
        elif current_method and 'index size' in line and '[MB]' in line and 'Live' not in line:
            match = re.search(r':\s*([\d.eE+-]+)', line)
            if match:
                if 'memory' not in results[dataset][extent][current_method]:
                    results[dataset][extent][current_method]['memory'] = float(match.group(1))
                current_method = None
                update_time = None

    return results


def parse_brail_variants_log(logfile):
    """Parse brail_variants log, extracting TIDY-Full (full pruning) as B-rail."""
    results = defaultdict(lambda: defaultdict(lambda: defaultdict(dict)))

    with open(logfile, 'r') as f:
        lines = f.readlines()

    dataset = None
    extent = None
    in_tidy_full = False
    update_time = None

    for line in lines:
        line = line.strip()

        if line.startswith('Dataset:'):
            dataset = line.split(':')[1].strip()
        elif line.startswith('Extent:'):
            extent = line.split(':')[1].strip()
            in_tidy_full = False
            update_time = None
        elif '--- TIDY-Full (full pruning) ---' in line:
            in_tidy_full = True
            update_time = None
        elif line.startswith('--- '):
            in_tidy_full = False
        elif in_tidy_full and 'Total updating time (dead)' in line:
            match = re.search(r':\s*([\d.eE+-]+)', line)
            if match:
                update_time = float(match.group(1))
        elif in_tidy_full and 'Total querying time Q1' in line:
            match = re.search(r':\s*([\d.eE+-]+)', line)
            if match and dataset and extent:
                results[dataset][extent].setdefault('B-rail', {})['q1_time'] = float(match.group(1))
        elif in_tidy_full and 'Total querying time Q2' in line:
            match = re.search(r':\s*([\d.eE+-]+)', line)
            if match and dataset and extent and update_time is not None:
                entry = results[dataset][extent].setdefault('B-rail', {})
                if 'query_time' not in entry:
                    entry['q2_time'] = float(match.group(1))
                    entry['query_time'] = entry.get('q1_time', 0) + entry['q2_time']
                    entry['update_time'] = update_time
        elif in_tidy_full and 'Index size' in line and '[MB]' in line:
            match = re.search(r':\s*([\d.eE+-]+)', line)
            if match and dataset and extent:
                if 'memory' not in results[dataset][extent]['B-rail']:
                    results[dataset][extent]['B-rail']['memory'] = float(match.group(1))
                in_tidy_full = False

    return results


def get_tidy_key(tidy_results, dataset):
    if dataset in tidy_results:
        return dataset
    return find_matching_dataset(tidy_results, dataset)

def parse_learned_log(logfile):
    """Parse the TIDY-LEARNED log for the TIDY results."""
    results = defaultdict(lambda: defaultdict(dict))
    
    with open(logfile, 'r') as f:
        lines = f.readlines()
    
    dataset = None
    extent = None
    update_time = None
    in_tidy = False
    
    for line in lines:
        line = line.strip()
        
        if line.startswith('Dataset:'):
            dataset = line.split(':')[1].strip()
        elif line.startswith('Extent:'):
            extent = line.split(':')[1].strip()
        elif line.startswith('TIDY-LEARNED') and 'index size' not in line:
            in_tidy = True
            update_time = None
        elif in_tidy and 'Total updating time (dead)' in line:
            match = re.search(r':\s*([\d.eE+-]+)', line)
            if match:
                update_time = float(match.group(1))
        elif in_tidy and 'Total querying time (dead)' in line:
            match = re.search(r':\s*([\d.eE+-]+)', line)
            if match and dataset and extent and update_time is not None:
                if 'query_time' not in results[dataset][extent]:
                    results[dataset][extent]['query_time'] = float(match.group(1))
                    results[dataset][extent]['update_time'] = update_time
        elif in_tidy and 'index size' in line and '[MB]' in line and 'TIDY' in line:
            match = re.search(r':\s*([\d.eE+-]+)', line)
            if match and dataset and extent:
                if 'memory' not in results[dataset][extent]:
                    results[dataset][extent]['memory'] = float(match.group(1))
                in_tidy = False

    return results

def get_extent_display_name(extent):
    if extent == 'snapshot':
        return 'S'
    mapping = {'dom0p0001': '0.01%', 'dom0p001': '0.1%', 'dom0p01': '1%'}
    return mapping.get(extent, extent)

def save_data_to_csv(competitors, tidy_results):
    """Save Table 9 (memory) data to CSV."""
    datasets = sorted(competitors.keys(),
                      key=lambda d: DATASET_ORDER.index(get_pretty_name(d))
                      if get_pretty_name(d) in DATASET_ORDER else 999)
    datasets = filter_datasets(datasets)

    memory_data = []
    for dataset in datasets:
        tidy_key = get_tidy_key(tidy_results, dataset)
        extent = 'dom0p0001'
        row = {'Dataset': get_pretty_name(dataset)}
        for method in METHOD_ORDER:
            if method == 'TIDY':
                tidy_data = tidy_results.get(tidy_key, {}).get(extent, {}) if tidy_key else {}
                row[method] = tidy_data.get('memory', 0)
            else:
                method_data = competitors[dataset].get(extent, {}).get(method, {})
                row[method] = method_data.get('memory', 0)
        memory_data.append(row)
    
    df_memory = pd.DataFrame(memory_data)
    df_memory.to_csv('competitors_memory.csv', index=False)
    print("Saved: competitors_memory.csv")

def plot_results(competitors, tidy_results):
    apply_default_style()
    
    datasets = sorted(competitors.keys(),
                      key=lambda d: DATASET_ORDER.index(get_pretty_name(d))
                      if get_pretty_name(d) in DATASET_ORDER else 999)
    datasets = filter_datasets(datasets)
    if not datasets:
        print("No datasets found in competitors log")
        return
    
    extents = ['snapshot', 'dom0p0001', 'dom0p001', 'dom0p01']
    n_datasets = len(datasets)

    # 1. Update time plot (linear scale, capped) — Figure 9(a)
    fig2, axes2 = create_subplot_figure(n_datasets, suptitle='Dead Index Update Time (Linear Scale)', add_legend=True)
    
    for idx, dataset in enumerate(datasets):
        ax = axes2[idx]
        tidy_key = get_tidy_key(tidy_results, dataset)
        extent = 'dom0p0001'
        
        vals = []
        for method in METHOD_ORDER:
            if method == 'TIDY':
                tidy_data = tidy_results.get(tidy_key, {}).get(extent, {}) if tidy_key else {}
                vals.append(tidy_data.get('update_time', 0))
            else:
                method_data = competitors[dataset].get(extent, {}).get(method, {})
                vals.append(method_data.get('update_time', 0))
        
        vals_sorted = sorted(vals)
        if len(vals_sorted) >= 2:
            ymax = vals_sorted[-2] * 1.2
        else:
            ymax = max(vals) * 1.1 if vals else 1
        
        colors = [METHOD_CONFIG[m]['color'] for m in METHOD_ORDER]
        hatches = [METHOD_CONFIG[m]['hatch'] for m in METHOD_ORDER]
        labels = [METHOD_CONFIG[m]['name'] for m in METHOD_ORDER]
        plot_capped_linear_bars(ax, METHOD_ORDER, vals, colors, labels, ymax, hatches=hatches, rotation=45,
                                bar_width=BAR_WIDTH_SIMPLE)
        
        # Add text labels above R-tree bars showing actual values
        x_positions = np.arange(len(METHOD_ORDER))
        for i, (method, val) in enumerate(zip(METHOD_ORDER, vals)):
            if method == 'R-tree' and val > ymax:
                # Format the value appropriately
                if val >= 1000:
                    text = f'{val:.0f}s'
                elif val >= 100:
                    text = f'{val:.1f}s'
                else:
                    text = f'{val:.2f}s'
                ax.text(x_positions[i], ymax * 0.95, text, 
                       ha='center', va='top', fontsize=FONT_SIZES['tick']-5, fontweight='bold')
        
        if idx == 0:
            ax.set_ylabel('Update Time (s)', fontsize=FONT_SIZES['axis_label'])
        ax.set_title(get_pretty_name(dataset), fontsize=FONT_SIZES['title'], pad=10)

    from matplotlib.ticker import FuncFormatter
    def fmt_max2dec(val, _):
        if val == int(val):
            return f'{int(val)}'
        s = f'{val:.2f}'.rstrip('0').rstrip('.')
        return s
    for ax in axes2:
        ax.yaxis.set_major_formatter(FuncFormatter(fmt_max2dec))
    add_method_legend_to_figure(fig2, METHOD_ORDER, METHOD_CONFIG)
    finalize_and_save(fig2, 'competitors_update_times_linear.pdf')

    # 2. Query time plot (grouped by extent) — Figure 9(b)
    fig3, axes3 = create_subplot_figure(n_datasets, suptitle='Dead Index Query Time', add_legend=True)
    
    for idx, dataset in enumerate(datasets):
        ax = axes3[idx]
        tidy_key = get_tidy_key(tidy_results, dataset)
        extent_labels = [get_extent_display_name(e) for e in extents]
        
        data = {}
        for method in METHOD_ORDER:
            data[method] = []
            for extent in extents:
                if method == 'TIDY':
                    tidy_data = tidy_results.get(tidy_key, {}).get(extent, {}) if tidy_key else {}
                    data[method].append(tidy_data.get('query_time', 0))
                else:
                    method_data = competitors[dataset].get(extent, {}).get(method, {})
                    data[method].append(method_data.get('query_time', 0))
        
        bars_dict = plot_grouped_bar_chart(ax, extent_labels, METHOD_ORDER, data, method_config=METHOD_CONFIG,
                               title=get_pretty_name(dataset), xlabel='Domain Extent', log_scale=True,
                               ylabel='Query Time (s)' if idx == 0 else None,
                               bar_width=BAR_WIDTH_GROUPED)

        # Add speedup annotations (competitor / TIDY) on top of non-TIDY bars, rotated 90°
        tidy_vals = data['TIDY']
        speedup_fontsize = FONT_SIZES['tick'] - 10
        for method in METHOD_ORDER:
            if method == 'TIDY':
                continue
            bars = bars_dict[method]
            method_vals = data[method]
            for bar, method_val, tidy_val in zip(bars, method_vals, tidy_vals):
                if tidy_val > 0 and method_val > 0:
                    speedup = method_val / tidy_val
                    label = f'{speedup:.0f}x' if speedup >= 10 else f'{speedup:.1f}x'
                    bar_x = bar.get_x() + bar.get_width() / 2
                    bar_top = bar.get_height()
                    ax.text(bar_x + bar.get_width() * 0.1, bar_top * 1.15, label,
                            ha='center', va='bottom',
                            fontsize=speedup_fontsize,
                            fontweight='bold',
                            rotation=90,
                            clip_on=False)

        # Extend the upper y-limit just enough for the rotated text
        ymin, ymax_curr = ax.get_ylim()
        ax.set_ylim(ymin, ymax_curr * 3)

    add_method_legend_to_figure(fig3, METHOD_ORDER, METHOD_CONFIG)
    finalize_and_save(fig3, 'competitors_query_times.pdf')

if __name__ == '__main__':
    if len(sys.argv) != 4:
        print("Usage: python plot.py <competitors_logfile> <learned_logfile> <brail_variants_logfile>")
        sys.exit(1)

    competitors = parse_competitors_log(sys.argv[1])
    tidy_results = parse_learned_log(sys.argv[2])
    brail_data = parse_brail_variants_log(sys.argv[3])

    # Merge B-rail data from brail_variants into competitors
    for dataset in brail_data:
        for extent in brail_data[dataset]:
            competitors[dataset][extent]['B-rail'] = brail_data[dataset][extent]['B-rail']

    save_data_to_csv(competitors, tidy_results)
    plot_results(competitors, tidy_results)
