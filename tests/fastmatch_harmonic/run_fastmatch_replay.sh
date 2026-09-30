#!/bin/sh
set -eu
APP=${1:?app required}
IMAGE=${2:?external fixture required}
OUT=${3:?external fresh output required}
EXPECTED=${4:-}
case "$OUT" in /*) ;; *) exit 64;; esac
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
OUT=$(realpath -m "$OUT")
case "$OUT/" in "$ROOT/"*) exit 64;; esac
if [ -n "$EXPECTED" ]; then sha256sum -c "$EXPECTED";fi
test ! -e "$OUT"
mkdir -p "$OUT"
cd "$ROOT"
for name in headless_fastmatch_baseline headless_fastmatch_audit; do
 "$APP" --cxscript-headless --image "$IMAGE" --script "tests/fastmatch_harmonic/$name.cxsc" \
  --out "$OUT/$name" --case-name "$name" --roi-x1 320 --roi-y1 240 \
  --timeout-sec 60 --max-elapsed-ms 60000 --unified-log "$OUT/$name.jsonl" > "$OUT/$name.log" 2>&1
done
for pair in headless_fastmatch_changed:HARMONIC_FASTMATCH_CHANGED headless_fastmatch_empty:HARMONIC_FASTMATCH_BASELINE_NOT_READY; do
 name=${pair%%:*}
 reason=${pair#*:}
 if "$APP" --cxscript-headless --image "$IMAGE" --script "tests/fastmatch_harmonic/$name.cxsc" --out "$OUT/$name" --unified-log "$OUT/$name.jsonl" > "$OUT/$name.log" 2>&1; then
  echo "FAIL expected rejection: $name" >&2;exit 1
 fi
 grep -q "$reason" "$OUT/$name.log"
done
grep -q '"assertions_passed":4' "$OUT/headless_fastmatch_audit/harmonic_audit_receipt.json"
for asset in result_overlay.png evidence_overlay.png tool_display.png shape_model.json; do
 cmp "$OUT/headless_fastmatch_baseline/$asset" "$OUT/headless_fastmatch_audit/$asset"
done
for name in headless_fastmatch_baseline headless_fastmatch_audit; do
 sed -E 's/"elapsed_ms":[[:space:]]*[0-9]+/"elapsed_ms":0/g' "$OUT/$name/object_state.json" > "$OUT/$name/object_state_without_timing.json"
done
cmp "$OUT/headless_fastmatch_baseline/object_state_without_timing.json" "$OUT/headless_fastmatch_audit/object_state_without_timing.json"
sha256sum "$APP" "$IMAGE" tests/fastmatch_harmonic/headless_fastmatch*.cxsc \
 "$OUT/headless_fastmatch_baseline/shape_model.json" \
 "$OUT/headless_fastmatch_audit/harmonic_audit_receipt.json" > "$OUT/reference_bundle.sha256"
sha256sum -c "$OUT/reference_bundle.sha256" > "$OUT/sha_verification.log"
sed '1s/^[0-9a-f]*/0000000000000000000000000000000000000000000000000000000000000000/' "$OUT/reference_bundle.sha256" > "$OUT/corrupt_reference_bundle.sha256"
if sh "$ROOT/tests/fastmatch_harmonic/run_fastmatch_replay.sh" "$APP" "$IMAGE" "$OUT/blocked_by_sha" "$OUT/corrupt_reference_bundle.sha256" > "$OUT/sha_rejection.log" 2>&1; then
 echo "FAIL corrupt SHA accepted" >&2;exit 1
fi
test ! -e "$OUT/blocked_by_sha"
grep -q FAILED "$OUT/sha_rejection.log"
printf '%s\n' '{"schema":"cxvision.fastmatch_reference_bundle.v1","status":"AUDIT_REPLAY_PASS","asset_role":"reference_snapshot_only","version":1,"live_baseline_required":true,"same_instance_unchanged":true,"production_eligible":false}' > "$OUT/reference_bundle_manifest.json"
printf '%s\n' FASTMATCH_FORMFIT_AUDIT_PASS
