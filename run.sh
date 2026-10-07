#!/usr/bin/env bash
# Reproduces the experiments of the paper:
# build -> generate query/update streams -> run experiments -> plot.
#
# By default this builds a Docker image and runs itself inside it, so the only requirement is Docker. 
# Set NATIVE=1 to run directly on the host instead (see Requirements in README.md for what that needs).
#
# Usage: ./run.sh [all|real|synthetic|<experiment> ...]
#   real       every experiment on the five real datasets
#   synthetic  the 1B-interval experiments (Table 10, Figure 11)
#   all        both (default)
#   <experiment> any of the names listed in REAL_EXPERIMENTS / SYNTHETIC_EXPERIMENTS
#
# Environment:
#   SKIP_CACHE=1   skip the hardware-counter experiment (Figure 10)
#   NATIVE=1       run directly on the host instead of spinning up Docker
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"

# By default this script builds and runs itself inside Docker, so you have to type "./run.sh" with no setup decisions. 
# /.dockerenv marks that we're already inside the container (avoids re-entering Docker from Docker); 
# NATIVE=1 opts out and runs on the host instead.
if [[ ! -f /.dockerenv && "${NATIVE:-0}" != "1" ]]; then
    if ! command -v docker >/dev/null 2>&1; then
        echo "Docker not found; running natively instead (set NATIVE=1 to silence this)." >&2
    else
        echo "Building Docker image ..."
        docker build -t tidy "$ROOT"
        exec docker run --rm \
            --user "$(id -u):$(id -g)" \
            -v "$ROOT":/tidy \
            --cap-add PERFMON --security-opt seccomp=unconfined \
            -e SKIP_CACHE="${SKIP_CACHE:-0}" \
            tidy "$@"
    fi
fi

# Native runs use a local venv; the Docker image already has the Python packages.
# The venv is rebuilt if it is broken, e.g. one left behind by an older Docker run.
if [[ ! -f /.dockerenv ]]; then
    VENV="$ROOT/.venv"
    if ! "$VENV/bin/python3" -c "import numpy, pandas, matplotlib" >/dev/null 2>&1; then
        echo "Creating $VENV and installing requirements.txt ..."
        python3 -m venv --clear "$VENV"
        "$VENV/bin/python3" -m pip install --quiet -r "$ROOT/requirements.txt"
    fi
    export PATH="$VENV/bin:$PATH"
fi

DATASETS=(
    "TAXIS"
    "BIKES"
    "FLIGHTS"
    "WEBKIT"
    "WILDFIRES"
)

# RAM (GB) needed per real dataset: measured peaks (TAXIS ~10 GB) plus headroom.
declare -A DATASET_RAM_GB=([TAXIS]=16 [FLIGHTS]=8 [BIKES]=4 [WEBKIT]=2 [WILDFIRES]=2)
SYNTHETIC_RAM_GB=64
RAM_GB=$(( $(awk '/^MemTotal:/ {print $2}' /proc/meminfo) / 1024 / 1024 ))
SKIPPED=()

# Order matters: competitors plots B-Rail from the brail_variants log.
REAL_EXPERIMENTS=(
    live_vs_dead          # Table 3
    brail_variants        # Figure 6, Table 4
    capacity_sensitivity  # Table 5
    brail_scalability     # Table 6
    btree_benchmark       # Figure 7, Table 7
    tidy_variants         # Figure 8
    rho_sensitivity       # Table 8
    competitors           # Figure 9, Table 9
    cache_misses          # Figure 10
    dataset_analysis      # Figure 2, Table 2
)
SYNTHETIC_EXPERIMENTS=(
    te_skew               # Table 10
    poisson_duration      # Figure 11
)

banner() {
    echo
    echo "############################################################"
    echo "# $* ($(date '+%Y-%m-%d %H:%M:%S'))"
    echo "############################################################"
}

ZENODO_RECORD="23094082"

download_dataset() {
    local dataset="$1"
    local url="https://zenodo.org/records/$ZENODO_RECORD/files/$dataset.dat.gz?download=1"
    echo "data/static/$dataset.dat is missing; downloading from Zenodo ..." >&2
    if curl -fSL -o "data/static/$dataset.dat.gz" "$url" 2>/dev/null \
        && gunzip -f "data/static/$dataset.dat.gz"; then
        echo "Downloaded data/static/$dataset.dat" >&2
    else
        rm -f "data/static/$dataset.dat.gz"
        echo "WARNING: could not download $dataset.dat from Zenodo; experiments will skip it" >&2
        echo "  Get it manually from https://doi.org/10.5281/zenodo.$ZENODO_RECORD (see README.md)" >&2
    fi
}

