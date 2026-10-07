import sys
import os
import re
import csv
from collections import defaultdict
import numpy as np
import matplotlib.patches as mpatches
from matplotlib.ticker import MaxNLocator, LogLocator, NullFormatter

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
from plot_utils import (
    DATASET_ORDER, FONT_SIZES, METHOD_COLORS, METHOD_NAMES, METHOD_HATCHES, LAYOUT_CONFIG,
    apply_default_style, create_subplot_figure, finalize_and_save,
    plot_simple_bar_chart,
    add_method_legend_to_figure, add_figure_legend,
    get_pretty_name, filter_datasets, get_extent_pretty_name
)

METHOD_ORDER = ['TIDY-NoMax', 'TIDY-Full']
METHOD_CONFIG = {
    'TIDY-NoMax': {'color': METHOD_COLORS['TIDY-NoMax'], 'name': METHOD_NAMES['TIDY-NoMax'], 'hatch': METHOD_HATCHES['TIDY-NoMax']},
    'TIDY-Full': {'color': METHOD_COLORS['TIDY-Full'], 'name': METHOD_NAMES['TIDY-Full'], 'hatch': METHOD_HATCHES['TIDY-Full']},
}
Q2_HATCH = 'xx'
Q2_ALPHA = 0.65

EXTENT_ORDER = ['snapshot', 'dom0p0001', 'dom0p001', 'dom0p01']
LOG_YMIN = 1e-4


def parse_brail_variants_log(logfile):
    results = defaultdict(lambda: defaultdict(dict))
    
    with open(logfile, 'r') as f:
        lines = f.readlines()
    
    dataset = None
    extent = None
    current_variant = None
    update_time = None
    
    for line in lines:
        line = line.strip()
        
        if line.startswith('Dataset:'):
            dataset = line.split(':')[1].strip()
        elif line.startswith('Extent:'):
            extent = line.split(':')[1].strip()
        elif line.startswith('--- TIDY-NoMax'):
            current_variant = 'TIDY-NoMax'
        elif line.startswith('--- TIDY-Full'):
            current_variant = 'TIDY-Full'
        elif line.startswith('--- Live'):
            current_variant = None
        elif current_variant and dataset and extent:
            # Figure 6a uses the isolated rebuild time; see the comment in main.cpp
            if 'Isolated updating time (dead)' in line:
                match = re.search(r':\s*([\d.eE+-]+)', line)
                if match:
                    update_time = float(match.group(1))
                    results[dataset][extent].setdefault(current_variant, {})['update_time'] = update_time
            elif 'Total querying time Q1' in line:
                match = re.search(r':\s*([\d.eE+-]+)', line)
                if match:
                    results[dataset][extent].setdefault(current_variant, {})['q1_time'] = float(match.group(1))
            elif 'Total querying time Q2' in line:
                match = re.search(r':\s*([\d.eE+-]+)', line)
                if match:
                    entry = results[dataset][extent].setdefault(current_variant, {})
                    entry['q2_time'] = float(match.group(1))
                    entry['query_time'] = entry.get('q1_time', 0) + entry['q2_time']
            elif 'Index size' in line and '[MB]' in line:
                match = re.search(r':\s*([\d.eE+-]+)', line)
                if match:
                    results[dataset][extent].setdefault(current_variant, {})['memory_mb'] = float(match.group(1))
    
    return results


