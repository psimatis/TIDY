#!/bin/bash
cd "$(dirname "$0")/../.."

# Build object files
make indexes/containers/relation.o indexes/containers/buffer.o

LOGFILE="experiments/brail_scalability/results.log"
> "$LOGFILE"

EXTENTS=(
    "dom0p001"
)

DATASETS=(
    "TAXIS"
)

g++ -O3 -mavx -std=c++17 -w -I. \
    indexes/containers/relation.o \
    indexes/containers/buffer.o \
    experiments/brail_scalability/main.cpp \
    -o experiments/brail_scalability/brail_scalability.exe

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

        ./experiments/brail_scalability/brail_scalability.exe "$stream" 2>&1 | tee -a "$LOGFILE"

        echo "" | tee -a "$LOGFILE"
    done
done

echo "All experiments completed!" | tee -a "$LOGFILE"

cd experiments/brail_scalability
python3 plot.py results.log
