#!/bin/bash
cd "$(dirname "$0")/../.."

FLAGS="-O3 -mavx -std=c++17 -w -I. -Iindexes/bplustrees"
DIR="experiments/btree_benchmark"

g++ $FLAGS $DIR/main_tlx.cpp       -o $DIR/btree_tlx.exe   || exit 1
g++ $FLAGS $DIR/main_qit_quit.cpp  -o $DIR/btree_quit.exe  || exit 1
g++ $FLAGS $DIR/main_brailtree.cpp -o $DIR/brail.exe        || exit 1

LOGFILE="$DIR/results.log"
> "$LOGFILE"

DATASETS=(
    "TAXIS"
    "BIKES"
    "FLIGHTS"
    "WEBKIT"
    "WILDFIRES"
)

EXTENTS=(
    "snapshot"
    "dom0p0001"
    "dom0p001"
    "dom0p01"
)

METHODS=(
    "$DIR/btree_tlx.exe"
    "$DIR/btree_quit.exe"
    "$DIR/brail.exe"
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

        for method in "${METHODS[@]}"; do
            name=$(basename "$method")
            echo "--- $name ---" | tee -a "$LOGFILE"
            ./$method $STAB_FLAG "$stream" 2>&1 | tee -a "$LOGFILE"
        done

        echo "" | tee -a "$LOGFILE"
    done
done

echo "All experiments completed!" | tee -a "$LOGFILE"
cd "$DIR"
python3 verify.py results.log
python3 plot.py results.log
