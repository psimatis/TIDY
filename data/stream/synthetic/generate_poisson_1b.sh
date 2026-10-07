#!/usr/bin/env bash
set -euo pipefail
# Generate the one-billion-interval Poisson-duration streams (Figure 11).
# Streams that were already generated completely with the same parameters are
# skipped; interrupted or stale ones are regenerated. Set FORCE=1 to regenerate
# everything.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/stream_check.sh"
SOURCE="$SCRIPT_DIR/streamGenerator_poisson_large.cpp"
GENERATOR="/tmp/tidy_streamGenerator_poisson_large_${UID}"
OUTPUT_DIR="$SCRIPT_DIR/poisson_duration/1b"

RECORDS="${RECORDS:-1000000000}"
DOMAIN="${DOMAIN:-10000000}"
QUERIES=1000
SEED=42
EXTENT_FRACTION=0.001
LAMBDAS=(100 1000 10000 100000)

trap 'rm -f "$GENERATOR"' EXIT

g++ -O3 -std=c++17 -Wall -Wextra -pedantic \
    "$SOURCE" -o "$GENERATOR"

for lambda_value in "${LAMBDAS[@]}"; do
    dataset="poisson_lambda_${lambda_value}"
    stream="$OUTPUT_DIR/$dataset/${dataset}_stream_dom0p001.stream"
    if [[ "${FORCE:-0}" != "1" ]] && stream_ok "$stream" \
        records="$RECORDS" configured_domain="$DOMAIN" queries="$QUERIES" lambda="$lambda_value"; then
        echo "Skipping existing stream: $dataset"
        continue
    fi
    [[ -e "$stream" ]] && echo "Existing stream is incomplete or stale; regenerating: $stream"
    "$GENERATOR" \
        "$OUTPUT_DIR" \
        "$RECORDS" \
        "$DOMAIN" \
        "$QUERIES" \
        "$SEED" \
        "$lambda_value" \
        "$EXTENT_FRACTION"
done

echo "Poisson streams are in $OUTPUT_DIR"
