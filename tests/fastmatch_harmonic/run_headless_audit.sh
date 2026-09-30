#!/bin/sh
# Run inside the application's configured offline runtime. No Python required.
set -eu
APP=${1:?application executable required}
IMAGE=${2:?external input image required}
OUT=${3:?external output directory required}
WIDTH=${4:-800}
HEIGHT=${5:-600}
case "$OUT" in /*) ;; *) echo "output must be an absolute external directory" >&2;exit 64;; esac
OUT=$(realpath -m -- "$OUT")
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
case "$OUT/" in "$ROOT/"*) echo "output must stay outside checkout" >&2;exit 64;; esac
if [ -e "$OUT" ]; then echo "use a fresh output directory to exclude stale receipts" >&2;exit 64;fi
mkdir -p "$OUT"
cd "$ROOT"
for name in headless_image_baseline headless_audit headless_object_source; do
  "$APP" --cxscript-headless --image "$IMAGE" \
    --script "tests/fastmatch_harmonic/$name.cxsc" --out "$OUT/$name" \
    --case-name "$name" --roi-x0 0 --roi-y0 0 --roi-x1 "$WIDTH" --roi-y1 "$HEIGHT" \
    --max-steps 20000 --timeout-sec 60 --max-elapsed-ms 60000 \
    --unified-log "$OUT/$name.jsonl" > "$OUT/$name.log" 2>&1
done
if "$APP" --cxscript-headless --image "$IMAGE" \
    --script tests/fastmatch_harmonic/headless_assertion_failure.cxsc \
    --out "$OUT/headless_assertion_failure" --roi-x1 "$WIDTH" --roi-y1 "$HEIGHT" \
    --unified-log "$OUT/headless_assertion_failure.jsonl" > "$OUT/headless_assertion_failure.log" 2>&1; then
  echo "FAIL: deliberately false assertion was accepted" >&2;exit 1
fi
grep -q HARMONIC_STATUS_ASSERTION_FAILED "$OUT/headless_assertion_failure.log"
test -s "$OUT/headless_audit/harmonic_audit_receipt.json"
test -s "$OUT/headless_object_source/harmonic_audit_receipt.json"
grep -q '"assertions_passed":160' "$OUT/headless_audit/harmonic_audit_receipt.json"
grep -q UNVERIFIED_TOPOLOGY_LEGACY_FALLBACK "$OUT/headless_object_source/harmonic_audit_receipt.json"
for name in headless_audit headless_object_source; do
  for asset in result_overlay.png evidence_overlay.png tool_display.png object_state.json measurement_observations.json; do
    cmp "$OUT/headless_image_baseline/$asset" "$OUT/$name/$asset"
  done
done
grep -q '"assertions_passed":2' "$OUT/headless_object_source/harmonic_audit_receipt.json"
sha256sum "$APP" "$IMAGE" tests/fastmatch_harmonic/headless*.cxsc \
  "$OUT/headless_audit/harmonic_audit_receipt.json" \
  "$OUT/headless_object_source/harmonic_audit_receipt.json" > "$OUT/replay_sha256.txt"
printf '%s\n' '{"schema":"cxvision.harmonic_headless_suite.v1","status":"PASS","audit_runs":56,"audit_assertions":160,"real_source_assertions":2,"expected_failure_rejected":true,"findobject_artifacts_unchanged":true,"production_eligible":false}' > "$OUT/suite_receipt.json"
printf '%s\n' 'HEADLESS_CXSCRIPT_AUDIT_PASS'
