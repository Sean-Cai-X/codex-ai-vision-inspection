#!/bin/sh
# Real application + cxscript, no Python oracle. Use the generated rectangle fixture only.
set -eu
APP=${1:?application required}
IMAGE=${2:?rectangle fixture required}
OUT=${3:?fresh external output required}
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
case "$OUT" in /*) ;; *) exit 64;; esac
OUT=$(realpath -m -- "$OUT")
case "$OUT/" in "$ROOT/"*) exit 64;; esac
test ! -e "$OUT"
mkdir -p "$OUT"
cd "$ROOT"
run() {
 name=$1
 script=$2
 "$APP" --cxscript-headless --image "$IMAGE" --script "$script" --out "$OUT/$name" \
 --case-name "$name" --roi-x0 0 --roi-y0 0 --roi-x1 320 --roi-y1 240 \
 --max-steps 20000 --timeout-sec 60 --max-elapsed-ms 60000 \
 --unified-log "$OUT/$name.jsonl" > "$OUT/$name.log" 2>&1
}
run baseline tests/fastmatch_harmonic/headless_image_baseline.cxsc
run roundtrip tests/fastmatch_harmonic/headless_asset_roundtrip.cxsc
ASSET="$OUT/roundtrip/harmonic_reference.so2"
SHA=$(sha256sum "$ASSET")
SHA=${SHA%% *}
sed "s/TRUSTED_ASSET_SHA/$SHA/" tests/fastmatch_harmonic/headless_asset_reload.cxsc > "$OUT/reload.cxsc"
mkdir -p "$OUT/reload" "$OUT/corrupt"
cp "$ASSET" "$OUT/reload/harmonic_reference.so2"
cp "$ASSET" "$OUT/corrupt/harmonic_reference.so2"
printf 'corrupted' >> "$OUT/corrupt/harmonic_reference.so2"
run reload "$OUT/reload.cxsc"
if run corrupt "$OUT/reload.cxsc"; then echo "FAIL corrupt accepted";exit 1;fi
grep -q ASSET_SHA_MISMATCH "$OUT/corrupt.log"
if run incompatible tests/fastmatch_harmonic/headless_asset_incompatible.cxsc; then echo "FAIL config accepted";exit 1;fi
grep -q INCOMPATIBLE_CONFIG "$OUT/incompatible.log"
for name in roundtrip reload; do
 grep -q '"assertions_passed":3' "$OUT/$name/harmonic_audit_receipt.json"
 grep -q '"operation":"load"' "$OUT/$name/harmonic_audit_receipt.json"
 for asset in result_overlay.png evidence_overlay.png tool_display.png object_state.json measurement_observations.json; do
  cmp "$OUT/baseline/$asset" "$OUT/$name/$asset"
 done
done
sha256sum "$APP" "$IMAGE" "$ASSET" tests/fastmatch_harmonic/headless_asset*.cxsc "$OUT/reload.cxsc" > "$OUT/replay_sha256.txt"
printf '%s\n' '{"schema":"cxvision.so2_asset_headless_suite.v1","status":"PASS","separate_process_reload":true,"corruption_rejected":true,"incompatible_config_rejected":true,"image_pipeline_unchanged":true,"production_eligible":false}' > "$OUT/suite_receipt.json"
echo HEADLESS_REFERENCE_ASSET_PASS