check_raw_data() {
    local found=0 fitting=()
    for dataset in "${DATASETS[@]}"; do
        if (( RAM_GB < DATASET_RAM_GB[$dataset] )); then
            echo "WARNING: $dataset needs ~${DATASET_RAM_GB[$dataset]} GB RAM, this machine has ${RAM_GB} GB; skipping $dataset." >&2
            SKIPPED+=("$dataset (needs ~${DATASET_RAM_GB[$dataset]} GB RAM)")
        else
            fitting+=("$dataset")
        fi
    done
    DATASETS=("${fitting[@]}")
    for dataset in "${DATASETS[@]}"; do
        if [[ ! -f "data/static/$dataset.dat" ]]; then
            download_dataset "$dataset"
        fi
        if [[ -f "data/static/$dataset.dat" ]]; then
            found=1
        fi
    done
    if [[ "$found" == "0" ]]; then
        echo "No real dataset found in data/static/ (see README.md)." >&2
        exit 1
    fi
}

generate_real_streams() {
    banner "Generating query/update streams"
    python3 data/stream/generate_streams.py "${DATASETS[@]}"
}

run_experiment() {
    local name="$1"
    banner "Experiment: $name"
    case "$name" in
        dataset_analysis)
            (cd data/static && python3 dataset_analysis.py)
            ;;
        competitors)
            experiments/competitors/run_learned.sh
            experiments/competitors/run.sh
            ;;
        cache_misses)
            local paranoid
            paranoid="$(cat /proc/sys/kernel/perf_event_paranoid 2>/dev/null || echo 4)"
            if [[ "${SKIP_CACHE:-0}" == "1" ]]; then
                echo "SKIP_CACHE=1; skipping"
            elif (( paranoid > 1 )); then
                echo "kernel.perf_event_paranoid=$paranoid; hardware counters are not readable." >&2
                echo "SKIPPING cache_misses (Figure 10). To include it, run 'sudo sysctl -w kernel.perf_event_paranoid=1' on the host and rerun './run.sh cache_misses'." >&2
            else
                experiments/cache_misses/run.sh
            fi
            ;;
        te_skew)
            data/stream/synthetic/generate_te_skew_streams.sh all
            experiments/synthetic/te_skew/run.sh
            ;;
        poisson_duration)
            data/stream/synthetic/generate_poisson_1b.sh
            experiments/synthetic/poisson_duration/run.sh
            ;;
        *)
            experiments/"$name"/run.sh
            ;;
    esac
}

is_real() {
    local name="$1" experiment
    for experiment in "${REAL_EXPERIMENTS[@]}"; do
        [[ "$experiment" == "$name" ]] && return 0
    done
    return 1
}

TARGETS=("$@")
[[ ${#TARGETS[@]} -eq 0 ]] && TARGETS=(all)

SELECTED=()
for target in "${TARGETS[@]}"; do
    case "$target" in
        all)       SELECTED+=("${REAL_EXPERIMENTS[@]}" "${SYNTHETIC_EXPERIMENTS[@]}") ;;
        real)      SELECTED+=("${REAL_EXPERIMENTS[@]}") ;;
        synthetic) SELECTED+=("${SYNTHETIC_EXPERIMENTS[@]}") ;;
        *)
            if ! is_real "$target" && [[ " ${SYNTHETIC_EXPERIMENTS[*]} " != *" $target "* ]]; then
                echo "Unknown experiment: $target" >&2
                exit 1
            fi
            SELECTED+=("$target")
            ;;
    esac
done

banner "Building"
make -j"$(nproc)" all

for name in "${SELECTED[@]}"; do
    if is_real "$name"; then
        check_raw_data
        generate_real_streams
        break
    fi
done

for name in "${SELECTED[@]}"; do
    if ! is_real "$name" && (( RAM_GB < SYNTHETIC_RAM_GB )); then
        echo "WARNING: $name needs ~${SYNTHETIC_RAM_GB} GB RAM, this machine has ${RAM_GB} GB; skipping $name." >&2
        SKIPPED+=("$name (needs ~${SYNTHETIC_RAM_GB} GB RAM)")
        continue
    fi
    run_experiment "$name"
done

banner "Done. Figures (PDF) and tables (CSV) are in each experiment's own folder."
if (( ${#SKIPPED[@]} > 0 )); then
    echo "Skipped for lack of RAM (their results are missing from the figures and tables):" >&2
    printf '  %s\n' "${SKIPPED[@]}" >&2
fi
