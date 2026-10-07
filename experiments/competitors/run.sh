#!/bin/bash
cd "$(dirname "$0")/../.."

make competitors_all

LOGFILE="experiments/competitors/results.log"
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

        echo "--- lit.exe ---" | tee -a "$LOGFILE"
        ./lit.exe $STAB_FLAG "$stream" 2>&1 | tee -a "$LOGFILE"

        echo "--- rtree.exe ---" | tee -a "$LOGFILE"
        ./rtree.exe $STAB_FLAG "$stream" 2>&1 | tee -a "$LOGFILE"

        echo "" | tee -a "$LOGFILE"
    done
done

echo "All experiments completed!" | tee -a "$LOGFILE"

cd experiments/competitors
python3 verify.py results.log results_learned.log ../brail_variants/results.log
python3 plot.py results.log results_learned.log ../brail_variants/results.log
