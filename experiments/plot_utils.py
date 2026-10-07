import matplotlib.pyplot as plt
import numpy as np
import re
import csv
from collections import defaultdict
from matplotlib.ticker import LogLocator, NullFormatter

DATASET_ORDER = ['BIKES', 'FLIGHTS', 'TAXIS', 'WILDFIRES', 'WEBKIT']

def filter_datasets(datasets):
    return [d for d in datasets if get_pretty_name(d)]

METHOD_COLORS = {
    'LIT': '#A0A0A0',
    'R-tree': '#D0D0D0',
    'TIDY': '#000000',
    'TIDY-ORACLE': '#C0C0C0',
    'TIDY-ADAPTIVE': '#808080',
    'TIDY-LEARNED': '#404040',
    'TLX': '#E0E0E0',
    'QUIT-dup': '#808080',
    'BrailTree': '#303030',
    'TIDY-NoMax': '#A0A0A0',
    'TIDY-Full': '#505050',
    'HINT': '#A0A0A0',
    'B-rail': '#707070',
}

METHOD_HATCHES = {
    'LIT': '//',
    'R-tree': '',
    'TIDY': '',
    'TIDY-ORACLE': '...',
    'TIDY-ADAPTIVE': '///',
    'TIDY-LEARNED': '---',
    'TLX': '++',
    'QUIT-dup': 'xx',
    'BrailTree': 'oo',
    'TIDY-NoMax': '//',
    'TIDY-Full': '',
    'HINT': '//',
    'B-rail': 'oo',
}

METHOD_NAMES = {
    'LIT': 'HINT',
    'R-tree': 'R*-tree',
    'TIDY': 'TIDY',
    'TIDY-ORACLE': 'TIDY-δ',
    'TIDY-ADAPTIVE': 'TIDY-Loc',
    'TIDY-LEARNED': 'TIDY-Lrn',
    'TLX': 'B+Tree',
    'QUIT-dup': 'QuIT',
    'BrailTree': 'B-Rail',
    'TIDY-NoMax': 'No MaxD',
    'TIDY-Full': 'With MaxD',
    'HINT': 'HINT',
    'B-rail': 'B-rail',
}

EXTENT_PRETTY = {
    'snapshot': 'S',
    'dom0p0001': '0.01%',
    'dom0p001': '0.1%',
    'dom0p01': '1%',
    'dom0p1': '10%',
}

def get_extent_pretty_name(extent):
    return EXTENT_PRETTY.get(extent, extent)

FONT_SIZES = {
    'title': 28,
    'suptitle': 32,
    'axis_label': 26,
    'tick': 22,
    'legend': 24,
    'annotation': 22,
    'bar_label': 22,
}

LAYOUT_CONFIG = {
    'subplot_width': 5.5,
    'subplot_height': 5.5,
    'legend_space': 0.15,
    'title_space': 0.15,
    'bar_width_simple': 0.5,
    'bar_width_grouped': 0.14,
    'bar_width_stacked': 0.5,
}

def apply_default_style():
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


def create_subplot_figure(n_subplots, suptitle=None, add_legend=False):
    width = LAYOUT_CONFIG['subplot_width'] * n_subplots
    height = LAYOUT_CONFIG['subplot_height']
    
    fig, axes = plt.subplots(1, n_subplots, figsize=(width, height))
    if n_subplots == 1:
        axes = [axes]
    
    legend_space = LAYOUT_CONFIG['legend_space'] if add_legend else 0
    title_space = LAYOUT_CONFIG['title_space'] if suptitle else 0
    
    if suptitle:
        y_pos = 1.0 - title_space * 0.3
        fig.suptitle(suptitle, fontsize=FONT_SIZES['suptitle'], y=y_pos)
    
    top = 1.0 - title_space - legend_space
    fig.subplots_adjust(top=top, bottom=0.15, left=0.08, right=0.98, wspace=0.25)
    return fig, axes


