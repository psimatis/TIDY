#!/bin/bash
cd "$(dirname "$0")/../.."

# Build object files
make indexes/containers/relation.o indexes/containers/buffer.o indexes/LIT/hierarchicalindex.o indexes/LIT/live_index.o indexes/LIT/hint_m_dynamic.o

# Compile the experiment binary
g++ -O3 -mavx -std=c++17 -w -I. \
    indexes/containers/relation.o \
    indexes/containers/buffer.o \
    indexes/LIT/hierarchicalindex.o \
    indexes/LIT/live_index.o \
    indexes/LIT/hint_m_dynamic.o \
    experiments/brail_variants/main.cpp \
    -o experiments/brail_variants/brail_variants.exe

LOGFILE="experiments/brail_variants/results.log"
> "$LOGFILE"

EXTENTS=(
    "snapshot"
    "dom0p0001"
    "dom0p001"
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

        ./experiments/brail_variants/brail_variants.exe -v nomax $STAB_FLAG "$stream" 2>&1 | tee -a "$LOGFILE"
        ./experiments/brail_variants/brail_variants.exe -v full $STAB_FLAG "$stream" 2>&1 | tee -a "$LOGFILE"

        echo "" | tee -a "$LOGFILE"
    done
done

echo "All experiments completed!" | tee -a "$LOGFILE"

cd experiments/brail_variants
python3 verify.py results.log
python3 plot.py results.log
