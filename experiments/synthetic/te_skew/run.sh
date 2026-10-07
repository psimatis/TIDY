#!/usr/bin/env bash
# Table 10: one billion intervals, 99% of end times in a hotspot covering 1% of
# the domain, with "uniform" and "before hotspot" query placement.
#
# Comment out entries of METHODS to skip them; each method writes its own log
# under logs/, so the results of skipped methods are kept.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/../common.sh"

METHODS=(
    rtree
    hint
    brail
    tidy
)

# Query placement -> stream directory (see data/stream/synthetic/generate_te_skew_streams.sh).
declare -A PLACEMENT_DATA=(
    [uniform]="$ROOT/data/stream/synthetic/te_skew/1b_w0p01"
    [before_hotspot]="$ROOT/data/stream/synthetic/te_skew/1b_w0p01_qhot"
)
DATASET="te_skew_0p99"
EXTENT="dom0p001"
LOG_DIR="$SCRIPT_DIR/logs"

build_methods "${METHODS[@]}"
mkdir -p "$LOG_DIR"

for placement in uniform before_hotspot; do
    stream="${PLACEMENT_DATA[$placement]}/$DATASET/${DATASET}_stream_${EXTENT}.stream"
    if [[ ! -f "$stream" ]]; then
        echo "Stream not found: $stream" >&2
        exit 1
    fi
    for method in "${METHODS[@]}"; do
        logfile="$LOG_DIR/${placement}_${method}.log"
        {
            echo "========================================"
            echo "Placement: $placement"
            echo "Dataset: $DATASET"
            echo "Extent: $EXTENT"
            echo "========================================"
        } | tee "$logfile"
        run_method "$method" "$stream" "$logfile"
    done
done

cd "$SCRIPT_DIR"
python3 verify.py
python3 table.py
