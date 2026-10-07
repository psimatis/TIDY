# Sourced by the synthetic stream generators.
#
# stream_ok STREAM KEY=VALUE...: true if STREAM was generated completely with
# the given metadata.json values. The generator writes metadata.json only after
# the stream is fully written, so a missing or older metadata.json means the
# generation was interrupted (or overwritten halfway) and must be redone. Both
# files can share one mtime tick, hence "stream not newer" rather than "older".
stream_ok() {
    local stream="$1"
    shift
    local metadata
    metadata="$(dirname "$stream")/metadata.json"
    [[ -s "$stream" && -f "$metadata" && ! "$stream" -nt "$metadata" ]] || return 1
    [[ "$(tail -c 1 "$stream" | od -An -c | tr -d ' ')" == '\n' ]] || return 1
    python3 - "$metadata" "$@" <<'PY'
import json, sys
metadata = json.load(open(sys.argv[1]))
for pair in sys.argv[2:]:
    key, expected = pair.split("=", 1)
    actual = metadata.get(key)
    try:
        same = float(actual) == float(expected)
    except (TypeError, ValueError):
        same = str(actual) == expected
    if not same:
        print(f"  metadata {key}={actual}, expected {expected}", file=sys.stderr)
        sys.exit(1)
PY
}
