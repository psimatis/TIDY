import sys
import os
import numpy as np
from collections import defaultdict

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from plot_utils import (
    get_extent_pretty_name, DATASET_ORDER,
    apply_default_style, create_subplot_figure, plot_grouped_bar_chart,
    finalize_and_save, add_method_legend_to_figure,
    METHOD_NAMES, METHOD_COLORS, METHOD_HATCHES, FONT_SIZES,
    get_pretty_name,
    parse_static_delta_log, parse_adaptive_log, parse_learned_log,
    find_best_adaptive, find_learned,
    plot_capped_linear_bars,
    filter_datasets
)

def get_extent_display_name(extent):
    if extent == 'snapshot':
        return 'S'
    return get_extent_pretty_name(extent)

METHOD_CONFIG = {
    'TIDY-ORACLE': {'name': METHOD_NAMES['TIDY-ORACLE'], 'color': METHOD_COLORS['TIDY-ORACLE'], 'hatch': METHOD_HATCHES['TIDY-ORACLE']},
    'TIDY-ADAPTIVE': {'name': METHOD_NAMES['TIDY-ADAPTIVE'], 'color': METHOD_COLORS['TIDY-ADAPTIVE'], 'hatch': METHOD_HATCHES['TIDY-ADAPTIVE']},
    'TIDY-LEARNED': {'name': METHOD_NAMES['TIDY-LEARNED'], 'color': METHOD_COLORS['TIDY-LEARNED'], 'hatch': METHOD_HATCHES['TIDY-LEARNED']},
}
METHOD_ORDER = ['TIDY-ORACLE', 'TIDY-ADAPTIVE', 'TIDY-LEARNED']


def plot_results(oracle_experiments, adaptive_results, learned_results, outdir):
    
    by_dataset = defaultdict(list)
    for exp in oracle_experiments:
        pretty = get_pretty_name(exp['dataset'])
        by_dataset[pretty].append(exp)

    apply_default_style()

    datasets = sorted(filter_datasets(by_dataset.keys()), key=lambda d: DATASET_ORDER.index(d) if d in DATASET_ORDER else 999)
    n_datasets = len(datasets)

    # Update time plot (linear scale, capped) — Figure 8(a)
    fig, axes = create_subplot_figure(n_datasets, suptitle='Dead Index Update Time Comparison (Linear Scale)', add_legend=True)
    
    for idx, dataset_name in enumerate(datasets):
        exps = by_dataset[dataset_name]
        ax = axes[idx]
        vals = []
        
        oracle_dataset = exps[0]['dataset']
        extent = 'dom0p0001'
        exp_for_extent = [e for e in exps if e.get('extent') == extent]
        if not exp_for_extent:
            exp_for_extent = [exps[0]]
        
        for method in METHOD_ORDER:
            if method == 'TIDY-ORACLE':
                method_vals = [e['methods'].get('TIDY-ORACLE', {}).get('dead_update_time', 0) for e in exp_for_extent]
                avg_val = np.mean(method_vals) if method_vals else 0
            elif method == 'TIDY-ADAPTIVE':
                best_adaptive = find_best_adaptive(adaptive_results, oracle_dataset, extent)
                avg_val = best_adaptive['update_time'] if best_adaptive else 0
            elif method == 'TIDY-LEARNED':
                learned_data = find_learned(learned_results, oracle_dataset, extent)
                avg_val = learned_data['update_time'] if learned_data else 0
            else:
                avg_val = 0
            
            vals.append(avg_val)
        
        # Set y-max based on second largest value
        vals_sorted = sorted(vals)
        if len(vals_sorted) >= 2:
            ymax = vals_sorted[-2] * 1.2
        else:
            ymax = max(vals) * 1.1
        
        colors = [METHOD_CONFIG[m]['color'] for m in METHOD_ORDER]
        hatches = [METHOD_CONFIG[m]['hatch'] for m in METHOD_ORDER]
        labels = [METHOD_CONFIG[m]['name'] for m in METHOD_ORDER]
        
        plot_capped_linear_bars(ax, METHOD_ORDER, vals, colors, labels, ymax, hatches=hatches)
        
        if idx == 0:
            ax.set_ylabel('Update Time (s)', fontsize=FONT_SIZES['axis_label'])
        ax.set_title(dataset_name, fontsize=FONT_SIZES['title'], pad=10)
    add_method_legend_to_figure(fig, METHOD_ORDER, METHOD_CONFIG)
    finalize_and_save(fig, os.path.join(outdir, 'tidy_variants_update_time_linear.pdf'))

    # Query time plot — Figure 8(b)
    fig, axes = create_subplot_figure(n_datasets, suptitle='Dead Index Query Time Comparison', add_legend=True)
    
    for idx, dataset_name in enumerate(datasets):
        exps = by_dataset[dataset_name]
        ax = axes[idx]
        exps = sorted(exps, key=lambda x: (0 if x.get('extent') == 'snapshot' else 1, x.get('extent') or ''))
        extents = [get_extent_display_name(e.get('extent')) for e in exps]

        data = {}
        for method in METHOD_ORDER:
            data[method] = []
            for exp in exps:
                oracle_dataset = exp['dataset']
                extent = exp['extent']
                
                if method == 'TIDY-ORACLE':
                    val = exp['methods'].get('TIDY-ORACLE', {}).get('dead_query_time', 0)
                elif method == 'TIDY-ADAPTIVE':
                    best_adaptive = find_best_adaptive(adaptive_results, oracle_dataset, extent)
                    val = best_adaptive['query_time'] if best_adaptive else 0
                elif method == 'TIDY-LEARNED':
                    learned_data = find_learned(learned_results, oracle_dataset, extent)
                    val = learned_data['query_time'] if learned_data else 0
                else:
                    val = 0
                
                data[method].append(val)
        
        plot_grouped_bar_chart(ax, extents, METHOD_ORDER, data, method_config=METHOD_CONFIG,
                               title=dataset_name, xlabel='Domain Extent', log_scale=True,
                               ylabel='Query Time (s)' if idx == 0 else None)

    add_method_legend_to_figure(fig, METHOD_ORDER, METHOD_CONFIG)

    finalize_and_save(fig, os.path.join(outdir, 'tidy_variants_query_time.pdf'))

if __name__ == "__main__":
    if len(sys.argv) != 4:
        print("Usage: plot.py <static_delta_logfile> <adaptive_logfile> <learned_logfile>")
        sys.exit(1)

    static_delta_logfile = sys.argv[1]
    adaptive_logfile = sys.argv[2]
    learned_logfile = sys.argv[3]
    outdir = os.path.dirname(os.path.abspath(__file__))

    static_delta_experiments = parse_static_delta_log(static_delta_logfile)
    adaptive_results = parse_adaptive_log(adaptive_logfile)
    learned_results = parse_learned_log(learned_logfile)

    if static_delta_experiments:
        plot_results(static_delta_experiments, adaptive_results, learned_results, outdir)
    else:
        print("No experiments found in static_delta log file")
