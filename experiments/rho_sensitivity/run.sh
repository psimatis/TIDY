#!/bin/bash
cd "$(dirname "$0")/../.."

# Build object files
make indexes/containers/relation.o indexes/containers/buffer.o indexes/LIT/hierarchicalindex.o indexes/LIT/live_index.o indexes/LIT/hint_m_dynamic.o

# Compile
g++ -O3 -mavx -std=c++17 -w -DWORKLOAD_COUNT -I. \
    indexes/containers/relation.o \
    indexes/containers/buffer.o \
    indexes/LIT/hierarchicalindex.o \
    indexes/LIT/live_index.o \
    indexes/LIT/hint_m_dynamic.o \
    experiments/rho_sensitivity/main.cpp \
    -o experiments/rho_sensitivity/rho_sensitivity.exe

LOGFILE="experiments/rho_sensitivity/results.log"
> "$LOGFILE"

# Table 8 is reported on TAXIS, 0.1% domain extent.
EXTENTS=(
    "dom0p001"
)

DATASETS=(
    "TAXIS"
)

RHO_VALUES=(
    "0.001"
    "0.01"
    "0.1"
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

        for rho in "${RHO_VALUES[@]}"; do
            echo "--- rho_sensitivity.exe (rho=$rho) ---" | tee -a "$LOGFILE"
            ./experiments/rho_sensitivity/rho_sensitivity.exe $STAB_FLAG -r "$rho" "$stream" 2>&1 | tee -a "$LOGFILE"
        done

        echo "" | tee -a "$LOGFILE"
    done
done

echo "All experiments completed!" | tee -a "$LOGFILE"

cd experiments/rho_sensitivity
python3 plot.py results.log
