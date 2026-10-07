# Shared by te_skew/run.sh and poisson_duration/run.sh (sourced, not executed).
#
# Methods: rtree (R*-tree), hint (HINT), brail (B-Rail), tidy (TIDY-Lrn).
# Every method writes its own log, so re-running a subset of METHODS keeps the
# results of the others.

SYNTHETIC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SYNTHETIC_DIR/../.." && pwd)"
BIN_DIR="$SYNTHETIC_DIR"
BUILD_DIR="$SYNTHETIC_DIR/.build"

# TIDY-Lrn configuration, the same as in Figure 9.
OUTLIER_FRACTION="${OUTLIER_FRACTION:-0.1}"
WARMUP="${WARMUP:-10000}"
LEARNING_RATE="${LEARNING_RATE:-100}"

declare -A METHOD_SOURCE=(
    [rtree]="experiments/competitors/main_rtree.cpp"
    [hint]="experiments/competitors/main_LIT.cpp"
    [brail]="experiments/competitors/main_brail.cpp"
    [tidy]="experiments/learned/main.cpp"
)

# build_methods METHOD...
build_methods() {
    local CXX="${CXX:-g++}"
    local sources=(
        indexes/containers/relation.cpp
        indexes/containers/buffer.cpp
        indexes/LIT/hierarchicalindex.cpp
        indexes/LIT/live_index.cpp
        indexes/LIT/hint_m_dynamic.cpp
    )
    local objects=() source object method
    mkdir -p "$BUILD_DIR"
    for source in "${sources[@]}"; do
        object="$BUILD_DIR/$(basename "${source%.cpp}").o"
        "$CXX" -O3 -mavx -std=c++14 -w -I"$ROOT" -c "$ROOT/$source" -o "$object"
        objects+=("$object")
    done
    for method in "$@"; do
        if [[ -z "${METHOD_SOURCE[$method]:-}" ]]; then
            echo "Unknown method: $method (expected rtree, hint, brail or tidy)" >&2
            exit 1
        fi
        "$CXX" -O3 -mavx -std=c++17 -w -DWORKLOAD_COUNT -I"$ROOT" "${objects[@]}" \
            "$ROOT/${METHOD_SOURCE[$method]}" -o "$BIN_DIR/$method.exe"
    done
}

# run_method METHOD STREAM LOGFILE: appends the method's report to LOGFILE.
run_method() {
    local method="$1" stream="$2" logfile="$3"
    echo "--- $method ---" | tee -a "$logfile"
    if [[ "$method" == "tidy" ]]; then
        "$BIN_DIR/tidy.exe" -f "$OUTLIER_FRACTION" -m "$WARMUP" -l "$LEARNING_RATE" \
            "$stream" 2>&1 | tee -a "$logfile"
    else
        "$BIN_DIR/$method.exe" "$stream" 2>&1 | tee -a "$logfile"
    fi
}
