#!/usr/bin/env python3
"""Shared log parsing and verification for the synthetic experiments.

Every method writes its own log under the experiment's ``logs/`` directory;
each log starts its blocks with ``Key: value`` header lines (Dataset, Lambda,
Placement, ...) followed by the method's report.
"""
import re
from pathlib import Path

import pandas as pd

METHOD_ORDER = ["R-tree", "HINT", "B-rail", "TIDY"]

# Banner each binary prints before its report.
METHOD_BANNERS = {
    "R-tree": "R-tree",
    "LIT": "HINT",
    "BRAIL": "B-rail",
    "TIDY-LEARNED": "TIDY",
}

NUMERIC_CONTEXT = {"Lambda", "Domain", "Cluster fraction", "Records"}


def parse_log(path, context_keys):
    rows = []
    context = {key: None for key in context_keys}
    current = None

    def flush():
        nonlocal current
        if current and "QueryTime" in current:
            rows.append(current)
        current = None

    with open(path) as stream:
        for raw_line in stream:
            line = raw_line.strip()
            key, _, value = line.partition(":")
            if key in context_keys:
                flush()
                value = value.strip()
                context[key] = float(value) if key in NUMERIC_CONTEXT else value
            elif line in METHOD_BANNERS:
                flush()
                current = dict(context)
                current["Method"] = METHOD_BANNERS[line]
            elif current is not None:
                match = re.search(r":\s*([\d.eE+-]+)", line)
                if not match:
                    continue
                number = float(match.group(1))
                if "Total updating time (dead)" in line:
                    current["UpdateTime"] = number
                elif "Total querying time (dead)" in line:
                    current["QueryTime"] = number
                elif "Total result [COUNT]" in line:
                    current["TotalResult"] = int(number)
                elif "index size" in line and "[MB]" in line and "Live" not in line:
                    current["MemoryMB"] = number
                elif line.startswith("Final Delta"):
                    current["Delta"] = number
                elif line.startswith("Short intervals"):
                    current["ShortCount"] = int(number)
                elif line.startswith("Long intervals"):
                    current["LongCount"] = int(number)
    flush()
    return rows


def load_frame(log_dir, context_keys, group_key):
    """All completed runs under log_dir; one row per (group_key, Method)."""
    logs = sorted(Path(log_dir).rglob("*.log"))
    if not logs:
        raise SystemExit(f"No logs found in {log_dir}")
    rows = [row for path in logs for row in parse_log(path, context_keys)]
    if not rows:
        raise SystemExit("No completed method runs found")
    frame = pd.DataFrame(rows)
    duplicate = frame.duplicated([group_key, "Method"], keep=False)
    if duplicate.any():
        pairs = frame.loc[duplicate, [group_key, "Method"]].drop_duplicates()
        raise SystemExit("Duplicate runs across logs:\n" + pairs.to_string(index=False))
    frame["Method"] = pd.Categorical(frame["Method"], METHOD_ORDER, ordered=True)
    return frame.sort_values([group_key, "Method"]).reset_index(drop=True)


def verify(frame, group_key):
    """Every method must return the same total result count per setting.

    Methods that have not been run are reported, not counted as failures.
    """
    failures = []
    for setting, group in frame.groupby(group_key, observed=True):
        counts = dict(zip(group["Method"].astype(str), group["TotalResult"]))
        if len(set(counts.values())) > 1:
            failures.append(f"{group_key}={setting}: result counts differ: {counts}")
        for method in METHOD_ORDER:
            if method not in counts:
                print(f"  PENDING {group_key}={setting}: {method} has not been run")
    if failures:
        print("VERIFICATION FAILED")
        for failure in failures:
            print(f"  {failure}")
        raise SystemExit(1)
    print(f"VERIFICATION PASSED ({len(frame)} method runs)")
