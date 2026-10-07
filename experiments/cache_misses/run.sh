#!/bin/bash
# Figure 10: L1/L2/L3 load misses per insert and per query, measured with
# hardware counters on a single performance core, with wall-clock time
# recorded alongside. Only loads are counted.
#
# Indexes: TIDY (learned), HINT, R-tree.
#
# Requires readable hardware counters (the setting resets on reboot):
#   sudo sysctl -w kernel.perf_event_paranoid=1

cd "$(dirname "$0")/../.."

make indexes/containers/relation.o indexes/containers/buffer.o \
     indexes/LIT/hierarchicalindex.o indexes/LIT/live_index.o indexes/LIT/hint_m_dynamic.o

OBJ="indexes/containers/relation.o indexes/containers/buffer.o indexes/LIT/hierarchicalindex.o indexes/LIT/live_index.o indexes/LIT/hint_m_dynamic.o"
FLAGS="-O3 -mavx -std=c++17 -w -DWORKLOAD_COUNT -I."
for idx in learned hint rtree; do
    g++ $FLAGS $OBJ experiments/cache_misses/main_${idx}.cpp \
        -o experiments/cache_misses/cache_${idx}.exe || exit 1
done

LOGFILE="experiments/cache_misses/results.log"
CSVFILE="experiments/cache_misses/results.csv"
> "$LOGFILE"

# Fresh CSV with a header row (the mains only append data rows).
echo "Dataset,Extent,Index,Phase,Ops,TimeSec,L1Miss,L2Miss,L3Miss,Instructions,Cycles,BranchMisses" > "$CSVFILE"

EXTENTS=(
    "dom0p001"
)

# Figure 10 shows TAXIS, WILDFIRES and WEBKIT.
DATASETS=(
    "TAXIS"
    "WILDFIRES"
    "WEBKIT"
)

FRACTION="0.1"
LEARNING_RATE="100"
WARMUP="10000"

# Pin to CPU 0 (a P-core) in addition to the sched_setaffinity call in each binary.
PIN="taskset -c 0"

for dataset in "${DATASETS[@]}"; do
    for ext in "${EXTENTS[@]}"; do
        stream="data/stream/domain_extent_uniform/${dataset}/${dataset}_stream_${ext}.stream"

        if [ ! -f "$stream" ]; then
            echo "Stream not found: $stream" | tee -a "$LOGFILE"
            continue
        fi

        echo "========================================" | tee -a "$LOGFILE"
        echo "Dataset: $dataset   Extent: $ext" | tee -a "$LOGFILE"
        echo "========================================" | tee -a "$LOGFILE"

        $PIN ./experiments/cache_misses/cache_learned.exe \
            -f "$FRACTION" -l "$LEARNING_RATE" -m "$WARMUP" \
            -D "$dataset" -E "$ext" "$stream" 2>&1 | tee -a "$LOGFILE"

        $PIN ./experiments/cache_misses/cache_hint.exe \
            -D "$dataset" -E "$ext" "$stream" 2>&1 | tee -a "$LOGFILE"

        $PIN ./experiments/cache_misses/cache_rtree.exe \
            -D "$dataset" -E "$ext" "$stream" 2>&1 | tee -a "$LOGFILE"

        echo "" | tee -a "$LOGFILE"
    done
done

echo "All experiments completed!" | tee -a "$LOGFILE"

cd experiments/cache_misses
python3 plot.py results.csv
