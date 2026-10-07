#!/bin/bash
cd "$(dirname "$0")/../.."

CC=g++
$CC -O3 -mavx -std=c++17 -w -DWORKLOAD_COUNT -I. \
    indexes/containers/relation.o indexes/containers/buffer.o \
    indexes/LIT/hierarchicalindex.o indexes/LIT/live_index.o \
    indexes/LIT/hint_m_dynamic.o \
    experiments/live_vs_dead/main.cpp -o live_vs_dead.exe

LOGFILE="experiments/live_vs_dead/results.log"
> "$LOGFILE"

EXTENTS=(
    "dom0p01"
)

DATASETS=(
    "TAXIS"
    "BIKES"
    "FLIGHTS"
    "WEBKIT"
    "WILDFIRES"
)

for dataset in "${DATASETS[@]}"; do
    for ext in "${EXTENTS[@]}"; do
        stream="data/stream/domain_extent_uniform/${dataset}/${dataset}_stream_${ext}.stream"

        if [ ! -f "$stream" ]; then
            echo "Stream not found: $stream" | tee -a "$LOGFILE"
            continue
        fi

        echo "========================================" | tee -a "$LOGFILE"
        echo "Dataset: $dataset" | tee -a "$LOGFILE"
        echo "Extent: $ext" | tee -a "$LOGFILE"
        echo "========================================" | tee -a "$LOGFILE"

        ./live_vs_dead.exe "$stream" 2>&1 | tee -a "$LOGFILE"

        echo "" | tee -a "$LOGFILE"
    done
done

echo "All experiments completed!" | tee -a "$LOGFILE"

cd experiments/live_vs_dead
python3 plot.py results.log
