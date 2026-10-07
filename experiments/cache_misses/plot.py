import sys
import os
import numpy as np
import pandas as pd
from matplotlib.patches import Patch

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
from plot_utils import (
    DATASET_ORDER, LAYOUT_CONFIG, FONT_SIZES, METHOD_COLORS, METHOD_HATCHES, METHOD_NAMES,
    apply_default_style, create_subplot_figure, finalize_and_save, get_pretty_name,
    add_figure_legend,
    filter_datasets,
)

# Methods are the x-axis categories; color + hatch encode the cache level.
INDEX_ORDER = ['R-tree', 'HINT', 'TIDY-LEARNED']
INDEX_CONFIG = {m: {'name': METHOD_NAMES[m], 'color': METHOD_COLORS[m], 'hatch': METHOD_HATCHES[m]}
                for m in INDEX_ORDER}

# Cache levels stacked in each bar, bottom -> top. Color AND hatch identify the
# level (same scheme for every method).
# Per-op columns (L1PerOp etc.) are computed before plotting so bars are
# comparable across datasets regardless of how many total operations each has.
LEVELS = [
    # (csv column, legend label, color, hatch)
    ('L1PerOp', 'L1', '#D9D9D9', '..'),
    ('L2PerOp', 'L2', '#959595', '//'),
    ('L3PerOp', 'L3', '#303030', 'xx'),
]


def ordered_datasets(df):
    names = filter_datasets(df['Pretty'].unique().tolist())
    return sorted(names, key=lambda d: DATASET_ORDER.index(d) if d in DATASET_ORDER else 999)


def make_stacked_plot(df, phase, suptitle, out_name, log_scale):
    sub = df[df['Phase'] == phase].copy()
    if sub.empty:
        print(f"No data for phase '{phase}', skipping {out_name}")
        return
    sub['Pretty'] = sub['Dataset'].map(get_pretty_name)
    datasets = ordered_datasets(sub)
    if not datasets:
        return

    fig, axes = create_subplot_figure(len(datasets), suptitle=suptitle, add_legend=True)
    x = np.arange(len(INDEX_ORDER))
    bw = LAYOUT_CONFIG['bar_width_stacked']

    for di, dataset in enumerate(datasets):
        ax = axes[di]
        d = sub[sub['Pretty'] == dataset]

        bottoms = np.zeros(len(INDEX_ORDER))
        all_totals = np.zeros(len(INDEX_ORDER))
        for col, label, color, hatch in LEVELS:
            vals = np.array([
                float(d[d['Index'] == m][col].iloc[0]) if not d[d['Index'] == m].empty else 0.0
                for m in INDEX_ORDER
            ])
            for i in range(len(INDEX_ORDER)):
                if vals[i] > 0:
                    ax.bar(x[i], vals[i], bw, bottom=max(bottoms[i], 1e-9),
                           color=color, hatch=hatch, edgecolor='black', linewidth=0.5)
            bottoms += vals
            all_totals += vals

        ax.set_xticks(x)
        ax.set_xticklabels([METHOD_NAMES[m] for m in INDEX_ORDER], fontsize=FONT_SIZES['bar_label'])
        ax.set_xlim(-0.5, len(INDEX_ORDER) - 0.5)
        ax.grid(axis='y', alpha=0.3, linestyle='--', linewidth=0.5)
        ax.set_title(dataset, fontsize=FONT_SIZES['title'], pad=10)
        if di == 0:
            ax.set_ylabel('Cache misses / op', fontsize=FONT_SIZES['axis_label'])

        if log_scale:
            ax.set_yscale('log')
            # Anchor the y-axis bottom just below the smallest non-zero bar
            # so every index is always visible regardless of R-tree dominating.
            min_nonzero = min((v for v in all_totals if v > 0), default=1e-3)
            ax.set_ylim(bottom=min_nonzero * 0.3)
        else:
            ax.yaxis.get_major_formatter().set_scientific(True)
            ax.yaxis.get_major_formatter().set_powerlimits((0, 0))

    handles = [Patch(facecolor=c, hatch=h, edgecolor='black', linewidth=0.5)
               for _, _, c, h in LEVELS]
    labels = [lbl for _, lbl, _, _ in LEVELS]
    add_figure_legend(fig, handles, labels, ncol=len(LEVELS))

    finalize_and_save(fig, out_name + '.pdf')


def print_tables(df):
    ops_col = df['Ops'].replace(0, np.nan)
    df = df.copy()
    for lvl in ['L1', 'L2', 'L3']:
        df[lvl + 'PerOp'] = (df[lvl + 'Miss'] / ops_col).fillna(0.0)
    df['TimePerOp'] = (df['TimeSec'] / ops_col).fillna(0.0)
    df['InstrPerOp'] = (df['Instructions'] / ops_col).fillna(0.0)
    df['IPC'] = (df['Instructions'] / df['Cycles'].replace(0, np.nan)).fillna(0.0)
    df['Pretty'] = df['Dataset'].map(get_pretty_name)

    lines = []
    for dataset in ordered_datasets(df):
        lines.append(f"\n=== {dataset} ===")
        for phase in ['insert', 'query']:
            lines.append(f"  [{phase}]   (per op, except IPC)")
            lines.append(f"    {'index':<14}{'time(s)':>12}{'L1/op':>10}{'L2/op':>10}"
                         f"{'L3/op':>10}{'instr/op':>12}{'IPC':>7}")
            for m in INDEX_ORDER:
                row = df[(df['Pretty'] == dataset) & (df['Index'] == m) & (df['Phase'] == phase)]
                if row.empty:
                    continue
                r = row.iloc[0]
                lines.append(f"    {m:<14}{r['TimePerOp']:>12.3e}"
                             f"{r['L1PerOp']:>10.2f}{r['L2PerOp']:>10.2f}{r['L3PerOp']:>10.2f}"
                             f"{r['InstrPerOp']:>12.1f}{r['IPC']:>7.2f}")
    text = "\n".join(lines)
    print(text)
    with open('summary_tables.txt', 'w') as f:
        f.write(text + "\n")
    print("\nSaved: summary_tables.txt")


def plot_results(csv_path):
    apply_default_style()
    df = pd.read_csv(csv_path)
    if df.empty:
        print("results.csv is empty, nothing to plot")
        return

    # If the script ran more than once, the CSV accumulates duplicate rows.
    # Keep only the last occurrence of each (Dataset, Extent, Index, Phase) so
    # the freshest numbers win.
    df = df.drop_duplicates(subset=['Dataset', 'Extent', 'Index', 'Phase'], keep='last').reset_index(drop=True)

    # Precompute per-op cache-miss columns so the stacked plots use comparable scales.
    ops = df['Ops'].replace(0, np.nan)
    df['L1PerOp'] = (df['L1Miss'] / ops).fillna(0.0)
    df['L2PerOp'] = (df['L2Miss'] / ops).fillna(0.0)
    df['L3PerOp'] = (df['L3Miss'] / ops).fillna(0.0)

    # ---- cache misses (stacked L1/L2/L3) — Figure 10 ----
    make_stacked_plot(df, 'insert', 'Cache misses per insert (L1 / L2 / L3)',
                      'cache_misses_insert', log_scale=True)
    make_stacked_plot(df, 'query',  'Cache misses per query (L1 / L2 / L3)',
                      'cache_misses_query', log_scale=False)

    print_tables(df)


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python plot.py <results.csv>")
        sys.exit(1)
    plot_results(sys.argv[1])