def plot_simple_bar_chart(ax, categories, values, colors=None, hatches=None, labels=None, ylabel=None, xlabel=None, title=None, log_scale=False, bar_width=None, rotation=0, show_values=False):
    if bar_width is None:
        bar_width = LAYOUT_CONFIG['bar_width_simple']
    x = np.arange(len(categories))
    
    if colors is None:
        colors = ['#808080'] * len(categories)
    if hatches is None:
        hatches = [''] * len(categories)
    if labels is None:
        labels = categories
    
    bars = []
    for i, (val, color, hatch, label) in enumerate(zip(values, colors, hatches, labels)):
        bar = ax.bar(x[i], val, width=bar_width, color=color, hatch=hatch, edgecolor='black', linewidth=0.5, label=label)
        bars.append(bar)
    
    ax.set_xticks(x)
    ax.set_xticklabels(labels, fontsize=FONT_SIZES['bar_label'], rotation=rotation, ha='right' if rotation else 'center')
    ax.set_xlim(-0.5, len(categories) - 0.5)
    ax.grid(axis='y', alpha=0.3, linestyle='--', linewidth=0.5)
    
    if log_scale:
        ax.set_yscale('log')
        from matplotlib.ticker import LogLocator, NullFormatter
        ax.yaxis.set_major_locator(LogLocator(base=10, numticks=15))
        ax.yaxis.set_minor_locator(LogLocator(base=10, subs='auto', numticks=15))
        ax.yaxis.set_minor_formatter(NullFormatter())
    if ylabel:
        ax.set_ylabel(ylabel, fontsize=FONT_SIZES['axis_label'])
    if xlabel:
        ax.set_xlabel(xlabel, fontsize=FONT_SIZES['axis_label'])
    if title:
        ax.set_title(title, fontsize=FONT_SIZES['title'], pad=10)
    if show_values:
        for bar, val in zip(bars, values):
            height = bar.get_height()
            ax.annotate(f'{val:.2g}', xy=(bar.get_x() + bar.get_width() / 2, height), xytext=(0, 3), 
                        textcoords="offset points", ha='center', va='bottom', fontsize=FONT_SIZES['annotation'])
    return bars


def plot_grouped_bar_chart(ax, group_labels, method_order, data, method_config=None, ylabel=None, xlabel=None, title=None, log_scale=False, bar_width=None, show_values=False):
    if bar_width is None:
        bar_width = LAYOUT_CONFIG['bar_width_grouped']
    n_groups = len(group_labels)
    n_methods = len(method_order)
    x = np.arange(n_groups)
    
    bars_dict = {}
    
    for i, method in enumerate(method_order):
        vals = data.get(method, [0] * n_groups)
        offset = (i - n_methods / 2 + 0.5) * bar_width
        
        if method_config:
            color = method_config.get(method, {}).get('color', METHOD_COLORS.get(method, '#808080'))
            label = method_config.get(method, {}).get('name', METHOD_NAMES.get(method, method))
            hatch = method_config.get(method, {}).get('hatch', METHOD_HATCHES.get(method, ''))
        else:
            color = METHOD_COLORS.get(method, '#808080')
            label = METHOD_NAMES.get(method, method)
            hatch = METHOD_HATCHES.get(method, '')
        
        bars = ax.bar(x + offset, vals, bar_width, label=label, color=color, hatch=hatch, edgecolor='black', linewidth=0.5)
        bars_dict[method] = bars
        
        if show_values:
            for bar, val in zip(bars, vals):
                height = bar.get_height()
                if height > 0:
                    ax.annotate(f'{val:.2g}', xy=(bar.get_x() + bar.get_width() / 2, height), xytext=(0, 3),
                                textcoords="offset points", ha='center', va='bottom', fontsize=FONT_SIZES['annotation'] - 2)
    
    ax.set_xticks(x)
    ax.set_xticklabels(group_labels, fontsize=FONT_SIZES['tick'])
    ax.grid(axis='y', alpha=0.3, linestyle='--', linewidth=0.5)
    
    if log_scale:
        ax.set_yscale('log')
        ax.yaxis.set_major_locator(LogLocator(base=10, numticks=15))
        ax.yaxis.set_minor_locator(LogLocator(base=10, subs='auto', numticks=15))
        ax.yaxis.set_minor_formatter(NullFormatter())
    if ylabel:
        ax.set_ylabel(ylabel, fontsize=FONT_SIZES['axis_label'])
    if xlabel:
        ax.set_xlabel(xlabel, fontsize=FONT_SIZES['axis_label'])    
    if title:
        ax.set_title(title, fontsize=FONT_SIZES['title'], pad=10)
    return bars_dict


def add_figure_legend(fig, handles, labels, ncol=None, frameon=False):
    if not handles or not labels:
        return
    if ncol is None or ncol <= 0:
        ncol = len(labels)
    fig.legend(handles, labels, fontsize=FONT_SIZES['legend'],
               loc='upper center', bbox_to_anchor=(0.5, 0.90), ncol=ncol, frameon=frameon)


