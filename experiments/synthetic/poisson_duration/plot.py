#!/usr/bin/env python3
"""Figure 11: ingestion and query time vs. Poisson mean lambda.

Writes poisson_update_time.pdf, poisson_query_time.pdf and poisson_results.csv
(every parsed metric, incl. memory and TIDY's calibrated delta).
"""
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parents[1]))
sys.path.insert(0, str(HERE.parent))

from plot_utils import (
    METHOD_COLORS,
    METHOD_HATCHES,
    METHOD_NAMES,
    add_method_legend_to_figure,
    apply_default_style,
    create_subplot_figure,
    finalize_and_save,
    plot_grouped_bar_chart,
)
from synthetic_lib import METHOD_ORDER, load_frame

CONTEXT = {"Dataset", "Extent", "Lambda", "Domain"}
METHOD_CONFIG = {
    method: {
        "name": METHOD_NAMES[method],
        "color": METHOD_COLORS[method],
        "hatch": METHOD_HATCHES[method],
    }
    for method in METHOD_ORDER
}


def metric_plot(frame, lambdas, domain, metric, ylabel, filename):
    data = {}
    for method in METHOD_ORDER:
        rows = frame[frame["Method"] == method].set_index("Lambda")[metric]
        data[method] = [float(rows.get(value, 0)) for value in lambdas]

    fig, axes = create_subplot_figure(
        1, suptitle=f"Poisson Duration Sensitivity: {ylabel}", add_legend=True
    )
    plot_grouped_bar_chart(
        axes[0],
        [f"{100 * value / domain:g}" for value in lambdas],
        METHOD_ORDER,
        data,
        method_config=METHOD_CONFIG,
        ylabel=ylabel,
        xlabel=r"Mean duration $\lambda$ (% of domain)",
        log_scale=True,
        bar_width=0.16,
    )
    add_method_legend_to_figure(fig, METHOD_ORDER, METHOD_CONFIG)
    finalize_and_save(fig, str(HERE / f"{filename}.pdf"))


if __name__ == "__main__":
    apply_default_style()
    frame = load_frame(HERE / "logs", CONTEXT, "Lambda")
    frame.to_csv(HERE / "poisson_results.csv", index=False)
    domains = frame["Domain"].dropna().unique()
    if len(domains) != 1:
        raise SystemExit(f"Expected one configured domain, found: {domains}")
    lambdas = sorted(frame["Lambda"].dropna().unique())
    metric_plot(frame, lambdas, domains[0], "UpdateTime", "Ingestion Time (s)", "poisson_update_time")
    metric_plot(frame, lambdas, domains[0], "QueryTime", "Query Time (s)", "poisson_query_time")
    print(f"Saved: {HERE / 'poisson_results.csv'}")
