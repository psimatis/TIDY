#!/bin/bash
cd "$(dirname "$0")/../.."

make learned_all

LOGFILE="experiments/competitors/results_learned.log"
> "$LOGFILE"

EXTENTS=(
    "snapshot"
    "dom0p0001"
    "dom0p001"
    "dom0p01"
)

DATASETS=(
    "WEBKIT"
    "WILDFIRES"
    "BIKES"
    "FLIGHTS"
    "TAXIS"
)

LR="100"
WARMUP="10000"
FRACTION="0.1"

for dataset in "${DATASETS[@]}"; do
    for ext in "${EXTENTS[@]}"; do
        if [ "$ext" = "snapshot" ]; then
            stream="data/stream/domain_extent_uniform/${dataset}/${dataset}_stream_dom0p0001.stream"
        else
            stream="data/stream/domain_extent_uniform/${dataset}/${dataset}_stream_${ext}.stream"
        fi

        if [ ! -f "$stream" ]; then
            echo "Stream not found: $stream" | tee -a "$LOGFILE"
            continue
        fi

        echo "========================================" | tee -a "$LOGFILE"
        echo "Dataset: $dataset" | tee -a "$LOGFILE"
        echo "Extent: $ext" | tee -a "$LOGFILE"
        echo "========================================" | tee -a "$LOGFILE"

        if [ "$ext" = "snapshot" ]; then
            STAB_FLAG="-s"
        else
            STAB_FLAG=""
        fi

        echo "--- learned.exe (M=$WARMUP, lr=$LR, rho=$FRACTION) ---" | tee -a "$LOGFILE"
        ./learned.exe $STAB_FLAG -f "$FRACTION" -l "$LR" -m "$WARMUP" "$stream" 2>&1 | tee -a "$LOGFILE"

        echo "" | tee -a "$LOGFILE"
    done
done

echo "All experiments completed!" | tee -a "$LOGFILE"