def add_method_legend_to_figure(fig, method_order, method_config=None, ncol=None):
    handles, labels = create_method_legend_handles(method_order, method_config)
    add_figure_legend(fig, handles, labels, ncol=ncol)


def sync_yaxis_limits(axes):
    if axes is None or len(axes) == 0:
        return
    
    all_ylims = [ax.get_ylim() for ax in axes]
    shared_ymin = min(lim[0] for lim in all_ylims)
    shared_ymax = max(lim[1] for lim in all_ylims)
    
    for ax in axes:
        ax.set_ylim(shared_ymin, shared_ymax)


def create_method_legend_handles(method_order, method_config=None):
    from matplotlib.patches import Patch
    handles = []
    labels = []
    
    for method in method_order:
        if method_config:
            color = method_config.get(method, {}).get('color', METHOD_COLORS.get(method, '#808080'))
            label = method_config.get(method, {}).get('name', METHOD_NAMES.get(method, method))
            hatch = method_config.get(method, {}).get('hatch', METHOD_HATCHES.get(method, ''))
        else:
            color = METHOD_COLORS.get(method, '#808080')
            label = METHOD_NAMES.get(method, method)
            hatch = METHOD_HATCHES.get(method, '')
        
        handle = Patch(facecolor=color, hatch=hatch, edgecolor='black', linewidth=0.5)
        handles.append(handle)
        labels.append(label)
    
    return handles, labels


def ensure_minimum_yticks(fig, min_ticks=2):
    """Ensure all axes in the figure have at least min_ticks y-axis tick marks."""
    from matplotlib.ticker import MaxNLocator, LogLocator
    # Draw the figure first to compute tick positions
    fig.canvas.draw()
    for ax in fig.get_axes():
        yticks = ax.get_yticks()
        ymin, ymax = ax.get_ylim()
        visible_ticks = [t for t in yticks if ymin <= t <= ymax]
        if len(visible_ticks) < min_ticks:
            if ax.get_yscale() == 'log':
                # For log scale, expand the y-axis range to include more decades
                import math
                if ymin > 0 and ymax > 0:
                    log_min = math.floor(math.log10(ymin))
                    log_max = math.ceil(math.log10(ymax))
                    if log_max - log_min < min_ticks - 1:
                        # Expand range to get more ticks
                        log_min = log_max - (min_ticks - 1)
                    ax.set_ylim(10**log_min, 10**log_max)
                ax.yaxis.set_major_locator(LogLocator(base=10, numticks=15))
            else:
                ax.yaxis.set_major_locator(MaxNLocator(nbins='auto', min_n_ticks=min_ticks))
    fig.canvas.draw()


def finalize_and_save(fig, outfile, dpi=150):
    ensure_minimum_yticks(fig, min_ticks=2)
    fig.savefig(outfile, dpi=dpi, bbox_inches='tight', pad_inches=0.1)
    plt.close(fig)
    print(f"Saved: {outfile}")


def get_pretty_name(dataset):
    """Dataset name as shown in plots (log names are already the display names)."""
    return dataset


def find_matching_dataset(results, target_dataset):
    """Return target_dataset if it has results, otherwise None."""
    return target_dataset if target_dataset in results else None


def parse_static_delta_log(logfile):
    """Parse static_delta log file for TIDY-ORACLE and TIDY-ESTIMATED-DELTA entries."""
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
                current_method = None
            elif line.startswith("Extent:") and current_exp:
                current_exp['extent'] = line.split(":", 1)[1].strip()
            elif line.startswith('TIDY-ORACLE') and current_exp:
                current_method = 'TIDY-ORACLE'
                if current_method not in current_exp['methods']:
                    current_exp['methods'][current_method] = {}
            elif line.startswith('TIDY-ESTIMATED-DELTA') and current_exp:
                current_method = 'TIDY-ESTIMATED-DELTA'
                if current_method not in current_exp['methods']:
                    current_exp['methods'][current_method] = {}
            
            if current_exp and current_method:
                if "updating time (dead)" in line:
                    match = re.search(r':\s*([\d.e+-]+)', line)
                    if match:
                        current_exp['methods'][current_method]['dead_update_time'] = float(match.group(1))
                elif "querying time (dead)" in line:
                    match = re.search(r':\s*([\d.e+-]+)', line)
                    if match:
                        current_exp['methods'][current_method]['dead_query_time'] = float(match.group(1))
                        current_method = None
                elif "Total result" in line:
                    match = re.search(r':\s*(\d+)', line)
                    if match:
                        current_exp['methods'][current_method]['total_result'] = int(match.group(1))
    
    if current_exp and current_exp['methods']:
        experiments.append(current_exp)
    
    return experiments


