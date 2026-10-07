import sys
from pathlib import Path
import numpy as np
import pandas as pd

INPUT_DIR = "../static"
OUTPUT_BASE_DIR = "domain_extent_uniform"
DATASETS = ["TAXIS", "BIKES", "FLIGHTS", "WEBKIT", "WILDFIRES"]

DOMAIN_PERCENTAGES = [0.0001, 0.001, 0.01]
QUERIES_PER_BUCKET = 1000
RANDOM_SEED = 42


def stream_filename(dataset_name, pct):
    pct_str = f"{pct:.4f}".replace(".", "p").rstrip("0")
    return f"{dataset_name}_stream_dom{pct_str}.stream"


def format_events(is_end, ids, ts):
    frame = pd.DataFrame({"op": np.where(is_end, "E", "S"), "id": ids, "ts": ts, "a": 0, "b": 0})
    return frame.to_csv(sep=" ", header=False, index=False, lineterminator="\n")


def generate_stream(input_filepath, output_dir):
    dataset_name = Path(input_filepath).stem
    out_dir = Path(output_dir) / dataset_name
    if all((out_dir / stream_filename(dataset_name, pct)).exists() for pct in DOMAIN_PERCENTAGES):
        print(f"Streams for {dataset_name} already exist, skipping")
        return

    print(f"Loading: {input_filepath}")

    data = pd.read_csv(input_filepath, sep=r"\s+", header=None, usecols=[0, 1], dtype=np.int64)
    start = data[0].to_numpy()
    end = data[1].to_numpy()
    del data

    n = len(start)
    if n == 0:
        return

    min_time = int(start.min())
    max_time = int(end.max())
    domain_extent = max_time - min_time

    # Event 2i is the start of interval i and event 2i+1 its end; a stable sort
    # keeps file order among equal timestamps, with a start before its own end.
    ts = np.empty(2 * n, dtype=np.int64)
    ts[0::2] = start - min_time
    ts[1::2] = end - min_time
    del start, end

    order = np.argsort(ts, kind="stable")
    ts = ts[order]
    is_end = (order & 1).astype(bool)
    record = order >> 1
    del order

    # Intervals are renumbered in the order their start events appear.
    is_start = ~is_end
    new_id = np.empty(n, dtype=np.int64)
    new_id[record[is_start]] = np.arange(n, dtype=np.int64)
    ids = new_id[record]
    del record, new_id, is_start

    out_dir.mkdir(parents=True, exist_ok=True)

    rng = np.random.default_rng(RANDOM_SEED)

    extents = {pct: int(domain_extent * pct) for pct in DOMAIN_PERCENTAGES}
    max_extent = max(extents.values())

    files = {pct: open(out_dir / stream_filename(dataset_name, pct), "w") for pct in DOMAIN_PERCENTAGES}

    def write_events(lo, hi):
        if lo < hi:
            text = format_events(is_end[lo:hi], ids[lo:hi], ts[lo:hi])
            for f in files.values():
                f.write(text)

    def write_query(max_seen_time):
        qend = int(rng.integers(max_extent, max_seen_time + 1))
        for pct in DOMAIN_PERCENTAGES:
            files[pct].write(f"Q {qend - extents[pct]} {qend} 0 0\n")

    num_events = len(ts)
    interval = max(1, num_events // QUERIES_PER_BUCKET)
    query_count = 0
    written = 0

    # A query is placed after every interval-th event; ts is sorted, so the
    # largest timestamp seen so far is the current event's.
    for idx in range(interval, num_events, interval):
        if query_count >= QUERIES_PER_BUCKET:
            break
        write_events(written, idx + 1)
        written = idx + 1
        max_seen_time = max(0, int(ts[idx]))
        if max_seen_time >= max_extent:
            write_query(max_seen_time)
            query_count += 1
    write_events(written, num_events)

    max_seen_time = max(0, int(ts[-1]))
    while query_count < QUERIES_PER_BUCKET and max_seen_time >= max_extent:
        write_query(max_seen_time)
        query_count += 1

    for f in files.values():
        f.close()

    print(f"  Done: {n} records, domain={domain_extent}, queries={query_count}")


if __name__ == "__main__":
    script_dir = Path(__file__).parent
    for dataset in sys.argv[1:] or DATASETS:
        path = script_dir / INPUT_DIR / f"{dataset}.dat"
        if path.exists():
            generate_stream(str(path), str(script_dir / OUTPUT_BASE_DIR))
