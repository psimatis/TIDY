#!/bin/bash
cd "$(dirname "$0")/../.."

make static_delta_all adaptive_all

g++ -O3 -mavx -std=c++17 -w -DWORKLOAD_COUNT -I. \
    indexes/containers/relation.o \
    indexes/containers/buffer.o \
    indexes/LIT/hierarchicalindex.o \
    indexes/LIT/live_index.o \
    indexes/LIT/hint_m_dynamic.o \
    experiments/learned/main.cpp \
    -o learned.exe

ORACLE_LOG="experiments/tidy_variants/results_oracle.log"
ADAPTIVE_LOG="experiments/tidy_variants/results_adaptive.log"
LEARNED_LOG="experiments/tidy_variants/results_learned.log"
> "$ORACLE_LOG"
> "$ADAPTIVE_LOG"
> "$LEARNED_LOG"

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

# Oracle delta values (99th percentile of durations)
declare -A DELTAS
DELTAS["TAXIS"]=2640
DELTAS["BIKES"]=5437
DELTAS["FLIGHTS"]=22500
DELTAS["WEBKIT"]=107624274
DELTAS["WILDFIRES"]=2318782

# Adaptive best setting
ADAPTIVE_FRACTION="0.01"

# Learned best setting
LEARNED_FRACTION="0.01"
LEARNED_LR="100"
LEARNED_WARMUP="10000"

for dataset in "${DATASETS[@]}"; do
    for ext in "${EXTENTS[@]}"; do
        if [ "$ext" = "snapshot" ]; then
            stream="data/stream/domain_extent_uniform/${dataset}/${dataset}_stream_dom0p0001.stream"
        else
            stream="data/stream/domain_extent_uniform/${dataset}/${dataset}_stream_${ext}.stream"
        fi

        if [ ! -f "$stream" ]; then
            echo "Stream not found: $stream" | tee -a "$ORACLE_LOG"
            continue
        fi

        if [ "$ext" = "snapshot" ]; then
            STAB_FLAG="-s"
        else
            STAB_FLAG=""
        fi

        delta=${DELTAS[$dataset]}

        # --- Oracle ---
        echo "========================================" | tee -a "$ORACLE_LOG"
        echo "Dataset: $dataset" | tee -a "$ORACLE_LOG"
        echo "Extent: $ext" | tee -a "$ORACLE_LOG"
        echo "========================================" | tee -a "$ORACLE_LOG"
        echo "--- static_delta.exe (Oracle, delta=$delta) ---" | tee -a "$ORACLE_LOG"
        ./static_delta.exe $STAB_FLAG -d "$delta" "$stream" 2>&1 | tee -a "$ORACLE_LOG"
        echo "" | tee -a "$ORACLE_LOG"

        # --- Adaptive ---
        echo "========================================" | tee -a "$ADAPTIVE_LOG"
        echo "Dataset: $dataset" | tee -a "$ADAPTIVE_LOG"
        echo "Extent: $ext" | tee -a "$ADAPTIVE_LOG"
        echo "========================================" | tee -a "$ADAPTIVE_LOG"
        echo "--- adaptive.exe (fraction=$ADAPTIVE_FRACTION) ---" | tee -a "$ADAPTIVE_LOG"
        ./adaptive.exe $STAB_FLAG -f "$ADAPTIVE_FRACTION" "$stream" 2>&1 | tee -a "$ADAPTIVE_LOG"
        echo "" | tee -a "$ADAPTIVE_LOG"

        # --- Learned ---
        echo "========================================" | tee -a "$LEARNED_LOG"
        echo "Dataset: $dataset" | tee -a "$LEARNED_LOG"
        echo "Extent: $ext" | tee -a "$LEARNED_LOG"
        echo "========================================" | tee -a "$LEARNED_LOG"
        echo "--- learned.exe (M=$LEARNED_WARMUP, lr=$LEARNED_LR) ---" | tee -a "$LEARNED_LOG"
        ./learned.exe $STAB_FLAG -f "$LEARNED_FRACTION" -l "$LEARNED_LR" -m "$LEARNED_WARMUP" "$stream" 2>&1 | tee -a "$LEARNED_LOG"
        echo "" | tee -a "$LEARNED_LOG"
    done
done

echo "All experiments completed!"

cd experiments/tidy_variants
python3 verify.py results_oracle.log results_adaptive.log results_learned.log
python3 plot.py results_oracle.log results_adaptive.log results_learned.log