def _build_stacked_figure(results):
    datasets = sorted(results.keys(), key=lambda d: DATASET_ORDER.index(get_pretty_name(d)) if get_pretty_name(d) in DATASET_ORDER else 999)
    datasets = filter_datasets(datasets)
    if not datasets:
        return None, None

    fig, axes = create_subplot_figure(len(datasets), suptitle='BRAIL Variants: Query Time (Q1+Q2 Breakdown)', add_legend=True)

    bar_width = LAYOUT_CONFIG['bar_width_grouped']
    n_methods = len(METHOD_ORDER)
    extents = EXTENT_ORDER

    for idx, dataset in enumerate(datasets):
        ax = axes[idx]
        extent_labels = [get_extent_pretty_name(e) for e in extents]
        n_groups = len(extent_labels)
        x = np.arange(n_groups)
        total_vals = {method: [] for method in METHOD_ORDER}

        for m_idx, method in enumerate(METHOD_ORDER):
            q1_vals = []
            q2_vals = []
            for extent in extents:
                data = results.get(dataset, {}).get(extent, {}).get(method, {})
                q1_vals.append(data.get('q1_time', 0))
                q2_vals.append(data.get('q2_time', 0))

            offset = (m_idx - (n_methods - 1) / 2) * bar_width
            color = METHOD_CONFIG[method]['color']
            q1_hatch = METHOD_CONFIG[method]['hatch']

            ax.bar(x + offset, q1_vals, bar_width, color=color, hatch=q1_hatch, edgecolor='black', linewidth=0.5)
            ax.bar(x + offset, q2_vals, bar_width, bottom=q1_vals, color=color, hatch=Q2_HATCH, alpha=Q2_ALPHA, edgecolor='black', linewidth=0.5)

            total_vals[method] = [q1 + q2 for q1, q2 in zip(q1_vals, q2_vals)]

        baseline_vals = total_vals['TIDY-NoMax']
        method_vals = total_vals['TIDY-Full']
        for g_idx in range(n_groups):
            base_val = baseline_vals[g_idx]
            method_val = method_vals[g_idx]
            if method_val > 0 and base_val > 0:
                speedup = base_val / method_val
                max_height = max(base_val, method_val)
                ax.text(x[g_idx], max_height * 1.1, f'{speedup:.1f}x', ha='center', va='bottom', fontsize=FONT_SIZES['tick'] - 6, fontweight='bold')

        ax.set_yscale('log')
        ax.yaxis.set_major_locator(LogLocator(base=10, numticks=15))
        ax.yaxis.set_minor_locator(LogLocator(base=10, subs='auto', numticks=15))
        ax.yaxis.set_minor_formatter(NullFormatter())
        ax.set_xticks(x)
        ax.set_xticklabels(extent_labels, fontsize=FONT_SIZES['tick'])
        ax.set_xlabel('Domain Extent', fontsize=FONT_SIZES['axis_label'])
        ax.set_title(get_pretty_name(dataset), fontsize=FONT_SIZES['title'], pad=10)
        if idx == 0:
            ax.set_ylabel('Query Time (s)', fontsize=FONT_SIZES['axis_label'])
        ax.grid(axis='y', alpha=0.3)

    global_ymax = LOG_YMIN
    for ax in axes:
        _, ymax = ax.get_ylim()
        global_ymax = max(global_ymax, ymax)

    label_ymax = global_ymax * 1.5
    for ax in axes:
        ax.set_ylim(LOG_YMIN, label_ymax)
        ax.yaxis.set_major_locator(LogLocator(base=10, numticks=15))
        ax.yaxis.set_minor_locator(LogLocator(base=10, subs='auto', numticks=15))
        ax.yaxis.set_minor_formatter(NullFormatter())

    handles = []
    labels = []
    for method in METHOD_ORDER:
        color = METHOD_CONFIG[method]['color']
        name = METHOD_CONFIG[method]['name']
        handles.append(mpatches.Patch(facecolor=color, hatch=METHOD_CONFIG[method]['hatch'], edgecolor='black', linewidth=0.5))
        labels.append(f'{name} Q1')
        handles.append(mpatches.Patch(facecolor=color, hatch=Q2_HATCH, alpha=Q2_ALPHA, edgecolor='black', linewidth=0.5))
        labels.append(f'{name} Q2')
    add_figure_legend(fig, handles, labels, ncol=4)

    return fig, datasets


def plot_stacked_query_times(results):
    apply_default_style()

    fig, datasets = _build_stacked_figure(results)
    if fig is None:
        print("No data to plot")
        return

    finalize_and_save(fig, 'brail_variants_stacked_query_time.pdf')
    print("Saved: brail_variants_stacked_query_time.pdf")


def plot_results(results):
    apply_default_style()
    
    datasets = sorted(results.keys(), key=lambda d: DATASET_ORDER.index(get_pretty_name(d)) if get_pretty_name(d) in DATASET_ORDER else 999)
    datasets = filter_datasets(datasets)
    
    if not datasets:
        print("No data to plot")
        return
    
    n_datasets = len(datasets)

    # Update Time Plot (simple bar per dataset) — Figure 6a
    fig2, axes2 = create_subplot_figure(n_datasets, suptitle='BRAIL Variants: Update Time', add_legend=True)
    
    for idx, dataset in enumerate(datasets):
        ax = axes2[idx]
        extent = 'dom0p0001'
        
        vals = []
        for method in METHOD_ORDER:
            val = results.get(dataset, {}).get(extent, {}).get(method, {}).get('update_time', 0)
            vals.append(val)
        
        colors = [METHOD_CONFIG[m]['color'] for m in METHOD_ORDER]
        hatches = [METHOD_CONFIG[m]['hatch'] for m in METHOD_ORDER]
        labels = [METHOD_CONFIG[m]['name'] for m in METHOD_ORDER]
        plot_simple_bar_chart(ax, METHOD_ORDER, vals, colors=colors, hatches=hatches, labels=labels,
                              title=get_pretty_name(dataset), log_scale=False,
                              ylabel='Update Time (s)' if idx == 0 else None)
        
        # Small values (WILDFIRES, WEBKIT) get a x10^-3 offset so tick labels stay short
        ax.yaxis.set_major_locator(MaxNLocator(nbins=4))
        ax.ticklabel_format(axis='y', style='sci', scilimits=(-3, 3), useMathText=True)
        ax.yaxis.get_offset_text().set_fontsize(FONT_SIZES['bar_label'])
        ax.yaxis.get_offset_text().set_x(-0.2)
    
    add_method_legend_to_figure(fig2, METHOD_ORDER, METHOD_CONFIG)
    finalize_and_save(fig2, 'brail_variants_update_time.pdf')

    # Memory usage data for Table 4 (no figure in the paper)
    with open('brail_variants_memory.csv', 'w', newline='') as csvfile:
        writer = csv.writer(csvfile)
        writer.writerow(['Dataset', 'Method', 'Memory (MB)'])
        for dataset in datasets:
            extent = 'dom0p0001'
            for method in METHOD_ORDER:
                val = results.get(dataset, {}).get(extent, {}).get(method, {}).get('memory_mb', 0)
                writer.writerow([get_pretty_name(dataset), METHOD_CONFIG[method]['name'], val])
    print("Data saved as brail_variants_memory.csv")

    plot_stacked_query_times(results)


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python plot.py <logfile>")
        sys.exit(1)
    
    results = parse_brail_variants_log(sys.argv[1])
    plot_results(results)
