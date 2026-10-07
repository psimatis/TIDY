#!/usr/bin/env python3
"""Table 10: ingestion and query cost under end-time skew.

Writes te_skew_table.csv, laid out as in the paper.
"""
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))

import pandas as pd

from synthetic_lib import METHOD_ORDER, load_frame

if __name__ == "__main__":
    frame = load_frame(HERE / "logs", {"Placement", "Dataset", "Extent"}, "Placement")

    def metric(placement, column):
        rows = frame[frame["Placement"] == placement].set_index("Method")[column]
        return [rows.get(method) for method in METHOD_ORDER]

    # Both placements ingest the same intervals; the paper reports the uniform run.
    table = pd.DataFrame(
        [
            metric("uniform", "UpdateTime"),
            metric("uniform", "QueryTime"),
            metric("before_hotspot", "QueryTime"),
        ],
        index=[
            "Total ingestion cost (s)",
            "Total query cost (s), uniform",
            "Total query cost (s), before hotspot",
        ],
        columns=METHOD_ORDER,
    )
    table.to_csv(HERE / "te_skew_table.csv", float_format="%.1f")
    print(table.to_string(float_format="%.1f"))
    print(f"Saved: {HERE / 'te_skew_table.csv'}")
