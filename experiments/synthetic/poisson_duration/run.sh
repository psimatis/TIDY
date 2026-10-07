#!/usr/bin/env bash
# Figure 11: one billion intervals with Poisson durations, lambda in {1e2..1e5}.
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

DATA_DIR="$ROOT/data/stream/synthetic/poisson_duration/1b"
LAMBDAS=(100 1000 10000 100000)
DOMAIN=10000000
EXTENT="dom0p001"
LOG_DIR="$SCRIPT_DIR/logs"

build_methods "${METHODS[@]}"
mkdir -p "$LOG_DIR"

for method in "${METHODS[@]}"; do
    logfile="$LOG_DIR/$method.log"
    > "$logfile"
    for lambda_value in "${LAMBDAS[@]}"; do
        dataset="poisson_lambda_${lambda_value}"
        stream="$DATA_DIR/$dataset/${dataset}_stream_${EXTENT}.stream"
        if [[ ! -f "$stream" ]]; then
            echo "Stream not found: $stream" >&2
            exit 1
        fi
        {
            echo "========================================"
            echo "Dataset: $dataset"
            echo "Extent: $EXTENT"
            echo "Lambda: $lambda_value"
            echo "Domain: $DOMAIN"
            echo "========================================"
        } | tee -a "$logfile"
        run_method "$method" "$stream" "$logfile"
    done
done

cd "$SCRIPT_DIR"
python3 verify.py
python3 plot.py
