#!/bin/sh
# Actual application/cxscript adapter replay. Geometry fixtures are native-generated.
set -eu
APP=${1:?application required}
FIXTURES=${2:?native-generated fixtures required}
IMAGE=${3:?external image required by application runner}
OUT=${4:?fresh external replay output required}
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
case "$OUT" in /*) ;; *) exit 64;; esac
OUT=$(realpath -m "$OUT")
case "$OUT/" in "$ROOT/"*) exit 64;; esac
test ! -e "$OUT"
mkdir -p "$OUT"
for name in success budget stale unknown overwrite auto mixed; do
 rc=0
 "$APP" --cxscript-headless --image "$IMAGE" --script "$FIXTURES/$name.cxsc" --out "$OUT/$name" \
  --case-name FindSetMatch --timeout-sec 30 --max-elapsed-ms 30000 --unified-log "$OUT/$name.jsonl" > "$OUT/$name.log" 2>&1 || rc=$?
 case "$name" in
  success) test "$rc" -eq 0;test -s "$FIXTURES/script_receipt.json";cmp "$FIXTURES/script_receipt.json" "$OUT/success/geometric_set_receipt_0.json";sha256sum "$FIXTURES/script_receipt.json" > "$OUT/receipt.sha256";;
  budget) test "$rc" -eq 0;grep -q BUDGET_EXHAUSTED "$FIXTURES/budget_receipt.json";cmp "$FIXTURES/budget_receipt.json" "$OUT/budget/geometric_set_receipt_0.json";;
  stale) test "$rc" -ne 0;grep -q SETMATCH_RESULT_NOT_READY "$OUT/$name.log";test ! -e "$FIXTURES/stale_receipt.json";;
  unknown) test "$rc" -ne 0;grep -q SETMATCH_UNKNOWN_NUMERIC_PARAMETER "$OUT/$name.log";;
  auto) test "$rc" -eq 0;test -s "$OUT/auto/geometric_set_receipt_0.json";;
  mixed) test "$rc" -ne 0;grep -q '"geometric_set_only": "false"' "$OUT/mixed/result_summary.json";;
  overwrite) test "$rc" -ne 0;grep -q SETMATCH_OUTPUT_EXISTS "$OUT/$name.log";sha256sum -c "$OUT/receipt.sha256";;
 esac
 printf '%s PASS\n' "$name" >> "$OUT/cases.txt"
done
for name in setmatch_asymmetric_mixed setmatch_symmetric_square setmatch_budget_stop; do
 "$APP" --cxscript-headless --image "$FIXTURES/evidence/$name/source_image.pgm" \
  --script "$FIXTURES/evidence/$name/run.cxsc" --out "$OUT/$name" \
  --case-name FindSetMatch --timeout-sec 30 --max-elapsed-ms 30000 \
  --unified-log "$OUT/$name.jsonl" > "$OUT/$name.log" 2>&1
 cmp "$OUT/$name/setmatch_receipt.json" "$OUT/$name/geometric_set_receipt_0.json"
 case "$name" in
  setmatch_budget_stop) grep -q '"execution_status": "BUDGET_EXHAUSTED"' "$OUT/$name/setmatch_receipt.json";;
  *) grep -q '"execution_status": "COMPLETED"' "$OUT/$name/setmatch_receipt.json";;
 esac
 printf '%s PASS\n' "$name" >> "$OUT/cases.txt"
done
sha256sum "$APP" "$FIXTURES/request.json" "$FIXTURES/"*.cxsc "$FIXTURES/"*receipt.json > "$OUT/replay.sha256"
printf '%s\n' '{"schema":"cxvision.geometric_set_adapter_replay.v1","status":"PASS","cases":10,"real_application":true,"image_extraction_performed":false,"production_eligible":false}' > "$OUT/receipt.json"
echo HEADLESS_SET_MATCH_ADAPTER_PASS