def parse_adaptive_log(logfile):
    """Parse TIDY-ADAPTIVE log file."""
    results = defaultdict(lambda: defaultdict(dict))
    
    with open(logfile, 'r') as f:
        lines = f.readlines()
    
    dataset = None
    extent = None
    outlier_fraction = None
    update_time = None
    query_time = None
    
    for line in lines:
        line = line.strip()
        
        if line.startswith('Dataset:'):
            dataset = line.split(':')[1].strip()
        elif line.startswith('Extent:'):
            extent = line.split(':')[1].strip()
        elif line in ('TIDY-ADAPTIVE', 'TIDY-ADAPTIVE-STAB'):
            outlier_fraction = None
            update_time = None
            query_time = None
        elif 'Outlier fraction' in line:
            match = re.search(r':\s*([\d.]+)', line)
            if match:
                outlier_fraction = float(match.group(1))
        elif 'Total updating time (dead)' in line:
            match = re.search(r':\s*([\d.eE+-]+)', line)
            if match:
                update_time = float(match.group(1))
        elif 'Total querying time (dead)' in line:
            match = re.search(r':\s*([\d.eE+-]+)', line)
            if match:
                query_time = float(match.group(1))
                if dataset and extent and outlier_fraction is not None and update_time is not None:
                    results[dataset][extent][outlier_fraction] = {
                        'update_time': update_time,
                        'query_time': query_time
                    }
    
    return results


def parse_learned_log(logfile):
    """Parse TIDY-LEARNED log file."""
    results = defaultdict(lambda: defaultdict(dict))
    
    with open(logfile, 'r') as f:
        lines = f.readlines()
    
    dataset = None
    extent = None
    update_time = None
    query_time = None
    
    for line in lines:
        line = line.strip()
        
        if line.startswith('Dataset:'):
            dataset = line.split(':')[1].strip()
        elif line.startswith('Extent:'):
            extent = line.split(':')[1].strip()
        elif line.startswith('TIDY-LEARNED'):
            update_time = None
            query_time = None
        elif 'Total updating time (dead)' in line:
            match = re.search(r':\s*([\d.eE+-]+)', line)
            if match:
                update_time = float(match.group(1))
        elif 'Total querying time (dead)' in line:
            match = re.search(r':\s*([\d.eE+-]+)', line)
            if match:
                query_time = float(match.group(1))
                if dataset and extent and update_time is not None:
                    results[dataset][extent] = {
                        'update_time': update_time,
                        'query_time': query_time
                    }
    
    return results


def find_best_adaptive(adaptive_results, dataset, extent):
    """Find the best adaptive variant (lowest update time)."""
    matched_dataset = find_matching_dataset(adaptive_results, dataset)
    if not matched_dataset or extent not in adaptive_results[matched_dataset]:
        return None
    
    best_utime = float('inf')
    best_data = None
    
    for fraction, data in adaptive_results[matched_dataset][extent].items():
        if data['update_time'] < best_utime:
            best_utime = data['update_time']
            best_data = data
    
    return best_data


def find_learned(learned_results, dataset, extent):
    """Find learned data for dataset/extent."""
    matched_dataset = find_matching_dataset(learned_results, dataset)
    if not matched_dataset or extent not in learned_results[matched_dataset]:
        return None
    return learned_results[matched_dataset][extent]


def plot_capped_linear_bars(ax, categories, values, colors, labels, ymax, hatches=None, bar_width=None, rotation=0):
    """Plot bars capped at ymax."""
    if bar_width is None:
        bar_width = LAYOUT_CONFIG['bar_width_simple']
    if hatches is None:
        hatches = [''] * len(categories)
    x = np.arange(len(categories))
    
    for i, (val, color, hatch, label) in enumerate(zip(values, colors, hatches, labels)):
        capped_val = min(val, ymax)
        ax.bar(i, capped_val, width=bar_width, color=color, hatch=hatch, edgecolor='black', linewidth=0.5)
    
    ax.set_xticks(x)
    ax.set_xticklabels(labels, fontsize=FONT_SIZES['bar_label'], rotation=rotation, ha='right' if rotation else 'center')
    ax.grid(axis='y', alpha=0.3, linestyle='--', linewidth=0.5)
    ax.set_ylim(0, ymax)


def export_to_csv(filepath, data, fieldnames):
    """Export data to CSV file."""
    with open(filepath, 'w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(data)
