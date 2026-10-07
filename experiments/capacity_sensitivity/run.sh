#!/bin/bash
cd "$(dirname "$0")/../.."

# Build object files
make indexes/containers/relation.o indexes/containers/buffer.o indexes/LIT/hierarchicalindex.o indexes/LIT/live_index.o indexes/LIT/hint_m_dynamic.o

LOGFILE="experiments/capacity_sensitivity/results.log"
> "$LOGFILE"

EXTENTS=(
    "dom0p001"
)

# Table 5 is reported on TAXIS only.
DATASETS=(
    "TAXIS"
)

CAPACITIES=(256 512 1024 2048 4096)

for dataset in "${DATASETS[@]}"; do
    for ext in "${EXTENTS[@]}"; do
        stream="data/stream/domain_extent_uniform/${dataset}/${dataset}_stream_${ext}.stream"

        if [ ! -f "$stream" ]; then
            echo "Stream not found: $stream" | tee -a "$LOGFILE"
            continue
        fi

        for capacity in "${CAPACITIES[@]}"; do
            echo "========================================" | tee -a "$LOGFILE"
            echo "Dataset: $dataset" | tee -a "$LOGFILE"
            echo "Extent: $ext" | tee -a "$LOGFILE"
            echo "Capacity: $capacity" | tee -a "$LOGFILE"
            echo "========================================" | tee -a "$LOGFILE"

            echo "Compiling with CAPACITY=$capacity..." | tee -a "$LOGFILE"
            g++ -O3 -mavx -std=c++17 -w -DCAPACITY=$capacity -I. \
                indexes/containers/relation.o indexes/containers/buffer.o \
                indexes/LIT/hierarchicalindex.o indexes/LIT/live_index.o indexes/LIT/hint_m_dynamic.o \
                experiments/capacity_sensitivity/main.cpp \
                -o experiments/capacity_sensitivity/capacity_sensitivity.exe 2>&1 | tee -a "$LOGFILE"

            ./experiments/capacity_sensitivity/capacity_sensitivity.exe "$stream" 2>&1 | tee -a "$LOGFILE"

            echo "" | tee -a "$LOGFILE"
        done
    done
done

echo "All experiments completed!" | tee -a "$LOGFILE"

cd experiments/capacity_sensitivity
python3 plot.py results.log
