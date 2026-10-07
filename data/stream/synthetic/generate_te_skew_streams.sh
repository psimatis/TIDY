#!/usr/bin/env bash
# Generate the one-billion-interval te_skew streams for every hotspot setting.
#
# Usage: ./generate_te_skew_streams.sh [1pt_uniform|1pt_not_uniform|all]
#
# Streams that were already generated completely with the same parameters are
# skipped, so this script is safe to re-run; interrupted or stale ones are
# regenerated. Set FORCE=1 to regenerate everything.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/stream_check.sh"
SOURCE="$SCRIPT_DIR/streamGenerator_te_skew_large.cpp"
GENERATOR="/tmp/tidy_streamGenerator_te_skew_large_${UID}"

RECORDS="${RECORDS:-1000000000}"
DOMAIN="${DOMAIN:-10000000}"
QUERIES="${QUERIES:-1000}"
SEED="${SEED:-42}"
ZIPF_EXPONENT="${ZIPF_EXPONENT:-1.2}"
MAX_DURATION_FRACTION="${MAX_DURATION_FRACTION:-0.01}"
EXTENT_FRACTION="${EXTENT_FRACTION:-0.001}"
EXTENT_TOKEN="${EXTENT_TOKEN:-dom0p001}"
FORCE="${FORCE:-0}"
# Share of end times inside the hotspot; the paper reports 0.99.
CLUSTER_FRACTIONS="${CLUSTER_FRACTIONS:-0.99}"

SET="${1:-all}"
case "$SET" in
    1pt_uniform|1pt_not_uniform|all) ;;
    *)
        echo "Unknown set: $SET" >&2
        echo "Expected 1pt_uniform, 1pt_not_uniform, or all" >&2
        exit 1
        ;;
esac

trap 'rm -f "$GENERATOR"' EXIT
g++ -O3 -std=c++17 -Wall -Wextra -pedantic "$SOURCE" -o "$GENERATOR"

generate_set() {
    local output_dir="$1"
    local hotspot_width="$2"
    local query_mode="$3"
    shift 3
    local cluster_fractions=("$@")

    mkdir -p "$output_dir"
    for cluster_fraction in "${cluster_fractions[@]}"; do
        local fraction_token="${cluster_fraction/./p}"
        local dataset="te_skew_${fraction_token}"
        local stream="$output_dir/$dataset/${dataset}_stream_${EXTENT_TOKEN}.stream"
        if [[ "$FORCE" != "1" ]] && stream_ok "$stream" \
            records="$RECORDS" configured_domain="$DOMAIN" queries="$QUERIES" \
            zipf_exponent="$ZIPF_EXPONENT" max_duration_fraction="$MAX_DURATION_FRACTION" \
            hotspot_width_fraction="$hotspot_width" cluster_fraction="$cluster_fraction" \
            query_mode="$query_mode"; then
            echo "Skipping existing stream: $stream"
            continue
        fi
        [[ -e "$stream" ]] && echo "Existing stream is incomplete or stale; regenerating: $stream"
        echo "Generating $stream (hotspot=$hotspot_width, queries=$query_mode)"
        "$GENERATOR" \
            "$output_dir" \
            "$RECORDS" \
            "$DOMAIN" \
            "$QUERIES" \
            "$SEED" \
            "$ZIPF_EXPONENT" \
            "$MAX_DURATION_FRACTION" \
            "$hotspot_width" \
            "$cluster_fraction" \
            "$EXTENT_FRACTION" \
            "$query_mode"
    done
}

if [[ "$SET" == "1pt_uniform" || "$SET" == "all" ]]; then
    # Hotspot covering 1% of the domain, queries uniform over the ingested
    # domain (Table 10, "uniform").
    generate_set "$SCRIPT_DIR/te_skew/1b_w0p01" 0.01 uniform $CLUSTER_FRACTIONS
fi

if [[ "$SET" == "1pt_not_uniform" || "$SET" == "all" ]]; then
    # Same 1% hotspot with adversarial query placement: every q.t_e sits right
    # before the hotspot (Table 10, "before hotspot").
    generate_set "$SCRIPT_DIR/te_skew/1b_w0p01_qhot" 0.01 hotspot $CLUSTER_FRACTIONS
fi

echo "Stream generation complete for set: $SET"
